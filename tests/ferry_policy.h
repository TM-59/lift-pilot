#ifndef FERRY_POLICY_H
#define FERRY_POLICY_H

/* Shared by the story test and the story bench: seeds and a human-like pilot. */
#include "../src/ferry.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static inline uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* Seed and index are stirred separately before they are combined (no XOR or sums of raw keys). */
static inline uint64_t sub_seed(uint64_t seed, uint64_t index, uint64_t salt)
{
    return mix64(mix64(seed + salt * 0x632be59bd9b4e019ULL) + (index + 1u) * 0x9e3779b97f4a7c15ULL);
}

typedef struct { uint64_t s; } Rng;
static inline double rng_unit(Rng *r)
{
    r->s += 0x9e3779b97f4a7c15ULL;
    return (double)(mix64(r->s) >> 11) / 9007199254740992.0;
}

/* A pilot who reads the instruments on screen, not the numbers in the code: the "glide needed" gauge (distance to the
 * middle of the strip over height), the height, the battery and whether the strip is under the nose. It glances at
 * the gauge every 0.2-0.6 s. Too low: motor on at the best-glide angle until the gauge looks comfortable (or the
 * battery is flat). Too high: nose down to a faster speed to lose height. Otherwise it holds a cruise speed on the
 * airspeed indicator with small stick corrections. Near the ground it raises the nose to flare. Each hop (and each
 * relaunch after an outlanding) gets a fresh hand: its own speeds, flare height and thresholds. */
#define BEST_SPEED 24.0

typedef struct {
    Rng rng;
    uint64_t key;
    int hop, outlandings, wait, climbing, diving, flaring;
    double cruise, dive, flare_h, flare_sink, motor_on, climb_by, climb_to, high, read, made, cmd, rate, gain;
    int glance;
} Pilot;

static inline void pilot_plan(Pilot *p)
{
    p->cruise = 24.0 + 6.0 * rng_unit(&p->rng);     /* m/s held on the airspeed indicator; best glide is near 23.5 */
    p->dive = 33.0 + 5.0 * rng_unit(&p->rng);       /* m/s, to lose height */
    p->flare_h = 3.0 + 7.0 * rng_unit(&p->rng);     /* m */
    p->flare_sink = 0.4 + 1.0 * rng_unit(&p->rng);  /* m/s of descent held in the round-out */
    p->rate = 2.0 + 3.0 * rng_unit(&p->rng);        /* deg/s: how fast the hand moves the stick */
    p->gain = 0.1 + 0.3 * rng_unit(&p->rng);        /* deg of pitch per m/s of airspeed error */
    p->motor_on = 0.80 + 0.20 * rng_unit(&p->rng);  /* glide made over glide needed below which the motor goes on */
    p->climb_by = 2.0 + 4.0 * rng_unit(&p->rng);    /* then it climbs until the glide needed is this much lower */
    p->high = 1.20 + 0.30 * rng_unit(&p->rng);      /* and above which the nose goes down to lose height */
    p->glance = 20 + (int)(40.0 * rng_unit(&p->rng)); /* frames between glances at the gauge */
    p->wait = 0;
    p->climbing = p->diving = p->flaring = 0;
    p->read = p->made = 0.0;
    p->cmd = 4.0;
}

/* Angle of attack that holds speed v in a steady glide, from the lift the pilot knows the wing gives. */
static inline double trim_alpha_deg(const Ferry *f, double v)
{
    const Aircraft *a = &f->aircraft;
    double cl = 2.0 * a->mass * GRAVITY / (a->rho * a->area * v * v);
    return (cl - a->cl0) / a->cl_alpha * 57.29577951308232;
}

/* Fly a speed the way a pilot does: hold a pitch attitude (nose on the horizon) for that speed, nudged by the airspeed
 * error. Holding attitude rather than angle of attack is what damps the phugoid. */
