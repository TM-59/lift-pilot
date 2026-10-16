#ifndef FLIGHT_H
#define FLIGHT_H

/* Longitudinal point-mass flight dynamics of a motor glider in still air (SI units).
 * State: x [m], h [m], airspeed V [m/s], flight path angle gamma [rad], angle of attack alpha [rad].
 * The elevator commands alpha through a first-order lag (the "short period" is not modelled). */
typedef struct {
    double mass, area, rho;
    double cl0, cl_alpha, alpha_stall;
    double cd0, k;           /* parabolic drag polar CD = cd0 + k * CL^2 */
    double tau;              /* angle-of-attack response time [s] */
    double max_thrust;       /* [N] */
} Aircraft;

typedef struct {
    double x, h, v, gamma, alpha;
} State;

typedef enum { FLYING, LANDED, CRASHED } Status;

#define GRAVITY 9.80665

Aircraft glider_default(void);
double lift_coeff(const Aircraft *a, double alpha);
double drag_coeff(const Aircraft *a, double alpha);
double lift_force(const Aircraft *a, const State *s);
double drag_force(const Aircraft *a, const State *s);
/* Steady glide for a fixed alpha: fills airspeed and (negative) flight path angle. */
void trim_glide(const Aircraft *a, double alpha, State *s);
/* Best lift-to-drag ratio from the polar: 1 / (2 sqrt(cd0 k)). */
double best_glide_ratio(const Aircraft *a);
/* One RK4 step; alpha_cmd in rad, throttle in 0..1. */
void flight_step(const Aircraft *a, State *s, double alpha_cmd, double throttle, double dt);
/* Ground contact test after a step: touchdown is gentle if the sink rate and path angle are small. */
Status check_touchdown(State *s);
double specific_energy(const State *s);

#endif
