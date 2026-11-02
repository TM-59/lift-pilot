#ifndef FERRY_H
#define FERRY_H

#include "flight.h"

/* "The Last Ferry": a story layer over the unchanged flight model in flight.c.
 * The club's last motor glider, the Kestrel, is flown down a valley in five hops,
 * strip to strip, to the buyer on the coast. Each hop starts at the top of the
 * climb-out and ends with a landing; the pilot commands the angle of attack and
 * the throttle exactly as in manual flight. Story scenes open at fixed progress
 * points (the start of the run and the end of a hop) and change game numbers
 * only: the battery charge, where the next strip is and how long it is, the air
 * the hop is flown through (a uniform vertical and horizontal motion of the air
 * mass, added to the position after each unchanged RK4 step), the start height
 * and, once, the aircraft's parameters (the motor taken out). */

enum { F_HOPS = 5 };

enum {
    FS_NONE = -1,
    FS_HANGAR,   /* Joost's hangar at Upper Field; his own spare cells go in the back */
    FS_WEIR,     /* choice: across the quarry (short, sinking air) or round Hob Ridge (long, rising air) */
    FS_ORCHARD,  /* Ros's orchard: the school has put the Kestrel on its timetable; the battery is topped up */
    FS_BUYER,    /* there is no school: the buyer breaks aircraft for parts (heavy: shown with no banner) */
    FS_MILL,     /* choice: pull the motor for Joost at Mill Strip, or fly her on whole (heavy) */
    FS_DOORS,    /* the hangar at Upper Field is emptied; the sea breeze and the breaker's short yard strip (heavy) */
    FS_COAST,    /* the breaker's yard; ending */
    FS_COUNT
};

/* Numeric story effects, one bit each, so the bench can remove them one by one. */
enum {
    FE_CELLS = 1u << 0,    /* Joost's spare cells: battery 5 -> 26 s of full throttle */
    FE_ROUTE = 1u << 1,    /* quarry: strip at 2,500 m, air sinking 0.5 m/s; ridge: strip at 3,500 m, climb-out 15 m higher */
    FE_ORCHARD = 1u << 2,  /* Ros's solar panels: +15 s of charge */
    FE_WIND = 1u << 3,     /* the valley wind gets up for the last two hops: 3 m/s on the nose */
    FE_MOTOR = 1u << 4,    /* pulling the motor: no throttle, a cleaner airframe (CD0 0.015 -> 0.014), a winch launch to 145 m */
    FE_DOORS = 1u << 5,    /* the last hop: the sea breeze (2 m/s more on the nose, air sinking 0.15 m/s), a 250 m yard strip */
    FE_ALL = (1u << 6) - 1u
};

enum { FB_NONE, FB_CLEAR, FB_OUT };  /* banner: none, "strip made", an outlanding */

typedef struct {
    Aircraft aircraft;
    State state;
    unsigned effects;
    int hop;              /* 0..4; F_HOPS when the run is over */
    int scene;            /* FS_* open now, or FS_NONE */
    int route;            /* -1 until chosen; 0 = across the quarry, 1 = round Hob Ridge */
    int pulled;           /* -1 until chosen; 0 = the motor pulled at Mill Strip, 1 = flown on whole */
    int motor;            /* 1 while the Kestrel has a motor */
    double command, throttle;  /* applied in the last frame (rad, 0..1) */
    double time;          /* mission clock, s; every second is flown (a retrieval re-flies the hop, no flat waits) */
    double hop_time;
    double charge;        /* battery, in seconds of full throttle */
    double low;           /* s flown with the LOW gauge on: the glide needed is steeper than the glide being made */
    double making;        /* glide being made over the ground (smoothed readout) */
    double strip, length, start_h, lift, wind;  /* this hop: strip threshold, strip length, start height [m], air [m/s] */
    int outlandings;
    int trailered;        /* hops finished on a trailer after three outlandings */
    int out_hop[F_HOPS];
    int crisis_hop[F_HOPS];   /* 1 if the LOW gauge came on in the hop, or the hop ended in an outlanding */
    int banner;           /* FB_* shown on screen */
    double banner_left;
    int loud;             /* number of times a "strip made" banner was raised */
    long frames;
    int order[FS_COUNT];
    int order_len;
} Ferry;

enum FerryValue {
    FV_X, FV_H, FV_V, FV_GAMMA, FV_ALPHA, FV_TIME, FV_COMMAND, FV_THROTTLE, FV_LIFT, FV_DRAG, FV_LD, FV_SINK,
    FV_STALL, FV_HOP, FV_SCENE, FV_BANNER, FV_STRIP, FV_LENGTH, FV_START_H, FV_AIR_LIFT, FV_WIND, FV_CHARGE,
    FV_MOTOR, FV_LOW, FV_LOW_ON, FV_NEED, FV_OUTLANDINGS, FV_ROUTE, FV_PULLED, FV_ENDING, FV_LOUD, FV_HEAVY,
    FV_GROUND_SPEED, FV_MAKING, FV_HOP_TIME, FV_TRAILERED, FV_OUT_HOP, FV_FIELDS
};

/* Glide ratio needed to reach a third of the way down the strip from here, above which the LOW gauge turns red. */
#define F_LOW_LINE 26.0

void ferry_init(Ferry *f, unsigned effects);
/* frames of one 0.02 s RK4 step each, under one command (degrees of angle of attack, throttle 0..1).
 * Does nothing while a scene is open or after the end. */
void ferry_tick(Ferry *f, int frames, double command_degrees, double throttle);
/* Close the open scene. Choice scenes take option 0 or 1; others take 0. Returns 0 when accepted. */
int ferry_answer(Ferry *f, int option);
int ferry_options(int scene);
/* 1 for scenes that must be shown without a "strip made" banner (the heavy scenes). */
int ferry_heavy(int scene);
/* 0 = the motor pulled, 1 = flown on whole; -1 before the coast. */
int ferry_ending(const Ferry *f);
/* Glide ratio needed to reach a third of the way down the strip: distance over energy height (height plus the height the speed
 * above 25 m/s would buy, as a total-energy gauge reads it); 0 over or past the strip. */
double ferry_need(const Ferry *f);
/* LOW: the glide needed to reach the strip is steeper than the glide being made over the ground. */
int ferry_low_on(const Ferry *f);
double ferry_value(const Ferry *f, int field);

#endif
