#include "flight.h"

#include <math.h>

Aircraft glider_default(void)
{
    Aircraft a = {
        .mass = 300.0, .area = 10.0, .rho = 1.225,
        .cl0 = 0.2, .cl_alpha = 5.5, .alpha_stall = 0.26,
        .cd0 = 0.015, .k = 0.02,
        .tau = 0.25, .max_thrust = 0.1 * 300.0 * GRAVITY,
    };
    return a;
}

double lift_coeff(const Aircraft *a, double alpha)
{
    double cl_stall = a->cl0 + a->cl_alpha * a->alpha_stall;
    if (alpha <= a->alpha_stall) return a->cl0 + a->cl_alpha * alpha;
    double cl = cl_stall * (1.0 - 3.0 * (alpha - a->alpha_stall));
    return cl > 0.3 * cl_stall ? cl : 0.3 * cl_stall;
}

double drag_coeff(const Aircraft *a, double alpha)
{
    double cl = lift_coeff(a, alpha);
    double cd = a->cd0 + a->k * cl * cl;
    if (alpha > a->alpha_stall) cd += 10.0 * (alpha - a->alpha_stall) * (alpha - a->alpha_stall);
    return cd;
}

static double dynamic_pressure(const Aircraft *a, double v)
{
    return 0.5 * a->rho * v * v;
}

double lift_force(const Aircraft *a, const State *s)
{
    return dynamic_pressure(a, s->v) * a->area * lift_coeff(a, s->alpha);
}

double drag_force(const Aircraft *a, const State *s)
{
    return dynamic_pressure(a, s->v) * a->area * drag_coeff(a, s->alpha);
}

void trim_glide(const Aircraft *a, double alpha, State *s)
{
    double cl = lift_coeff(a, alpha), cd = drag_coeff(a, alpha);
    s->alpha = alpha;
    s->gamma = -atan2(cd, cl);
    s->v = sqrt(2.0 * a->mass * GRAVITY * cos(s->gamma) / (a->rho * a->area * cl));
}

double best_glide_ratio(const Aircraft *a)
{
    return 1.0 / (2.0 * sqrt(a->cd0 * a->k));
}

static void derivative(const Aircraft *a, const State *s, double alpha_cmd, double throttle, State *d)
{
    double l = lift_force(a, s), dr = drag_force(a, s);
    double thrust = throttle * a->max_thrust;
    d->x = s->v * cos(s->gamma);
    d->h = s->v * sin(s->gamma);
    d->v = (thrust - dr) / a->mass - GRAVITY * sin(s->gamma);
    d->gamma = (l / a->mass - GRAVITY * cos(s->gamma)) / s->v;
    d->alpha = (alpha_cmd - s->alpha) / a->tau;
}

static State advance(const State *s, const State *d, double dt)
{
    State n = {s->x + d->x * dt, s->h + d->h * dt, s->v + d->v * dt,
               s->gamma + d->gamma * dt, s->alpha + d->alpha * dt};
    return n;
}

void flight_step(const Aircraft *a, State *s, double alpha_cmd, double throttle, double dt)
{
    State k1, k2, k3, k4;
    derivative(a, s, alpha_cmd, throttle, &k1);
    State s2 = advance(s, &k1, dt / 2.0);
    derivative(a, &s2, alpha_cmd, throttle, &k2);
    State s3 = advance(s, &k2, dt / 2.0);
    derivative(a, &s3, alpha_cmd, throttle, &k3);
    State s4 = advance(s, &k3, dt);
    derivative(a, &s4, alpha_cmd, throttle, &k4);
    s->x += dt / 6.0 * (k1.x + 2 * k2.x + 2 * k3.x + k4.x);
    s->h += dt / 6.0 * (k1.h + 2 * k2.h + 2 * k3.h + k4.h);
    s->v += dt / 6.0 * (k1.v + 2 * k2.v + 2 * k3.v + k4.v);
    s->gamma += dt / 6.0 * (k1.gamma + 2 * k2.gamma + 2 * k3.gamma + k4.gamma);
    s->alpha += dt / 6.0 * (k1.alpha + 2 * k2.alpha + 2 * k3.alpha + k4.alpha);
}

Status check_touchdown(State *s)
{
    if (s->h > 0.0) return FLYING;
    double sink = -s->v * sin(s->gamma);
    s->h = 0.0;
    return sink < 2.0 && fabs(s->gamma) < 0.15 ? LANDED : CRASHED;
}

double specific_energy(const State *s)
{
    return 0.5 * s->v * s->v + GRAVITY * s->h;
}
