#ifndef PITCH_H
#define PITCH_H

#include "flight.h"

/* Rigid-body longitudinal dynamics: the point-mass model of flight.c plus a pitch degree of freedom.
 * The angle of attack is no longer commanded through a lag: it is alpha = theta - gamma, where the
 * pitch attitude theta turns at the pitch rate q, and q is driven by the pitching moment
 *   M = qbar S c Cm,  Cm = cm0 + cm_alpha alpha + cm_q (q c / 2V) + cm_de delta_e,  cm_alpha = -cl_alpha * static_margin.
 * Lift and drag are the coefficients of flight.c (the elevator adds no lift). This model is separate:
 * flight_step, the laboratory and The Last Ferry keep the lag model and their outputs unchanged. */
typedef struct {
    Aircraft base;          /* mass, wing area, density, lift curve, polar, thrust (base.tau is not used) */
    double chord;           /* mean aerodynamic chord c [m] */
    double inertia;         /* pitch moment of inertia Iyy [kg m^2] */
    double static_margin;   /* (x_neutral_point - x_cg) / c; positive = statically stable */
    double cm0;             /* pitching moment coefficient at alpha = 0, q = 0, elevator 0 */
    double cm_q;            /* pitch damping per unit of q c / (2V) */
    double cm_de;           /* elevator power per rad; positive delta_e = trailing edge down = nose down */
} PitchAircraft;

/* State: x [m], h [m], airspeed v [m/s], path angle gamma [rad], pitch attitude theta [rad], pitch rate q [rad/s]. */
typedef struct {
    double x, h, v, gamma, theta, q;
} PitchState;

/* One oscillatory mode of the linearised motion. */
typedef struct {
    double re, im;          /* eigenvalue with im >= 0 [1/s] */
    double wn, zeta;        /* natural frequency |lambda| [rad/s], damping ratio -re / |lambda| */
    double period;          /* 2 pi / im [s]; 0 when the pair is real */
} PitchMode;

PitchAircraft pitch_default(void);
double pitch_cm_alpha(const PitchAircraft *p);
double pitch_moment_coeff(const PitchAircraft *p, double alpha, double qhat, double elevator);
double pitch_alpha(const PitchState *s);
/* Elevator angle [rad] that makes the pitching moment zero at alpha with q = 0. */
double pitch_trim_elevator(const PitchAircraft *p, double alpha);
/* Angle of attack [rad] at which the pitching moment is zero for a given elevator (linear lift curve). */
double pitch_trim_alpha(const PitchAircraft *p, double elevator);
/* Steady unpowered glide trimmed by the elevator at alpha: fills v, gamma, theta, q = 0 (keeps x, h). */
void pitch_trim(const PitchAircraft *p, double alpha, PitchState *s);
/* One RK4 step; elevator in rad, throttle in 0..1. */
void pitch_step(const PitchAircraft *p, PitchState *s, double elevator, double throttle, double dt);
/* Jacobian of (v, gamma, theta, q) about a steady state, by central differences. */
void pitch_jacobian(const PitchAircraft *p, const PitchState *trim, double elevator, double a[4][4]);
/* All four eigenvalues of a 4x4 matrix (characteristic polynomial, Durand-Kerner), sorted by |lambda| descending. */
void pitch_eigen(double a[4][4], double re[4], double im[4]);
/* The short-period and phugoid modes of the glide trimmed at alpha: the two eigenvalue pairs, fast pair first.
 * Returns 1 when both pairs are complex (two oscillations), 0 otherwise (the modes are then raw eigenvalues 0/1 and 2/3). */
int pitch_modes(const PitchAircraft *p, double alpha, PitchMode *short_period, PitchMode *phugoid);
/* Short-period approximation at constant airspeed: w^2 = -M_alpha - Z M_q, 2 zeta w = Z - M_q with Z = qbar S cl_alpha / (m V). */
void pitch_short_period_approx(const PitchAircraft *p, double alpha, double *wn, double *zeta);
/* Phugoid period of Lanchester, pi sqrt(2) V / g, corrected for pitch damping: the moment balance gives
 * delta alpha = -kappa q with kappa = cm_q c / (2 V cm_alpha), which scales the phugoid frequency by 1 / sqrt(1 + Z kappa). */
double pitch_phugoid_theory(const PitchAircraft *p, double alpha);

#endif
