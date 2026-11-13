#include "pitch.h"

#include <complex.h>
#include <math.h>

PitchAircraft pitch_default(void)
{
    PitchAircraft p = {
        .base = glider_default(),
        .chord = 1.0, .inertia = 400.0, .static_margin = 0.15,
        .cm0 = 0.05, .cm_q = -10.0, .cm_de = -1.2,
    };
    return p;
}

double pitch_cm_alpha(const PitchAircraft *p)
{
    return -p->base.cl_alpha * p->static_margin;
}

double pitch_moment_coeff(const PitchAircraft *p, double alpha, double qhat, double elevator)
{
    return p->cm0 + pitch_cm_alpha(p) * alpha + p->cm_q * qhat + p->cm_de * elevator;
}

double pitch_alpha(const PitchState *s)
{
    return s->theta - s->gamma;
}

double pitch_trim_elevator(const PitchAircraft *p, double alpha)
{
    return -(p->cm0 + pitch_cm_alpha(p) * alpha) / p->cm_de;
}

double pitch_trim_alpha(const PitchAircraft *p, double elevator)
{
    return -(p->cm0 + p->cm_de * elevator) / pitch_cm_alpha(p);
}

void pitch_trim(const PitchAircraft *p, double alpha, PitchState *s)
{
    State g = {s->x, s->h, 0.0, 0.0, 0.0};
    trim_glide(&p->base, alpha, &g);
    s->v = g.v;
    s->gamma = g.gamma;
    s->theta = g.gamma + alpha;
    s->q = 0.0;
}

static void pitch_derivative(const PitchAircraft *p, const PitchState *s, double elevator, double throttle, PitchState *d)
{
    const Aircraft *a = &p->base;
    State point = {s->x, s->h, s->v, s->gamma, pitch_alpha(s)};
    double l = lift_force(a, &point), dr = drag_force(a, &point), thrust = throttle * a->max_thrust;
    double qbar = 0.5 * a->rho * s->v * s->v;
    double cm = pitch_moment_coeff(p, point.alpha, s->q * p->chord / (2.0 * s->v), elevator);
    d->x = s->v * cos(s->gamma);
    d->h = s->v * sin(s->gamma);
    d->v = (thrust - dr) / a->mass - GRAVITY * sin(s->gamma);
    d->gamma = (l / a->mass - GRAVITY * cos(s->gamma)) / s->v;
    d->theta = s->q;
    d->q = qbar * a->area * p->chord * cm / p->inertia;
}

static PitchState pitch_advance(const PitchState *s, const PitchState *d, double dt)
{
    PitchState n = {s->x + d->x * dt, s->h + d->h * dt, s->v + d->v * dt,
                    s->gamma + d->gamma * dt, s->theta + d->theta * dt, s->q + d->q * dt};
    return n;
}

void pitch_step(const PitchAircraft *p, PitchState *s, double elevator, double throttle, double dt)
{
    PitchState k1, k2, k3, k4;
    pitch_derivative(p, s, elevator, throttle, &k1);
    PitchState s2 = pitch_advance(s, &k1, dt / 2.0);
    pitch_derivative(p, &s2, elevator, throttle, &k2);
    PitchState s3 = pitch_advance(s, &k2, dt / 2.0);
    pitch_derivative(p, &s3, elevator, throttle, &k3);
    PitchState s4 = pitch_advance(s, &k3, dt);
    pitch_derivative(p, &s4, elevator, throttle, &k4);
    s->x += dt / 6.0 * (k1.x + 2 * k2.x + 2 * k3.x + k4.x);
    s->h += dt / 6.0 * (k1.h + 2 * k2.h + 2 * k3.h + k4.h);
    s->v += dt / 6.0 * (k1.v + 2 * k2.v + 2 * k3.v + k4.v);
    s->gamma += dt / 6.0 * (k1.gamma + 2 * k2.gamma + 2 * k3.gamma + k4.gamma);
    s->theta += dt / 6.0 * (k1.theta + 2 * k2.theta + 2 * k3.theta + k4.theta);
    s->q += dt / 6.0 * (k1.q + 2 * k2.q + 2 * k3.q + k4.q);
}

static double *slot(PitchState *s, int i)
{
    double *f[4] = {&s->v, &s->gamma, &s->theta, &s->q};
    return f[i];
}

void pitch_jacobian(const PitchAircraft *p, const PitchState *trim, double elevator, double a[4][4])
{
    double scale[4] = {trim->v, 1.0, 1.0, 1.0};
    for (int j = 0; j < 4; j++) {
        double eps = 1e-6 * scale[j];
        PitchState up = *trim, down = *trim, du, dd;
        *slot(&up, j) += eps;
        *slot(&down, j) -= eps;
        pitch_derivative(p, &up, elevator, 0.0, &du);
        pitch_derivative(p, &down, elevator, 0.0, &dd);
        for (int i = 0; i < 4; i++) a[i][j] = (*slot(&du, i) - *slot(&dd, i)) / (2.0 * eps);
    }
}

