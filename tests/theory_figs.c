/* Regenerates every plotted figure of docs/theory and the table of measured values
 * (docs/theory/figures/values.md). Run with `make theory-figs`. Each figure is produced by the
 * same flight model (src/flight.c) and story layer (src/ferry.c) the game uses. */
#include "../src/ferry.h"
#include "../src/flight.h"
#include "../src/svg.h"
#include "pitch_measure.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define DEG (PI / 180.0)
#define MAXN 4096

static char dir[512];
static FILE *values;
static float xs[MAXN], ya[MAXN], yb[MAXN], yc[MAXN];

static const char *path(const char *name)
{
    static char buf[640];
    snprintf(buf, sizeof buf, "%s/%s", dir, name);
    return buf;
}

static void plot(const char *name, const char *title, const char *xl, const char *yl, int count, const Series *s, int ns)
{
    if (svg_plot(path(name), title, xl, yl, xs, count, s, ns) != 0) {
        fprintf(stderr, "cannot write %s\n", name);
        exit(1);
    }
}

/* One row of the measured-values table: id | what | measured | theory | relative error. */
static void value(const char *id, const char *what, double measured, double theory)
{
    double err = theory != 0.0 ? fabs(measured / theory - 1.0) : fabs(measured - theory);
    fprintf(values, "| `%s` | %s | %.6g | %.6g | %.2e |\n", id, what, measured, theory, err);
}

/* 01: lift and drag coefficients against angle of attack, through the stall. */
static void fig_coefficients(void)
{
    Aircraft a = glider_default();
    int n = 141;
    for (int i = 0; i < n; i++) {
        double deg = -4.0 + 0.2 * i, al = deg * DEG;
        xs[i] = (float)deg;
        ya[i] = (float)lift_coeff(&a, al);
        yb[i] = (float)(10.0 * drag_coeff(&a, al));
    }
    value("cl-at-stall", "lift coefficient at the stall angle 0.26 rad (theory CL0 + CLalpha x 0.26)", lift_coeff(&a, a.alpha_stall), 0.2 + 5.5 * 0.26);
    value("cl-floor", "lift coefficient far past the stall, at 0.60 rad (theory 0.3 x the stall value)", lift_coeff(&a, 0.60), 0.3 * (0.2 + 5.5 * 0.26));
    Series s[] = {{"CL", ya, "#1f77b4"}, {"10 x CD", yb, "#d62728"}};
    plot("coefficients.svg", "Lift and drag coefficients", "angle of attack [deg]", "coefficient", n, s, 2);
}

/* 02: the drag polar and the lift-to-drag ratio below the stall. */
static void fig_polar(void)
{
    Aircraft a = glider_default();
    int n = 161;
    double best = 0.0, best_cl = 0.0;
    for (int i = 0; i < n; i++) {
        double al = (-2.0 + 0.1 * i) * DEG, cl = lift_coeff(&a, al), cd = drag_coeff(&a, al);
        xs[i] = (float)cl;
        ya[i] = (float)cd;
        yb[i] = (float)(a.cd0 + a.k * cl * cl);
        yc[i] = (float)(cl / cd);
        if (cl / cd > best) { best = cl / cd; best_cl = cl; }
    }
    value("best-glide", "largest CL / CD over the angle sweep (theory 1 / (2 sqrt(CD0 k)))", best, best_glide_ratio(&a));
    value("best-glide-cl", "lift coefficient at which it occurs (theory sqrt(CD0 / k))", best_cl, sqrt(a.cd0 / a.k));
    Series p[] = {{"CD from the code", ya, "#1f77b4"}, {"CD0 + k CL^2", yb, "#d62728"}};
    plot("polar.svg", "Drag polar below the stall", "CL", "CD", n, p, 2);
    Series r[] = {{"CL / CD", yc, "#1f77b4"}};
    plot("glide-ratio.svg", "Lift-to-drag ratio against lift coefficient", "CL", "CL / CD", n, r, 1);
}

/* 03: lift and drag forces against airspeed at a fixed angle of attack (both grow with V squared). */
static void fig_forces(void)
{
    Aircraft a = glider_default();
    int n = 61;
    for (int i = 0; i < n; i++) {
        State s = {0.0, 100.0, 10.0 + i, 0.0, 4.0 * DEG};
        xs[i] = (float)s.v;
        ya[i] = (float)(lift_force(&a, &s) / 1000.0);
        yb[i] = (float)(drag_force(&a, &s) / 1000.0);
        yc[i] = (float)(a.mass * GRAVITY / 1000.0);
    }
    State s = {0.0, 100.0, 30.0, 0.0, 4.0 * DEG};
    value("lift-30", "lift at 30 m/s and 4 deg (theory 0.5 rho V^2 S CL), N", lift_force(&a, &s), 0.5 * 1.225 * 900.0 * 10.0 * (0.2 + 5.5 * 4.0 * DEG));
    Series f[] = {{"lift [kN]", ya, "#1f77b4"}, {"drag [kN]", yb, "#d62728"}, {"weight [kN]", yc, "#888888"}};
    plot("forces.svg", "Lift and drag at 4 deg angle of attack", "airspeed [m/s]", "force [kN]", n, f, 3);
}

