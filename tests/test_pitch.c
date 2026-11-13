/* Rigid-body pitch model (src/pitch.c): linear theory against the eigenvalues, and the eigenvalues
 * against the simulated nonlinear response. */
#include <math.h>
#include <stdio.h>

#include "pitch_measure.h"

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

static double rel(double a, double b) { return fabs(a / b - 1.0); }

static void test_eigen_solver_on_a_known_matrix(void)
{
    /* Block diagonal: eigenvalues -1 +- 2i, 3 and -0.5. */
    double a[4][4] = {{-1, 2, 0, 0}, {-2, -1, 0, 0}, {0, 0, 3, 0}, {0, 0, 0, -0.5}};
    double re[4], im[4];
    pitch_eigen(a, re, im);
    /* sorted by modulus: 3, then the pair (modulus sqrt 5) with the positive imaginary part first, then -0.5 */
    CHECK(fabs(re[0] - 3.0) < 1e-10 && im[0] == 0.0, "real root 3: %g%+gi", re[0], im[0]);
    CHECK(fabs(re[1] + 1.0) < 1e-10 && fabs(im[1] - 2.0) < 1e-10 && fabs(im[2] + 2.0) < 1e-10, "complex pair %g%+gi", re[1], im[1]);
    CHECK(fabs(re[3] + 0.5) < 1e-10 && im[3] == 0.0, "real root -0.5: %g%+gi", re[3], im[3]);
}

static void test_trim_is_an_equilibrium(void)
{
    PitchAircraft p = pitch_default();
    double de;
    PitchState s = pm_trimmed(&p, &de), s0 = s;
    CHECK(fabs(pitch_moment_coeff(&p, PM_ALPHA, 0.0, de)) < 1e-15, "moment at trim %g", pitch_moment_coeff(&p, PM_ALPHA, 0.0, de));
    CHECK(fabs(pitch_trim_alpha(&p, de) - PM_ALPHA) < 1e-14, "trim alpha round trip");
    for (int k = 0; k < 1500; k++) pitch_step(&p, &s, de, 0.0, 0.02);
    CHECK(fabs(s.v - s0.v) < 1e-9 && fabs(s.gamma - s0.gamma) < 1e-12 && fabs(s.q) < 1e-12 && fabs(pitch_alpha(&s) - PM_ALPHA) < 1e-12,
          "trim drifted: dv %g dgamma %g q %g", s.v - s0.v, s.gamma - s0.gamma, s.q);
}

static void test_energy_is_conserved_without_drag(void)
{
    PitchAircraft p = pitch_default();
    p.base.cd0 = p.base.k = 0.0;
    PitchState s = {0.0, 500.0, 30.0, 0.0, 5.0 * PM_DEG, 0.2};
    double e0 = 0.5 * s.v * s.v + GRAVITY * s.h;
    for (int k = 0; k < 1000; k++) pitch_step(&p, &s, 0.0, 0.0, 0.02);
    double e = 0.5 * s.v * s.v + GRAVITY * s.h;
    CHECK(fabs(e / e0 - 1.0) < 1e-6, "energy %g -> %g", e0, e);
}

static void test_two_modes_and_the_short_period_approximation(void)
{
    double margins[] = {0.05, 0.10, 0.15, 0.20, 0.30};
    for (int i = 0; i < 5; i++) {
        PitchAircraft p = pitch_default();
        p.static_margin = margins[i];
        PitchMode sp, ph;
        CHECK(pitch_modes(&p, PM_ALPHA, &sp, &ph), "SM %.2f: two oscillatory modes", margins[i]);
        CHECK(sp.wn > 5.0 * ph.wn, "SM %.2f: modes separate (%g vs %g rad/s)", margins[i], sp.wn, ph.wn);
        double wn, zeta;
        pitch_short_period_approx(&p, PM_ALPHA, &wn, &zeta);
        CHECK(rel(sp.wn, wn) < 0.05, "SM %.2f: short-period wn %.4f vs approx %.4f", margins[i], sp.wn, wn);
        CHECK(rel(sp.zeta, zeta) < 0.05, "SM %.2f: short-period zeta %.4f vs approx %.4f", margins[i], sp.zeta, zeta);
        double tp = pitch_phugoid_theory(&p, PM_ALPHA);
        CHECK(rel(ph.period, tp) < 0.05, "SM %.2f: phugoid %.3f s vs corrected Lanchester %.3f s", margins[i], ph.period, tp);
    }
}

