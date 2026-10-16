#include <math.h>
#include <stdio.h>

#include "../src/flight.h"
#include "../src/render.h"

static int failures;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

static double deg(double d) { return d * 3.14159265358979 / 180.0; }

static void test_best_glide_matches_the_polar(void)
{
    Aircraft a = glider_default();
    double best = 0.0;
    for (double al = deg(-2.0); al < a.alpha_stall; al += 0.0005) {
        double r = lift_coeff(&a, al) / drag_coeff(&a, al);
        if (r > best) best = r;
    }
    double expected = best_glide_ratio(&a);
    CHECK(fabs(best - expected) / expected < 0.01, "numeric best L/D %.2f vs analytic %.2f", best, expected);
}

static void test_lift_peaks_at_the_stall_angle(void)
{
    Aircraft a = glider_default();
    double peak = lift_coeff(&a, a.alpha_stall);
    CHECK(lift_coeff(&a, a.alpha_stall - 0.02) < peak, "lift should still be rising below stall");
    CHECK(lift_coeff(&a, a.alpha_stall + 0.02) < peak, "lift should fall past stall");
    CHECK(drag_coeff(&a, a.alpha_stall + 0.1) > 1.5 * drag_coeff(&a, a.alpha_stall), "drag should rise past stall");
}

static void test_steady_glide_stays_steady(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 1000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    double v0 = s.v, g0 = s.gamma;
    for (int k = 0; k < 1500; k++) flight_step(&a, &s, deg(4.0), 0.0, 0.02);
    CHECK(fabs(s.v - v0) / v0 < 0.002, "airspeed drifted %.4f -> %.4f", v0, s.v);
    CHECK(fabs(s.gamma - g0) < 0.002, "path angle drifted %.4f -> %.4f", g0, s.gamma);
    double ratio = s.x / (1000.0 - s.h);
    CHECK(fabs(ratio - lift_coeff(&a, deg(4.0)) / drag_coeff(&a, deg(4.0))) < 0.5, "glide ratio %.2f", ratio);
}

static void test_energy_is_conserved_without_drag(void)
{
    Aircraft a = glider_default();
    a.cd0 = 0.0;
    a.k = 0.0;
    State s = {0.0, 500.0, 30.0, 0.0, deg(5.0)};
    double e0 = specific_energy(&s);
    for (int k = 0; k < 1000; k++) flight_step(&a, &s, deg(5.0), 0.0, 0.02);
    CHECK(fabs(specific_energy(&s) - e0) / e0 < 1e-4, "energy %.3f -> %.3f", e0, specific_energy(&s));
}

static void test_phugoid_period_follows_lanchester(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 3000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    double ve = s.v, expected = 3.14159265358979 * sqrt(2.0) * ve / GRAVITY;
    s.v *= 1.1;
    double dt = 0.02, prev = s.v, prev2 = s.v, t_min = -1.0, period = -1.0;
    for (int k = 1; k < 4000 && period < 0.0; k++) {
        flight_step(&a, &s, deg(4.0), 0.0, dt);
        double t = k * dt;
        if (t_min < 0.0 && prev < prev2 && prev < s.v) t_min = t - dt;
        if (t_min > 0.0 && prev > prev2 && prev >= s.v) period = t - dt;
        prev2 = prev;
        prev = s.v;
    }
    CHECK(period > 0.0, "no phugoid cycle found");
    CHECK(fabs(period - expected) / expected < 0.1, "period %.2f s vs Lanchester %.2f s", period, expected);
}

static void test_touchdown_rating(void)
{
    State soft = {0.0, -0.1, 25.0, -0.05, 0.0};
    State hard = {0.0, -0.1, 25.0, -0.5, 0.0};
    State air = {0.0, 10.0, 25.0, -0.05, 0.0};
    CHECK(check_touchdown(&soft) == LANDED, "gentle touchdown should be a landing");
    CHECK(check_touchdown(&hard) == CRASHED, "steep touchdown should crash");
    CHECK(check_touchdown(&air) == FLYING, "above ground is still flying");
}

static void test_render_fits_the_screen(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 400.0, 25.0, -0.03, 0.05};
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    size_t used = render_frame(&a, &s, 3000.0, buf, sizeof buf);
    int lines = 0;
    for (size_t i = 0; i < used; i++) lines += buf[i] == '\n';
    CHECK(lines == RENDER_ROWS, "expected %d lines, got %d", RENDER_ROWS, lines);
    CHECK(render_frame(&a, &s, 3000.0, buf, 8) == 0, "tiny buffer must be refused");
}

int main(void)
{
    test_best_glide_matches_the_polar();
    test_lift_peaks_at_the_stall_angle();
    test_steady_glide_stays_steady();
    test_energy_is_conserved_without_drag();
    test_phugoid_period_follows_lanchester();
    test_touchdown_rating();
    test_render_fits_the_screen();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