/* 04: the speed polar: sink rate against airspeed for steady glides at every angle below the stall. */
static void fig_speed_polar(void)
{
    Aircraft a = glider_default();
    int n = 0;
    double min_sink = 1e9, min_sink_v = 0.0;
    for (double deg = 14.8; deg >= -1.0; deg -= 0.2) {
        State s = {0};
        trim_glide(&a, deg * DEG, &s);
        double sink = -s.v * sin(s.gamma);
        xs[n] = (float)s.v;
        ya[n] = (float)-sink;
        if (sink < min_sink) { min_sink = sink; min_sink_v = s.v; }
        n++;
    }
    State s = {0};
    trim_glide(&a, 4.0 * DEG, &s);
    double cl = lift_coeff(&a, 4.0 * DEG), cd = drag_coeff(&a, 4.0 * DEG);
    value("trim-speed-4deg", "steady-glide airspeed at 4 deg (theory sqrt(2 m g cos(gamma) / (rho S CL))), m/s", s.v,
          sqrt(2.0 * a.mass * GRAVITY * cos(atan2(cd, cl)) / (a.rho * a.area * cl)));
    value("trim-angle-4deg", "steady-glide path angle at 4 deg (theory -atan(CD / CL)), rad", s.gamma, -atan2(cd, cl));
    value("min-sink", "smallest sink rate over the sweep, m/s (reported, no closed form quoted)", min_sink, min_sink);
    value("min-sink-speed", "airspeed of the smallest sink rate, m/s (reported)", min_sink_v, min_sink_v);
    Series p[] = {{"steady glides", ya, "#1f77b4"}};
    plot("speed-polar.svg", "Speed polar: sink rate of the steady glide", "airspeed [m/s]", "vertical speed [m/s]", n, p, 1);
}

/* 05: a steady glide stays steady; the same glide with the drag removed keeps its energy. */
static void fig_energy(void)
{
    Aircraft a = glider_default(), clean = a;
    clean.cd0 = clean.k = 0.0;
    State s = {0.0, 1000.0, 0.0, 0.0, 0.0}, c = {0.0, 500.0, 30.0, 0.0, 5.0 * DEG};
    trim_glide(&a, 4.0 * DEG, &s);
    double e0 = specific_energy(&s), c0 = specific_energy(&c), v0 = s.v, worst = 0.0;
    double rate_theory = -drag_force(&a, &s) * s.v / a.mass;
    int n = 151;
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 10; k++) { flight_step(&a, &s, 4.0 * DEG, 0.0, 0.02); flight_step(&clean, &c, 5.0 * DEG, 0.0, 0.02); }
        xs[i] = (float)(0.2 * i);
        ya[i] = (float)((specific_energy(&s) - e0) / GRAVITY);
        yb[i] = (float)((specific_energy(&c) - c0) / GRAVITY);
        if (fabs(specific_energy(&c) / c0 - 1.0) > worst) worst = fabs(specific_energy(&c) / c0 - 1.0);
    }
    value("energy-no-drag", "largest relative change of V^2/2 + g h over 30 s with CD = 0 and no thrust (ideal 0)", worst, 0.0);
    value("energy-rate", "rate of energy loss in the steady 4 deg glide, W/kg (theory -D V / m)", (specific_energy(&s) - e0) / 30.0, rate_theory);
    value("steady-speed", "airspeed after 30 s of the steady 4 deg glide (theory unchanged), m/s", s.v, v0);
    Series e[] = {{"with drag", ya, "#d62728"}, {"drag removed", yb, "#1f77b4"}};
    plot("energy.svg", "Energy height lost in a glide", "time [s]", "change of energy height [m]", n, e, 2);
}

/* 06: the phugoid: speed and height trade back and forth after a 10 % speed disturbance. */
static void fig_phugoid(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 3000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, 4.0 * DEG, &s);
    double ve = s.v, expected = PI * sqrt(2.0) * ve / GRAVITY;
    s.v *= 1.1;
    int n = 301;
    double prev = s.v, prev2 = s.v, t_min = -1.0, period = -1.0, h0 = s.h, sink = -ve * sin(-atan2(drag_coeff(&a, 4.0 * DEG), lift_coeff(&a, 4.0 * DEG)));
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 10; k++) {
            flight_step(&a, &s, 4.0 * DEG, 0.0, 0.02);
            double t = 0.2 * (i - 1) + 0.02 * (k + 1);
            if (t_min < 0.0 && prev < prev2 && prev < s.v) t_min = t - 0.02;
            if (t_min > 0.0 && period < 0.0 && prev > prev2 && prev >= s.v) period = 2.0 * (t - 0.02 - t_min);
            prev2 = prev;
            prev = s.v;
        }
        xs[i] = (float)(0.2 * i);
        ya[i] = (float)(s.v - ve);
        yb[i] = (float)((s.h - h0 + sink * 0.2 * i) / 10.0);   /* height about the steady descent, in units of 10 m */
    }
    value("phugoid-period", "phugoid period from one speed minimum to the next maximum, doubled (theory pi sqrt(2) V / g), s", period, expected);
    Series p[] = {{"speed - trim [m/s]", ya, "#1f77b4"}, {"height dev. [10 m]", yb, "#d62728"}};
    plot("phugoid.svg", "Phugoid after a 10 % speed disturbance", "time [s]", "deviation", n, p, 2);
}

