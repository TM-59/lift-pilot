/* Measurements of the rigid-body pitch model taken from simulated time histories, shared by
 * tests/test_pitch.c (assertions) and tests/theory_figs.c (figures and docs/theory/figures/values.md). */
#ifndef PITCH_MEASURE_H
#define PITCH_MEASURE_H

#include "../src/pitch.h"

#include <complex.h>
#include <math.h>

#define PM_PI 3.14159265358979323846
#define PM_DEG (PM_PI / 180.0)
#define PM_ALPHA (4.0 * PM_DEG)   /* every linearisation is about the 4 deg glide of S06 */

/* Steady glide at 4 deg, 1000 m up, trimmed by the elevator returned through *elevator. */
static PitchState pm_trimmed(const PitchAircraft *p, double *elevator)
{
    PitchState s = {0.0, 1000.0, 0.0, 0.0, 0.0, 0.0};
    pitch_trim(p, PM_ALPHA, &s);
    *elevator = pitch_trim_elevator(p, PM_ALPHA);
    return s;
}

/* Eigenvector of the short period, normalised to v_q = 1: solves the first three rows of (A - lambda I) v = 0. */
static void pm_short_period_vector(const PitchAircraft *p, double complex v[4])
{
    double de, a[4][4], re[4], im[4];
    PitchState s = pm_trimmed(p, &de);
    pitch_jacobian(p, &s, de, a);
    pitch_eigen(a, re, im);
    double complex lambda = re[0] + im[0] * I, m[3][4];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) m[i][j] = a[i][j] - (i == j ? lambda : 0.0);
        m[i][3] = -a[i][3];   /* right-hand side with v_q = 1 */
    }
    for (int c = 0; c < 3; c++) {   /* Gauss-Jordan with partial pivoting */
        int piv = c;
        for (int r = c + 1; r < 3; r++) if (cabs(m[r][c]) > cabs(m[piv][c])) piv = r;
        for (int j = 0; j < 4; j++) { double complex t = m[c][j]; m[c][j] = m[piv][j]; m[piv][j] = t; }
        for (int r = 0; r < 3; r++) {
            if (r == c) continue;
            double complex f = m[r][c] / m[c][c];
            for (int j = c; j < 4; j++) m[r][j] -= f * m[c][j];
        }
    }
    for (int i = 0; i < 3; i++) v[i] = m[i][3] / m[i][i];
    v[3] = 1.0;
}

/* The trimmed glide displaced along the real part of the short-period eigenvector, with a pitch-rate amplitude
 * of `size` rad/s. Started there, the linearised motion contains the short period only (no phugoid). */
static PitchState pm_short_period_start(const PitchAircraft *p, double size, double *elevator)
{
    double complex v[4];
    pm_short_period_vector(p, v);
    PitchState s = pm_trimmed(p, elevator);
    s.v += size * creal(v[0]);
    s.gamma += size * creal(v[1]);
    s.theta += size * creal(v[2]);
    s.q += size * creal(v[3]);
    return s;
}

/* Damped frequency and damping ratio of the short period measured from the simulated pitch rate, started on the
 * short-period eigenvector (0.01 rad/s): the time between the first two zero crossings is half a damped period,
 * the ratio of the first two (opposite) peaks is exp(-sigma pi / wd). dt = 0.002 s, peaks by parabola. */
static void pm_short_period_response(const PitchAircraft *p, double *wd, double *zeta, double *wn)
{
    double de, dt = 0.002;
    PitchState s = pm_short_period_start(p, 0.01, &de);
    double q0 = s.q, q1, q2 = 0.0, zc[2] = {0, 0}, pk[2] = {0, 0};
    int nz = 0, np = 0;
    q1 = q0;
    pitch_step(p, &s, de, 0.0, dt);
    q2 = s.q;
    for (int k = 2; k < 5000 && (nz < 2 || np < 2); k++) {
        double qm = q1, qc = q2;   /* q at steps k-2, k-1 */
        pitch_step(p, &s, de, 0.0, dt);
        double qn = s.q;
        if (nz < 2 && qc != 0.0 && (qc > 0.0) != (qn > 0.0)) zc[nz++] = (k - 1 + qc / (qc - qn)) * dt;
        if (np < 2 && k > 2 && ((qc > qm && qc >= qn) || (qc < qm && qc <= qn))) {
            double den = qm - 2.0 * qc + qn, off = den != 0.0 ? 0.5 * (qm - qn) / den : 0.0;
            pk[np++] = qc - 0.25 * (qm - qn) * off;
        }
        q1 = qc;
        q2 = qn;
    }
    *wd = PM_PI / (zc[1] - zc[0]);
    double sigma = *wd * log(fabs(pk[0] / pk[1])) / PM_PI;
    *wn = hypot(sigma, *wd);
    *zeta = sigma / *wn;
}

/* Growth rate of an angle-of-attack disturbance: ln(|da(t2)| / |da(t1)|) / (t2 - t1). */
static double pm_growth_rate(const PitchAircraft *p, double t1, double t2)
{
    double de, dt = 0.002;
    PitchState s = pm_trimmed(p, &de);
    double a0 = pitch_alpha(&s), d1 = 0.0;
    s.theta += 1e-5;
    int n1 = (int)(t1 / dt + 0.5), n2 = (int)(t2 / dt + 0.5);
    for (int k = 1; k <= n2; k++) {
        pitch_step(p, &s, de, 0.0, dt);
        if (k == n1) d1 = pitch_alpha(&s) - a0;
    }
    return log(fabs((pitch_alpha(&s) - a0) / d1)) / (t2 - t1);
}

/* Largest RK4 step for which a small pitch-rate disturbance does not grow (bisection on the simulation). */
static double pm_rk4_limit_measured(const PitchAircraft *p)
{
    double lo = 0.05, hi = 2.0, de;
    PitchState trim = pm_trimmed(p, &de);
    for (int it = 0; it < 40; it++) {
        double dt = 0.5 * (lo + hi);
        PitchState s = trim;
        s.q += 1e-7;
        double early = 0.0, late = 0.0;   /* largest |dq| over steps 1000..1199 and 3000..3199 */
        for (int k = 0; k < 3200; k++) {
            pitch_step(p, &s, de, 0.0, dt);
            double d = fabs(s.q - trim.q);
            if (!isfinite(d)) { late = INFINITY; break; }
            if (k >= 1000 && k < 1200 && d > early) early = d;
            if (k >= 3000 && d > late) late = d;
        }
        if (late > early) hi = dt; else lo = dt;
    }
    return 0.5 * (lo + hi);
}

/* The same limit from the RK4 amplification factor R(z) = 1 + z + z^2/2 + z^3/6 + z^4/24: the step at which
 * |R(lambda dt)| = 1 along the direction of the short-period eigenvalue. */
static double pm_rk4_limit_theory(const PitchMode *sp)
{
    double complex lambda = sp->re + sp->im * I;
    double lo = 0.0, hi = 4.0 / sp->wn;
    for (int it = 0; it < 80; it++) {
        double dt = 0.5 * (lo + hi);
        double complex z = lambda * dt, r = 1.0 + z + z * z / 2.0 + z * z * z / 6.0 + z * z * z * z / 24.0;
        if (cabs(r) > 1.0) hi = dt; else lo = dt;
    }
    return 0.5 * (lo + hi);
}

#endif
