#include "pitch_lab.h"
#include "pitch.h"

#include <math.h>

#define DEG (3.14159265358979323846 / 180.0)
#define DT 0.02
static PitchAircraft craft;
static PitchState state;
static PitchMode sp, ph;
static double elapsed, trim_elevator, offset, approx_wn, approx_zeta, eig_re[4], eig_im[4];
static int oscillatory, departed;

void pl_reset(double static_margin)
{
    craft = pitch_default();
    if (isfinite(static_margin)) craft.static_margin = fmax(-0.10, fmin(0.30, static_margin));
    state = (PitchState){0.0, 1000.0, 0.0, 0.0, 0.0, 0.0};
    pitch_trim(&craft, 4.0 * DEG, &state);
    trim_elevator = pitch_trim_elevator(&craft, 4.0 * DEG);
    offset = elapsed = 0.0;
    departed = 0;
    double a[4][4];
    pitch_jacobian(&craft, &state, trim_elevator, a);
    pitch_eigen(a, eig_re, eig_im);
    oscillatory = pitch_modes(&craft, 4.0 * DEG, &sp, &ph);
    pitch_short_period_approx(&craft, 4.0 * DEG, &approx_wn, &approx_zeta);
}

void pl_elevator(double offset_degrees)
{
    if (isfinite(offset_degrees)) offset = fmax(-5.0, fmin(5.0, offset_degrees)) * DEG;
}

void pl_kick(double q_degrees)
{
    if (isfinite(q_degrees) && !departed) state.q += fmax(-30.0, fmin(30.0, q_degrees)) * DEG;
}

void pl_step(int frames)
{
    if (frames < 0) return;
    if (frames > 250) frames = 250;
    for (int i = 0; i < frames && !departed; i++) {
        pitch_step(&craft, &state, trim_elevator + offset, 0.0, DT);
        elapsed += DT;
        double alpha = pitch_alpha(&state);
        if (!(fabs(alpha) <= 25.0 * DEG && state.v >= 8.0 && state.v <= 80.0)) departed = 1;
    }
}

double pl_value(int field)
{
    if (field >= PL_EIG_RE0 && field <= PL_EIG_RE3) return eig_re[field - PL_EIG_RE0];
    if (field >= PL_EIG_IM0 && field <= PL_EIG_IM3) return eig_im[field - PL_EIG_IM0];
    switch (field) {
    case PL_TIME: return elapsed;
    case PL_ALPHA: return pitch_alpha(&state) / DEG;
    case PL_Q: return state.q / DEG;
    case PL_THETA: return state.theta / DEG;
    case PL_GAMMA: return state.gamma / DEG;
    case PL_V: return state.v;
    case PL_ELEVATOR: return offset / DEG;
    case PL_TRIM_ALPHA: return fabs(craft.static_margin) < 1e-9 ? NAN : pitch_trim_alpha(&craft, trim_elevator + offset) / DEG;
    case PL_STATIC_MARGIN: return craft.static_margin;
    case PL_SP_WN: return sp.wn;
    case PL_SP_ZETA: return sp.zeta;
    case PL_APPROX_WN: return approx_wn;
    case PL_APPROX_ZETA: return approx_zeta;
    case PL_PH_PERIOD: return ph.period;
    case PL_PH_THEORY: return pitch_phugoid_theory(&craft, 4.0 * DEG);
    case PL_OSCILLATORY: return oscillatory;
    case PL_DEPARTED: return departed;
    default: return 0.0;
    }
}