/* 07: the angle of attack follows the command through a first-order lag. */
static void fig_lag(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 1000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, 4.0 * DEG, &s);
    int n = 101;
    double at_tau = 0.0;
    for (int i = 0; i < n; i++) {
        if (i) flight_step(&a, &s, 8.0 * DEG, 0.0, 0.02);
        double t = 0.02 * i;
        xs[i] = (float)t;
        ya[i] = (float)(s.alpha / DEG);
        yb[i] = (float)(8.0 - 4.0 * exp(-t / a.tau));
        if (i == 12) at_tau = s.alpha / DEG;   /* t = 0.24 s, the frame nearest to tau = 0.25 s */
    }
    value("lag-at-0.24s", "angle of attack 0.24 s after a 4 -> 8 deg command (theory 8 - 4 exp(-0.24 / 0.25)), deg", at_tau, 8.0 - 4.0 * exp(-0.24 / 0.25));
    Series p[] = {{"angle of attack", ya, "#1f77b4"}, {"8 - 4 exp(-t / tau)", yb, "#d62728"}};
    plot("lag.svg", "Response of the angle of attack to a step command", "time [s]", "angle of attack [deg]", n, p, 2);
}

/* State after 20 s of the disturbed phugoid, integrated with step dt. */
static State after_20s(double dt)
{
    Aircraft a = glider_default();
    State s = {0.0, 3000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, 4.0 * DEG, &s);
    s.v *= 1.1;
    int steps = (int)(20.0 / dt + 0.5);
    for (int k = 0; k < steps; k++) flight_step(&a, &s, 4.0 * DEG, 0.0, dt);
    return s;
}

/* 08: error of the RK4 step against the step size (fourth order: halving dt divides the error by 16). */
static void fig_rk4(void)
{
    State ref = after_20s(0.00125);
    double dts[5] = {0.16, 0.08, 0.04, 0.02, 0.01}, err[5];
    for (int i = 0; i < 5; i++) {
        State s = after_20s(dts[i]);
        err[i] = fabs(s.h - ref.h);
        xs[i] = (float)log10(dts[i]);
        ya[i] = (float)log10(err[i]);
        yb[i] = (float)(log10(err[0]) + 4.0 * (log10(dts[i]) - log10(dts[0])));
    }
    value("rk4-order", "ratio of the height errors at dt = 0.08 and dt = 0.04 after 20 s (fourth order: 16)", err[1] / err[2], 16.0);
    value("rk4-error-game", "height error after 20 s at the game's dt = 0.02 against a run with dt = 0.00125, m (ideal 0)", err[3], 0.0);
    Series p[] = {{"measured", ya, "#1f77b4"}, {"slope 4", yb, "#d62728"}};
    plot("rk4.svg", "Error of one 20 s flight against the step size", "log10 dt [s]", "log10 height error [m]", 5, p, 2);
}

/* 09: where the touchdown rule draws the line between a landing and a crash. */
static void fig_touchdown(void)
{
    int n = 121;
    for (int i = 0; i < n; i++) {
        double v = 10.0 + 0.5 * i;
        xs[i] = (float)v;
        ya[i] = (float)(asin(fmin(1.0, 2.0 / v)) / DEG);   /* sink 2 m/s */
        yb[i] = (float)(0.15 / DEG);                        /* path angle 0.15 rad */
        yc[i] = ya[i] < yb[i] ? ya[i] : yb[i];
    }
    State soft = {0.0, -0.1, 25.0, -0.05, 0.0}, steep = {0.0, -0.1, 12.0, -0.16, 0.0}, fast = {0.0, -0.1, 40.0, -0.06, 0.0};
    value("touchdown-soft", "25 m/s at -0.05 rad: sink 1.25 m/s (1 = landed)", check_touchdown(&soft), 1.0);
    value("touchdown-steep", "12 m/s at -0.16 rad: sink 1.91 m/s but path steeper than 0.15 rad (2 = crashed)", check_touchdown(&steep), 2.0);
    value("touchdown-fast", "40 m/s at -0.06 rad: sink 2.40 m/s (2 = crashed)", check_touchdown(&fast), 2.0);
    Series p[] = {{"sink = 2 m/s", ya, "#d62728"}, {"angle = 0.15 rad", yb, "#888888"}, {"landing below", yc, "#1f77b4"}};
    plot("touchdown.svg", "Steepest descent angle that still counts as a landing", "airspeed at touchdown [m/s]", "descent angle [deg]", n, p, 3);
}

/* Ground distance of a 4 degree glide from the first hop's start height down to 1 m, in a given air mass. */
static double ferry_glide(double wind, double lift, float *trace, int *count)
{
    Ferry f;
    ferry_init(&f, FE_ALL);
    ferry_answer(&f, 0);
    f.wind = wind;
    f.lift = lift;
    int n = 0;
    while (ferry_value(&f, FV_H) > 1.0 && n < MAXN) {
        if (trace) { xs[n] = (float)ferry_value(&f, FV_X); trace[n] = (float)ferry_value(&f, FV_H); }
        ferry_tick(&f, 10, 4.0, 0.0);
        n++;
    }
    if (count) *count = n;
    return ferry_value(&f, FV_X);
}