static inline double hold_speed(const Pilot *p, const Ferry *f, double v)
{
    double a = trim_alpha_deg(f, v), r = a / 57.29577951308232;
    const Aircraft *ac = &f->aircraft;
    double cl = ac->cl0 + ac->cl_alpha * r, cd = ac->cd0 + ac->k * cl * cl;
    double theta = a - atan2(cd, cl) * 57.29577951308232 + p->gain * (f->state.v - v);
    double cmd = theta - f->state.gamma * 57.29577951308232;
    return fmax(-2.0, fmin(12.0, cmd));
}

static inline void pilot_init(Pilot *p, uint64_t key)
{
    memset(p, 0, sizeof *p);
    p->key = key;
    p->hop = -1;
    p->rng.s = key;
}

static inline double pilot_target(Pilot *p, const Ferry *f, double *throttle)
{
    /* Each hop gets its own stirred stream, so a choice changes the hops it touches and not, by chance, every later one. */
    if (f->hop != p->hop || f->outlandings != p->outlandings) {
        if (f->hop != p->hop) p->rng.s = sub_seed(p->key, (uint64_t)f->hop, 5);
        p->hop = f->hop;
        p->outlandings = f->outlandings;
        pilot_plan(p);
    }
    *throttle = 0.0;
    double h = f->state.h;
    /* Round out earlier when coming down faster; once begun, the round-out lasts to the ground. It holds a gentle
     * descent (0.4-1.4 m/s) on the speed it has, rather than pulling the nose up, so the glider does not balloon. */
    double sink = -f->state.v * sin(f->state.gamma);
    if (p->flaring || h < p->flare_h + 1.5 * fmin(4.0, fmax(0.0, sink))) {
        p->climbing = p->diving = 0;
        p->flaring = 1;
        double v = fmax(f->state.v, 15.0), want = -asin(fmin(0.5, p->flare_sink / v));
        double cmd = trim_alpha_deg(f, v) + 0.5 * (want - f->state.gamma) * 57.29577951308232;
        return fmax(-2.0, fmin(12.0, cmd));
    }
    if (--p->wait <= 0) { p->made = f->making; p->read = ferry_need(f); p->wait = p->glance; }
    /* Over the strip: hold the approach speed and let her settle. */
    if (f->state.x >= f->strip) return hold_speed(p, f, p->cruise);
    int motor = f->motor && f->charge > 0.0 && h > 25.0;
    /* The two needles: the glide needed to reach the strip and the glide being made over the ground. While the motor
     * runs the second needle is pinned, so the climb lasts until the needed glide has come down by a few points. */
    if (p->climbing) {
        if (!motor || p->read < p->climb_to) p->climbing = 0;
        else { *throttle = 1.0; return hold_speed(p, f, BEST_SPEED); }
    }
    double margin = p->made / fmax(p->read, 1.0);
    if (motor && margin < p->motor_on) {
        p->climbing = 1;
        p->climb_to = p->read - p->climb_by;
        *throttle = 1.0;
        return hold_speed(p, f, BEST_SPEED);
    }
    if (margin < 1.0) return hold_speed(p, f, BEST_SPEED);   /* no motor: stretch the glide */
    /* On short final, or below 100 m, there is no diving off extra height: it would only arrive fast and float. */
    int final = f->state.x > f->strip - 600.0 || h < 100.0;
    if (!final && (margin > p->high || (p->diving && margin > p->high - 0.1))) {
        /* Nose down, but never steeper than about 7 degrees of path: a dive is a way to lose height, not to aim at the ground. */
        p->diving = 1;
        return fmin(12.0, hold_speed(p, f, p->dive) + fmax(0.0, -0.12 - f->state.gamma) * 57.29577951308232);
    }
    p->diving = 0;
    return hold_speed(p, f, p->cruise);
}

/* Returns the commanded angle of attack in degrees and sets the throttle. The hand moves the command towards the
 * target at a limited rate, as a pilot eases the stick rather than snapping it. */
static inline double pilot_command(Pilot *p, const Ferry *f, double *throttle)
{
    double target = pilot_target(p, f, throttle), step = p->rate * 0.02;
    if (target > p->cmd + step) p->cmd += step;
    else if (target < p->cmd - step) p->cmd -= step;
    else p->cmd = target;
    return p->cmd;
}

#endif
