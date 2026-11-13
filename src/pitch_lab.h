#ifndef PITCH_LAB_H
#define PITCH_LAB_H

/* Browser bridge for the pitch laboratory: one rigid-body glider (src/pitch.c) trimmed at 4 deg, its
 * four eigenvalues, and a step-by-step simulation the page draws. All numbers are computed here in C. */
enum {
    PL_TIME, PL_ALPHA, PL_Q, PL_THETA, PL_GAMMA, PL_V, PL_ELEVATOR, PL_TRIM_ALPHA, PL_STATIC_MARGIN,
    PL_SP_WN, PL_SP_ZETA, PL_APPROX_WN, PL_APPROX_ZETA, PL_PH_PERIOD, PL_PH_THEORY, PL_OSCILLATORY, PL_DEPARTED,
    PL_EIG_RE0, PL_EIG_RE1, PL_EIG_RE2, PL_EIG_RE3, PL_EIG_IM0, PL_EIG_IM1, PL_EIG_IM2, PL_EIG_IM3, PL_FIELDS
};
/* Trimmed 4 deg glide with the given static margin (clamped to -0.10..0.30); time 0, elevator offset 0. */
void pl_reset(double static_margin);
/* Elevator offset from the 4 deg trim elevator, in degrees (clamped to +-5). Positive = trailing edge down = nose down. */
void pl_elevator(double offset_degrees);
/* Adds a pitch-rate disturbance, in deg/s. */
void pl_kick(double q_degrees);
/* Advances by frames x 0.02 s (at most 250 frames); stops once the glider departs (|alpha| > 25 deg or V out of 8..80 m/s). */
void pl_step(int frames);
/* Angles in degrees, rates in deg/s, eigenvalues in 1/s. */
double pl_value(int field);

#endif