/* 10: The Last Ferry: the same glide through still air, a headwind and sinking air. */
static void fig_air(void)
{
    Aircraft a = glider_default();
    State s = {0};
    trim_glide(&a, 4.0 * DEG, &s);
    double vx = s.v * cos(s.gamma), sink = -s.v * sin(s.gamma), drop = 82.0 - 1.0;
    int n = 0, m = 0;
    static float still[MAXN];
    double d0 = ferry_glide(0.0, 0.0, still, &n);
    for (int i = 0; i < n; i++) ya[i] = still[i];
    double d1 = ferry_glide(-3.0, 0.0, NULL, NULL), d2 = ferry_glide(0.0, -0.5, NULL, NULL);
    /* headwind and sink traces on the same x grid as the still-air run: h(x) is a straight line in each case */
    for (int i = 0; i < n; i++) {
        yb[i] = (float)fmax(0.0, 82.0 - xs[i] * sink / (vx - 3.0));
        yc[i] = (float)fmax(0.0, 82.0 - xs[i] * (sink + 0.5) / vx);
    }
    (void)m;
    value("air-still", "ground distance of the 4 deg glide from 82 m to 1 m in still air (theory drop x vx / sink), m", d0, drop * vx / sink);
    value("air-headwind", "the same with 3 m/s on the nose (theory drop x (vx - 3) / sink), m", d1, drop * (vx - 3.0) / sink);
    value("air-sink", "the same in air sinking 0.5 m/s (theory drop x vx / (sink + 0.5)), m", d2, drop * vx / (sink + 0.5));
    Series p[] = {{"still air", ya, "#1f77b4"}, {"3 m/s headwind", yb, "#2ca02c"}, {"air sinking 0.5", yc, "#d62728"}};
    plot("air.svg", "One glide through three air masses", "distance over the ground [m]", "height [m]", n, p, 3);
}

/* 11: The Last Ferry: the battery under full throttle, and the two glide gauges along a hop. */
static void fig_gauges(void)
{
    Ferry f;
    ferry_init(&f, FE_ALL);
    ferry_answer(&f, 0);
    double c0 = ferry_value(&f, FV_CHARGE);
    int n = 0;
    for (; n < 181; n++) {
        xs[n] = (float)(0.2 * n);
        ya[n] = (float)ferry_value(&f, FV_CHARGE);
        yb[n] = (float)ferry_value(&f, FV_THROTTLE) * 10.0f;
        ferry_tick(&f, 10, 4.0, 1.0);
    }
    value("battery-start", "battery after the hangar scene, s of full throttle (theory 26)", c0, 26.0);
    value("battery-10s", "battery after 10 s of full throttle (theory 26 - 10)", ya[50], 16.0);
    value("battery-empty", "throttle actually applied once the battery is flat (theory 0)", ferry_value(&f, FV_THROTTLE), 0.0);
    Series b[] = {{"charge [s]", ya, "#2ca02c"}, {"applied throttle x 10", yb, "#d62728"}};
    plot("battery.svg", "Battery under a full-throttle command", "time [s]", "charge and throttle", n, b, 2);

    Ferry g;
    ferry_init(&g, FE_ALL);
    ferry_answer(&g, 0);
    double need0 = ferry_need(&g), h = ferry_value(&g, FV_H), v = ferry_value(&g, FV_V);
    value("need-start", "glide needed at the start of hop 1 (theory (2500 + 80) / (82 + (V^2 - 25^2) / 2g))", need0,
          2580.0 / (h + (v * v - 625.0) / (2.0 * GRAVITY)));
    value("making-start", "glide-made readout at the start of a hop (theory: the steady 4 deg launch glide, ground speed over sink)",
          ferry_value(&g, FV_MAKING), g.state.v * cos(g.state.gamma) / (-g.state.v * sin(g.state.gamma)));
    value("low-start", "LOW gauge at the start: needed above made (1 = on)", ferry_low_on(&g), need0 > ferry_value(&g, FV_MAKING) ? 1.0 : 0.0);
    n = 0;
    while (ferry_value(&g, FV_SCENE) == FS_NONE && ferry_value(&g, FV_OUTLANDINGS) < 1.0 && n < 400) {
        xs[n] = (float)ferry_value(&g, FV_X);
        ya[n] = (float)fmin(ferry_need(&g), 60.0);
        yb[n] = (float)ferry_value(&g, FV_MAKING);
        yc[n] = 26.0f;
        ferry_tick(&g, 25, 4.0, ferry_value(&g, FV_X) < 700.0 ? 1.0 : 0.0);   /* motor for the first 700 m, then glide */
        n++;
    }
    Series q[] = {{"glide needed", ya, "#d62728"}, {"glide made", yb, "#1f77b4"}, {"red line 26", yc, "#888888"}};
    plot("gauges.svg", "Glide needed and glide made along the first hop", "distance flown [m]", "glide ratio", n, q, 3);
}


/* ---- 05: rigid-body pitch dynamics (src/pitch.c) ---- */

/* 12: the pitching moment against angle of attack, each centre of gravity trimmed at 4 deg. */
static void fig_moment(void)
{
    double margins[4] = {0.30, 0.15, 0.0, -0.05};
    float *ys[4] = {ya, yb, yc, NULL};
    static float yd[MAXN];
    ys[3] = yd;
    int n = 121;
    for (int m = 0; m < 4; m++) {
        PitchAircraft p = pitch_default();
        p.static_margin = margins[m];
        double de = pitch_trim_elevator(&p, PM_ALPHA);
        for (int i = 0; i < n; i++) {
            double deg = -2.0 + 0.1 * i;
            xs[i] = (float)deg;
            ys[m][i] = (float)pitch_moment_coeff(&p, deg * DEG, 0.0, de);
        }
    }
    PitchAircraft p = pitch_default();
    double de = pitch_trim_elevator(&p, PM_ALPHA), h = 1e-4;
    value("cm-slope", "slope dCm/dalpha of the code at SM 0.15, per rad (theory -CLalpha x SM = -0.825)",
          (pitch_moment_coeff(&p, PM_ALPHA + h, 0.0, de) - pitch_moment_coeff(&p, PM_ALPHA - h, 0.0, de)) / (2.0 * h), -5.5 * 0.15);
    value("cm-trim-elevator", "elevator that trims 4 deg at SM 0.15, deg (theory -(Cm0 + Cmalpha x 4 deg) / Cmde)", de / DEG,
          -(0.05 - 0.825 * PM_ALPHA) / -1.2 / DEG);
    value("cm-trim-alpha", "angle of attack where Cm = 0 for that elevator, deg (theory 4)", pitch_trim_alpha(&p, de) / DEG, 4.0);
    Series s[] = {{"SM 0.30", ya, "#2ca02c"}, {"SM 0.15", yb, "#1f77b4"}, {"SM 0 (neutral)", yc, "#888888"}, {"SM -0.05", yd, "#d62728"}};
    plot("pitch-moment.svg", "Pitching moment, elevator set to trim 4 deg", "angle of attack [deg]", "Cm", n, s, 4);
}