void pitch_eigen(double a[4][4], double re[4], double im[4])
{
    /* Faddeev-LeVerrier: det(lambda I - A) = lambda^4 + c[1] lambda^3 + c[2] lambda^2 + c[3] lambda + c[4]. */
    double m[4][4] = {{0}}, am[4][4], c[5] = {1.0, 0, 0, 0, 0};
    for (int k = 1; k <= 4; k++) {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) m[i][j] += (i == j) * c[k - 1];
        double tr = 0.0;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                am[i][j] = 0.0;
                for (int l = 0; l < 4; l++) am[i][j] += a[i][l] * m[l][j];
            }
        for (int i = 0; i < 4; i++) tr += am[i][i];
        c[k] = -tr / k;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) m[i][j] = am[i][j];
    }
    /* Durand-Kerner on the monic quartic. */
    double complex z[4], start = 0.4 + 0.9 * I;
    double radius = 1.0;
    for (int k = 1; k <= 4; k++) radius = fmax(radius, pow(fabs(c[k]), 1.0 / k));
    for (int i = 0; i < 4; i++) z[i] = radius * cpow(start, i);
    for (int it = 0; it < 500; it++) {
        double change = 0.0;
        for (int i = 0; i < 4; i++) {
            double complex num = ((((z[i] + c[1]) * z[i] + c[2]) * z[i] + c[3]) * z[i] + c[4]), den = 1.0;
            for (int j = 0; j < 4; j++)
                if (j != i) den *= z[i] - z[j];
            double complex step = num / den;
            z[i] -= step;
            change = fmax(change, cabs(step) / fmax(1.0, cabs(z[i])));
        }
        if (change < 1e-15) break;
    }
    /* Sort by modulus, largest first; within a pair put the positive imaginary part first. */
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++) {
            double ai = cabs(z[i]), aj = cabs(z[j]);
            if (aj > ai * (1.0 + 1e-9) || (fabs(aj - ai) <= ai * 1e-9 && cimag(z[j]) > cimag(z[i]))) {
                double complex t = z[i]; z[i] = z[j]; z[j] = t;
            }
        }
    for (int i = 0; i < 4; i++) {
        re[i] = creal(z[i]);
        im[i] = fabs(cimag(z[i])) < 1e-12 * fmax(1.0, cabs(z[i])) ? 0.0 : cimag(z[i]);
    }
}

static PitchMode mode_of(double re, double im)
{
    PitchMode m = {re, fabs(im), hypot(re, im), 0.0, 0.0};
    m.zeta = m.wn > 0.0 ? -re / m.wn : 0.0;
    m.period = m.im > 0.0 ? 2.0 * 3.14159265358979323846 / m.im : 0.0;
    return m;
}

int pitch_modes(const PitchAircraft *p, double alpha, PitchMode *short_period, PitchMode *phugoid)
{
    PitchState s = {0.0, 1000.0, 0, 0, 0, 0};
    pitch_trim(p, alpha, &s);
    double a[4][4], re[4], im[4];
    pitch_jacobian(p, &s, pitch_trim_elevator(p, alpha), a);
    pitch_eigen(a, re, im);
    *short_period = mode_of(re[0], im[0]);
    *phugoid = mode_of(re[2], im[2]);
    int both = im[0] != 0.0 && im[2] != 0.0 && fabs(im[0] + im[1]) < 1e-9 * (1.0 + fabs(im[0])) && fabs(im[2] + im[3]) < 1e-9 * (1.0 + fabs(im[2]));
    return both;
}

void pitch_short_period_approx(const PitchAircraft *p, double alpha, double *wn, double *zeta)
{
    const Aircraft *a = &p->base;
    PitchState s = {0.0, 1000.0, 0, 0, 0, 0};
    pitch_trim(p, alpha, &s);
    double qbar = 0.5 * a->rho * s.v * s.v, k = qbar * a->area * p->chord / p->inertia;
    double m_alpha = k * pitch_cm_alpha(p), m_q = k * p->cm_q * p->chord / (2.0 * s.v);
    double z = qbar * a->area * a->cl_alpha / (a->mass * s.v);
    double w2 = -m_alpha - z * m_q;
    *wn = w2 > 0.0 ? sqrt(w2) : 0.0;
    *zeta = *wn > 0.0 ? (z - m_q) / (2.0 * *wn) : 0.0;
}

double pitch_phugoid_theory(const PitchAircraft *p, double alpha)
{
    const Aircraft *a = &p->base;
    PitchState s = {0.0, 1000.0, 0, 0, 0, 0};
    pitch_trim(p, alpha, &s);
    double qbar = 0.5 * a->rho * s.v * s.v, z = qbar * a->area * a->cl_alpha / (a->mass * s.v);
    double kappa = p->cm_q * p->chord / (2.0 * s.v * pitch_cm_alpha(p));
    return 3.14159265358979323846 * sqrt(2.0) * s.v / GRAVITY * sqrt(1.0 + z * kappa);
}