static void test_phugoid_without_pitch_damping_is_lanchester(void)
{
    PitchAircraft p = pitch_default();
    p.cm_q = 0.0;
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    double de;
    PitchState s = pm_trimmed(&p, &de);
    double lanchester = PM_PI * sqrt(2.0) * s.v / GRAVITY;
    CHECK(rel(ph.period, lanchester) < 0.05, "phugoid %.3f s vs Lanchester %.3f s", ph.period, lanchester);
}

static void test_response_matches_the_eigenvalue(void)
{
    double margins[] = {0.10, 0.15, 0.30};
    for (int i = 0; i < 3; i++) {
        PitchAircraft p = pitch_default();
        p.static_margin = margins[i];
        PitchMode sp, ph;
        pitch_modes(&p, PM_ALPHA, &sp, &ph);
        double wd, zeta, wn;
        pm_short_period_response(&p, &wd, &zeta, &wn);
        CHECK(rel(wd, sp.im) < 0.02, "SM %.2f: measured wd %.4f vs eigenvalue %.4f", margins[i], wd, sp.im);
        CHECK(rel(zeta, sp.zeta) < 0.05, "SM %.2f: measured zeta %.4f vs eigenvalue %.4f", margins[i], zeta, sp.zeta);
    }
}

static void test_elevator_step_settles_at_the_moment_balance(void)
{
    PitchAircraft p = pitch_default();
    double de;
    PitchState s = pm_trimmed(&p, &de);
    double step = de - 1.0 * PM_DEG;   /* trailing edge up 1 deg: nose up */
    double expected = pitch_trim_alpha(&p, step);
    CHECK(fabs(expected - PM_ALPHA - p.cm_de * PM_DEG / pitch_cm_alpha(&p)) < 1e-12, "trim alpha is linear in the elevator");
    for (int k = 0; k < 150000; k++) pitch_step(&p, &s, step, 0.0, 0.02);
    CHECK(fabs(pitch_alpha(&s) - expected) < 1e-6, "alpha after 3000 s %.7f vs %.7f", pitch_alpha(&s), expected);
    CHECK(fabs(s.q) < 1e-5, "pitch rate settles: %g", s.q);
}

static void test_negative_static_margin_diverges(void)
{
    PitchAircraft p = pitch_default();
    p.static_margin = -0.05;
    double de;
    PitchState s = pm_trimmed(&p, &de);
    double a[4][4], re[4], im[4], lambda = -1.0;
    pitch_jacobian(&p, &s, de, a);
    pitch_eigen(a, re, im);
    for (int i = 0; i < 4; i++) if (im[i] == 0.0 && re[i] > lambda) lambda = re[i];
    CHECK(lambda > 0.0, "a real positive eigenvalue: %g", lambda);
    double rate = pm_growth_rate(&p, 10.0, 16.0);
    CHECK(rel(rate, lambda) < 0.03, "growth rate %.4f vs eigenvalue %.4f", rate, lambda);
    /* and the stable aircraft has no eigenvalue with a positive real part */
    p.static_margin = 0.15;
    s = pm_trimmed(&p, &de);
    pitch_jacobian(&p, &s, de, a);
    pitch_eigen(a, re, im);
    for (int i = 0; i < 4; i++) CHECK(re[i] < 0.0, "SM 0.15 eigenvalue %d has real part %g", i, re[i]);
}

static void test_rk4_stability_limit(void)
{
    PitchAircraft p = pitch_default();
    PitchMode sp, ph;
    pitch_modes(&p, PM_ALPHA, &sp, &ph);
    double theory = pm_rk4_limit_theory(&sp), measured = pm_rk4_limit_measured(&p);
    CHECK(rel(measured, theory) < 0.01, "RK4 limit measured %.4f s vs |R(lambda dt)| = 1 at %.4f s", measured, theory);
    CHECK(theory > 10.0 * 0.02, "the game step 0.02 s is far inside the limit (%.3f s)", theory);
}

int main(void)
{
    test_eigen_solver_on_a_known_matrix();
    test_trim_is_an_equilibrium();
    test_energy_is_conserved_without_drag();
    test_two_modes_and_the_short_period_approximation();
    test_phugoid_without_pitch_damping_is_lanchester();
    test_response_matches_the_eigenvalue();
    test_elevator_step_settles_at_the_moment_balance();
    test_negative_static_margin_diverges();
    test_rk4_stability_limit();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("pitch: all tests passed");
    return 0;
}