/* 13: the trimmed glide of the rigid body stays put; and V^2/2 + g h is conserved with the drag removed. */
static void fig_pitch_trim(void)
{
    PitchAircraft p = pitch_default();
    double de;
    PitchState s = pm_trimmed(&p, &de);
    double v0 = s.v, worst_a = 0.0;
    PitchAircraft clean = p;
    clean.base.cd0 = clean.base.k = 0.0;
    PitchState c = {0.0, 500.0, 30.0, 0.0, 5.0 * DEG, 0.2};
    double e0 = 0.5 * c.v * c.v + GRAVITY * c.h, worst_e = 0.0;
    int n = 151;
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 10; k++) { pitch_step(&p, &s, de, 0.0, 0.02); pitch_step(&clean, &c, 0.0, 0.0, 0.02); }
        double e = 0.5 * c.v * c.v + GRAVITY * c.h;
        xs[i] = (float)(0.2 * i);
        ya[i] = (float)(c.theta / DEG);
        yb[i] = (float)((c.theta - c.gamma) / DEG);
        yc[i] = (float)((e - e0) / GRAVITY * 1e6);
        if (fabs(pitch_alpha(&s) - PM_ALPHA) > worst_a) worst_a = fabs(pitch_alpha(&s) - PM_ALPHA);
        if (fabs(e / e0 - 1.0) > worst_e) worst_e = fabs(e / e0 - 1.0);
    }
    value("pitch-trim-drift", "largest change of alpha over 30 s of the trimmed rigid-body glide, rad (ideal 0)", worst_a, 0.0);
    value("pitch-trim-speed", "airspeed after 30 s, m/s (theory: the 4 deg steady glide of S06)", s.v, v0);
    value("pitch-energy", "largest relative change of V^2/2 + g h over 30 s with CD = 0, pitching freely (ideal 0)", worst_e, 0.0);
    Series e[] = {{"pitch attitude [deg]", ya, "#1f77b4"}, {"angle of attack [deg]", yb, "#d62728"}, {"energy change [um]", yc, "#888888"}};
    plot("pitch-free.svg", "Rigid body released at 5 deg, 0.2 rad/s, drag removed", "time [s]", "deg / micrometre", n, e, 3);
}

/* 14: the four eigenvalues in the complex plane as the centre of gravity moves (root locus). Hand-written SVG:
 * two panels, the whole plane and a zoom on the phugoid near the origin. */
