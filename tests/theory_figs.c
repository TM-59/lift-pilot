/* Regenerates every plotted figure of docs/theory and the table of measured values
 * (docs/theory/figures/values.md). Run with `make theory-figs`. Each figure is produced by the
 * same flight model (src/flight.c) and story layer (src/ferry.c) the game uses. */
#include "../src/ferry.h"
#include "../src/flight.h"
#include "../src/svg.h"

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
    value("making-start", "glide-made readout at the start of a hop, as coded (it starts at its cap, 60; the steady 4 deg glide is 26.8)",
          ferry_value(&g, FV_MAKING), 60.0);
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
    if (fclose(values) != 0) return 1;
    printf("theory figures written to %s\n", dir);
    return 0;
}