static void fig_root_locus(void)
{
    FILE *f = fopen(path("root-locus.svg"), "w");
    if (!f) { fprintf(stderr, "cannot write root-locus.svg\n"); exit(1); }
    const int W = 720, H = 400, T = 50, B = 340;
    struct { double x0, x1, y0, y1; int l, r; const char *title; } pan[2] = {
        {-6.0, 1.0, -5.0, 5.0, 60, 360, "all four eigenvalues"}, {-0.7, 0.7, -0.7, 0.7, 430, 700, "zoom: phugoid and divergence"}};
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\" font-family=\"sans-serif\" font-size=\"12\">\n", W, H, W, H);
    fprintf(f, "<rect width=\"%d\" height=\"%d\" fill=\"#ffffff\"/>\n<text x=\"60\" y=\"24\" font-size=\"16\" font-weight=\"bold\" fill=\"#222\">Root locus: static margin -0.10 (red) to 0.30 (blue)</text>\n", W, H);
    for (int k = 0; k < 2; k++) {
        double sx = (pan[k].r - pan[k].l) / (pan[k].x1 - pan[k].x0), sy = (B - T) / (pan[k].y1 - pan[k].y0);
        double zx = pan[k].l - pan[k].x0 * sx, zy = T + pan[k].y1 * sy;
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"none\" stroke=\"#888\"/>\n", pan[k].l, T, pan[k].r - pan[k].l, B - T);
        fprintf(f, "<rect x=\"%.1f\" y=\"%d\" width=\"%.1f\" height=\"%d\" fill=\"#fdecea\"/>\n", zx, T, pan[k].r - zx, B - T);
        fprintf(f, "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"#444\"/><line x1=\"%d\" y1=\"%.1f\" x2=\"%d\" y2=\"%.1f\" stroke=\"#444\"/>\n",
                zx, T, zx, B, pan[k].l, zy, pan[k].r, zy);
        fprintf(f, "<text x=\"%d\" y=\"%d\" fill=\"#222\">%s</text>\n", pan[k].l, T - 8, pan[k].title);
        fprintf(f, "<text x=\"%d\" y=\"%d\" fill=\"#444\">%g</text><text x=\"%d\" y=\"%d\" text-anchor=\"end\" fill=\"#444\">%g</text>\n",
                pan[k].l, B + 16, pan[k].x0, pan[k].r, B + 16, pan[k].x1);
        fprintf(f, "<text x=\"%d\" y=\"%d\" text-anchor=\"end\" fill=\"#444\">%g</text><text x=\"%d\" y=\"%d\" text-anchor=\"end\" fill=\"#444\">%g</text>\n",
                pan[k].l - 4, T + 10, pan[k].y1, pan[k].l - 4, B, pan[k].y0);
        fprintf(f, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" fill=\"#222\">real part [1/s] (shaded: unstable)</text>\n", (pan[k].l + pan[k].r) / 2, B + 32);
        for (int i = 0; i <= 80; i++) {
            PitchAircraft p = pitch_default();
            p.static_margin = -0.10 + 0.005 * i;
            double de, a[4][4], re[4], im[4];
            PitchState s = pm_trimmed(&p, &de);
            pitch_jacobian(&p, &s, de, a);
            pitch_eigen(a, re, im);
            int red = (int)(214 - 183 * i / 80.0), green = (int)(39 + 80 * i / 80.0), blue = (int)(40 + 140 * i / 80.0);
            for (int j = 0; j < 4; j++) {
                double x = zx + re[j] * sx, y = zy - im[j] * sy;
                if (x < pan[k].l || x > pan[k].r || y < T || y > B) continue;
                fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%s\" fill=\"rgb(%d,%d,%d)\"/>\n", x, y, i == 50 ? "4.5" : "2", red, green, blue);
            }
        }
    }
    fprintf(f, "<text x=\"16\" y=\"%d\" transform=\"rotate(-90 16 %d)\" text-anchor=\"middle\" fill=\"#222\">imaginary part [rad/s]</text>\n", (T + B) / 2, (T + B) / 2);
    fprintf(f, "<text x=\"60\" y=\"392\" fill=\"#222\">large dots: the default, static margin 0.15</text>\n</svg>\n");
    if (fclose(f) != 0) exit(1);
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    int two = pitch_modes(&p, PM_ALPHA, &sp, &ph);
    value("modes-oscillatory", "two complex pairs at SM 0.15 (1 = yes)", two, 1.0);
    value("sp-eig-re", "short-period eigenvalue, real part, 1/s (reported)", sp.re, sp.re);
    value("sp-eig-im", "short-period eigenvalue, imaginary part, rad/s (reported)", sp.im, sp.im);
    value("ph-eig-re", "phugoid eigenvalue, real part, 1/s (reported)", ph.re, ph.re);
    value("ph-eig-im", "phugoid eigenvalue, imaginary part, rad/s (reported)", ph.im, ph.im);
    value("mode-separation", "short-period over phugoid natural frequency at SM 0.15 (reported)", sp.wn / ph.wn, sp.wn / ph.wn);
    double a[4][4] = {{-1, 2, 0, 0}, {-2, -1, 0, 0}, {0, 0, 3, 0}, {0, 0, 0, -0.5}}, re[4], im[4];
    pitch_eigen(a, re, im);
    value("eigen-check", "eigenvalue solver on a known matrix: imaginary part of the pair (theory 2)", im[1], 2.0);
}

/* 15/16: the short-period natural frequency and damping against the static margin: eigenvalues and the approximation. */
static void fig_short_period(void)
{
    int n = 0;
    for (double sm = 0.03; sm <= 0.3001; sm += 0.005, n++) {
        PitchAircraft p = pitch_default();
        p.static_margin = sm;
        PitchMode sp, ph;
        pitch_modes(&p, PM_ALPHA, &sp, &ph);
        double wn, zeta;
        pitch_short_period_approx(&p, PM_ALPHA, &wn, &zeta);
        xs[n] = (float)sm;
        ya[n] = (float)sp.wn;
        yb[n] = (float)wn;
    }
    Series w[] = {{"eigenvalue", ya, "#1f77b4"}, {"approximation", yb, "#d62728"}};
    plot("short-period-frequency.svg", "Short-period natural frequency", "static margin", "wn [rad/s]", n, w, 2);
    n = 0;
    for (double sm = 0.03; sm <= 0.3001; sm += 0.005, n++) {
        PitchAircraft p = pitch_default();
        p.static_margin = sm;
        PitchMode sp, ph;
        pitch_modes(&p, PM_ALPHA, &sp, &ph);
        double wn, zeta;
        pitch_short_period_approx(&p, PM_ALPHA, &wn, &zeta);
        xs[n] = (float)sm;
        ya[n] = (float)sp.zeta;
        yb[n] = (float)zeta;
    }
    Series z[] = {{"eigenvalue", ya, "#1f77b4"}, {"approximation", yb, "#d62728"}};
    plot("short-period-damping.svg", "Short-period damping ratio", "static margin", "zeta", n, z, 2);
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    double wn, zeta;
    pitch_short_period_approx(&p, PM_ALPHA, &wn, &zeta);
    value("sp-wn", "short-period natural frequency at SM 0.15 from the eigenvalue, rad/s (theory: the approximation)", sp.wn, wn);
    value("sp-zeta", "short-period damping ratio at SM 0.15 from the eigenvalue (theory: the approximation)", sp.zeta, zeta);
    p.static_margin = 0.05;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    pitch_short_period_approx(&p, PM_ALPHA, &wn, &zeta);
    value("sp-wn-0.05", "the same at SM 0.05, rad/s (theory: the approximation)", sp.wn, wn);
}

/* 17: the phugoid period of the rigid body against the static margin: eigenvalue, corrected Lanchester, Lanchester. */
static void fig_pitch_phugoid(void)
{
    int n = 0;
    double lanchester = 0.0;
    for (double sm = 0.03; sm <= 0.3001; sm += 0.005, n++) {
        PitchAircraft p = pitch_default();
        p.static_margin = sm;
        PitchMode sp, ph;
        pitch_modes(&p, PM_ALPHA, &sp, &ph);
        double de;
        PitchState s = pm_trimmed(&p, &de);
        lanchester = PM_PI * sqrt(2.0) * s.v / GRAVITY;
        xs[n] = (float)sm;
        ya[n] = (float)ph.period;
        yb[n] = (float)pitch_phugoid_theory(&p, PM_ALPHA);
        yc[n] = (float)lanchester;
    }
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    value("ph-period", "phugoid period of the rigid body at SM 0.15 from the eigenvalue, s (theory: Lanchester with pitch damping)", ph.period,
          pitch_phugoid_theory(&p, PM_ALPHA));
    p.cm_q = 0.0;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    value("ph-period-no-damping", "the same with Cmq = 0, s (theory: Lanchester pi sqrt(2) V / g)", ph.period, lanchester);
    Series s[] = {{"eigenvalue", ya, "#1f77b4"}, {"Lanchester x sqrt(1 + Z kappa)", yb, "#d62728"}, {"Lanchester", yc, "#888888"}};
    plot("pitch-phugoid.svg", "Phugoid period of the rigid-body glider", "static margin", "period [s]", n, s, 3);
}

/* 18: the simulated pitch rate started on the short-period eigenvector, with the eigenvalue's envelope. */
static void fig_sp_response(void)
{
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    double de;
    PitchState s = pm_short_period_start(&p, 0.01, &de);
    double amp = 0.01;   /* the start is 0.01 x Re(v) with v_q = 1: q(t) = 0.01 exp(re t) cos(im t) */
    int n = 201;
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 5; k++) pitch_step(&p, &s, de, 0.0, 0.002);
        double t = 0.01 * i;
        xs[i] = (float)t;
        ya[i] = (float)(s.q / DEG);
        yb[i] = (float)(amp * exp(sp.re * t) / DEG);
        yc[i] = -yb[i];
    }
    double wd, zeta, wn;
    pm_short_period_response(&p, &wd, &zeta, &wn);
    value("sp-measured-wd", "damped frequency from the zero crossings of the simulated pitch rate, rad/s (theory: imaginary part of the eigenvalue)", wd, sp.im);
    value("sp-measured-zeta", "damping ratio from the ratio of successive peaks (theory: the eigenvalue)", zeta, sp.zeta);
    Series r[] = {{"pitch rate q", ya, "#1f77b4"}, {"+ 0.01 exp(re t)", yb, "#d62728"}, {"- 0.01 exp(re t)", yc, "#d62728"}};
    plot("sp-response.svg", "Short period: simulated pitch rate and the eigenvalue envelope", "time [s]", "q [deg/s]", n, r, 3);
}

/* 19: a 1 deg nose-up elevator step: the short period settles alpha in about a second, the phugoid follows. */
static void fig_elevator_step(void)
{
    PitchAircraft p = pitch_default();
    double de;
    PitchState s = pm_trimmed(&p, &de);
    double step = de - 1.0 * DEG, target = pitch_trim_alpha(&p, step);
    int n = 401;
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 5; k++) pitch_step(&p, &s, step, 0.0, 0.02);
        xs[i] = (float)(0.1 * i);
        ya[i] = (float)(pitch_alpha(&s) / DEG);
        yb[i] = (float)(target / DEG);
        yc[i] = (float)(s.v / 5.0);
    }
    for (int k = 0; k < 150000 - 20000; k++) pitch_step(&p, &s, step, 0.0, 0.02);
    value("step-alpha", "angle of attack 3000 s after a 1 deg nose-up step, deg (theory -(Cm0 + Cmde de) / Cmalpha)", pitch_alpha(&s) / DEG, target / DEG);
    value("step-delta", "change of the steady angle of attack, deg (theory Cmde x 1 deg / Cmalpha = 1.4545)", (target - PM_ALPHA) / DEG, -1.2 / -0.825);
    Series r[] = {{"alpha [deg]", ya, "#1f77b4"}, {"moment balance [deg]", yb, "#d62728"}, {"airspeed / 5 [m/s]", yc, "#2ca02c"}};
    plot("elevator-step.svg", "Elevator step: 1 deg trailing edge up at t = 0", "time [s]", "deg, m/s / 5", n, r, 3);
}

/* 20: behind the neutral point the disturbance grows as exp(lambda t), lambda the positive real eigenvalue. */
static void fig_divergence(void)
{
    PitchAircraft p = pitch_default();
    p.static_margin = -0.05;
    double de, a[4][4], re[4], im[4], lambda = -1.0;
    PitchState s = pm_trimmed(&p, &de);
    pitch_jacobian(&p, &s, de, a);
    pitch_eigen(a, re, im);
    for (int i = 0; i < 4; i++) if (im[i] == 0.0 && re[i] > lambda) lambda = re[i];
    double a0 = pitch_alpha(&s);
    s.theta += 1e-5;
    int n = 161;
    double ref = 0.0;
    for (int i = 0; i < n; i++) {
        if (i) for (int k = 0; k < 50; k++) pitch_step(&p, &s, de, 0.0, 0.002);
        double t = 0.1 * i, d = fabs(pitch_alpha(&s) - a0);
        if (i == 100) ref = log10(d) - lambda * t / log(10.0);
        xs[i] = (float)t;
        ya[i] = (float)log10(d);
    }
    for (int i = 0; i < n; i++) yb[i] = (float)(ref + lambda * xs[i] / log(10.0));
    value("diverge-lambda", "positive real eigenvalue at SM -0.05, 1/s (reported)", lambda, lambda);
    value("diverge-rate", "growth rate of the simulated alpha disturbance between 10 s and 16 s, 1/s (theory: the eigenvalue)", pm_growth_rate(&p, 10.0, 16.0), lambda);
    /* the product of the four eigenvalues is det A; it changes sign with the static margin */
    for (int k = 0; k < 2; k++) {
        PitchAircraft q = pitch_default();
        q.static_margin = k ? -0.05 : 0.05;
        PitchState t = pm_trimmed(&q, &de);
        double b[4][4], r[4], m[4];
        pitch_jacobian(&q, &t, de, b);
        pitch_eigen(b, r, m);
        double complex det = 1.0;
        for (int i = 0; i < 4; i++) det *= r[i] + m[i] * I;
        value(k ? "det-minus" : "det-plus", k ? "product of the eigenvalues (det A) at SM -0.05, 1/s^4 (theory: negative)" : "product of the eigenvalues (det A) at SM 0.05, 1/s^4 (theory: positive)",
              creal(det), creal(det));
    }
    {   /* the two-state approximation still calls SM -0.05 stable: its stiffness -M_alpha - Z M_q stays positive */
        PitchAircraft q = pitch_default();
        q.static_margin = -0.05;
        double wn, zeta;
        pitch_short_period_approx(&q, PM_ALPHA, &wn, &zeta);
        value("approx-wn-minus", "short-period approximation at SM -0.05, rad/s (positive: the approximation misses the divergence)", wn, wn);
    }
    p.static_margin = 0.0;
    s = pm_trimmed(&p, &de);
    pitch_jacobian(&p, &s, de, a);
    pitch_eigen(a, re, im);
    double smallest = fabs(re[3]);
    value("neutral-root", "smallest eigenvalue modulus at SM 0 (theory 0: neutral stability)", smallest, 0.0);
    Series r[] = {{"log10 |alpha - trim| [rad]", ya, "#d62728"}, {"slope lambda / ln 10", yb, "#888888"}};
    plot("divergence.svg", "Static margin -0.05: a 1e-5 rad nudge diverges", "time [s]", "log10 |delta alpha| [rad]", n, r, 2);
}

/* 21: the RK4 amplification factor along the short-period eigenvalue: the step must stay below |R| = 1. */
static void fig_pitch_rk4(void)
{
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    double complex lambda = sp.re + sp.im * I;
    int n = 161;
    for (int i = 0; i < n; i++) {
        double dt = 0.005 * i;
        double complex z = lambda * dt, r = 1.0 + z + z * z / 2.0 + z * z * z / 6.0 + z * z * z * z / 24.0;
        xs[i] = (float)dt;
        ya[i] = (float)cabs(r);
        yb[i] = 1.0f;
        yc[i] = (float)exp(sp.re * dt);
    }
    double theory = pm_rk4_limit_theory(&sp);
    value("rk4-pitch-limit", "largest stable step from the simulation (bisection), s (theory: abs R(lambda dt) = 1)", pm_rk4_limit_measured(&p), theory);
    value("rk4-pitch-margin", "that limit over the game step 0.02 s (reported)", theory / 0.02, theory / 0.02);
    Series r[] = {{"|R(lambda dt)|", ya, "#1f77b4"}, {"1 (neutral)", yb, "#888888"}, {"exact exp(re dt)", yc, "#d62728"}};
    plot("pitch-rk4.svg", "RK4 growth per step on the short-period eigenvalue", "step dt [s]", "amplification per step", n, r, 3);
}

int main(int argc, char **argv)
{
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "docs/theory/figures");
    values = fopen(path("values.md"), "w");
    if (!values) { fprintf(stderr, "cannot write %s (does the directory exist?)\n", path("values.md")); return 1; }
    fprintf(values, "# Measured values\n\nGenerated by `make theory-figs` (`tests/theory_figs.c`). Do not edit by hand.\n"
                    "SI units: m, s, kg, N, rad unless a row says otherwise. Default aircraft: 300 kg, 10 m^2, air density 1.225 kg/m^3.\n\n"
                    "| id | what | measured | theory | relative error |\n|---|---|---|---|---|\n");
    fig_coefficients();
    fig_polar();
    fig_forces();
    fig_speed_polar();
    fig_energy();
    fig_phugoid();
    fig_lag();
    fig_rk4();
    fig_touchdown();
    fig_air();
    fig_gauges();
    fig_moment();
    fig_pitch_trim();
    fig_root_locus();
    fig_short_period();
    fig_pitch_phugoid();
    fig_sp_response();
    fig_elevator_step();
    fig_divergence();
    fig_pitch_rk4();
    if (fclose(values) != 0) return 1;
    printf("theory figures written to %s\n", dir);
    return 0;
}
