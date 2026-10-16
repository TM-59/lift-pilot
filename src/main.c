#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "flight.h"
#include "gif.h"
#include "render.h"
#include "svg.h"

#define RUNWAY_X 4200.0
#define DT 0.02

static double deg(double d) { return d * 3.14159265358979 / 180.0; }

/* Phugoid demo: trimmed glide disturbed by +10% airspeed, written as CSV and SVG charts. */
static int headless(const char *dir)
{
    Aircraft a = glider_default();
    State s = {0.0, 3000.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    s.v *= 1.1;
    enum { N = 6000, STRIDE = 20, M = N / STRIDE };
    static float t[M], v[M], hh[M], xx[M];
    char path[512];
    snprintf(path, sizeof path, "%s/run.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "t,x,h,v,gamma,alpha\n");
    for (int k = 0; k < N; k++) {
        if (k % STRIDE == 0) {
            int i = k / STRIDE;
            t[i] = (float)(k * DT); xx[i] = (float)s.x; hh[i] = (float)s.h; v[i] = (float)s.v;
            fprintf(csv, "%.2f,%.1f,%.2f,%.3f,%.5f,%.5f\n", k * DT, s.x, s.h, s.v, s.gamma, s.alpha);
        }
        flight_step(&a, &s, deg(4.0), 0.0, DT);
    }
    fclose(csv);

    snprintf(path, sizeof path, "%s/speed.svg", dir);
    Series sp[] = {{"airspeed V [m/s]", v, "#1f77b4"}};
    if (svg_plot(path, "Phugoid: airspeed after a +10% disturbance", "t [s]", "V [m/s]", t, M, sp, 1)) return 1;
    snprintf(path, sizeof path, "%s/track.svg", dir);
    Series tr[] = {{"altitude h [m]", hh, "#2ca02c"}};
    if (svg_plot(path, "Flight path (altitude vs distance)", "x [m]", "h [m]", xx, M, tr, 1)) return 1;

    enum { P = 60 };
    static float al[P], cl[P], cd[P], ld[P], cdx[P];
    for (int i = 0; i < P; i++) {
        double alpha = deg(-2.0) + i * (a.alpha_stall + deg(6.0) - deg(-2.0)) / (P - 1);
        al[i] = (float)(alpha * 180.0 / 3.14159265358979);
        cl[i] = (float)lift_coeff(&a, alpha);
        cd[i] = (float)drag_coeff(&a, alpha);
        cdx[i] = cd[i];
        ld[i] = cl[i] / cd[i];
    }
    snprintf(path, sizeof path, "%s/polar.svg", dir);
    Series po[] = {{"CL", cl, "#d62728"}};
    if (svg_plot(path, "Drag polar (CL vs CD)", "CD", "CL", cdx, P, po, 1)) return 1;
    snprintf(path, sizeof path, "%s/ld.svg", dir);
    Series lr[] = {{"L/D", ld, "#9467bd"}};
    if (svg_plot(path, "Lift-to-drag ratio vs angle of attack", "alpha [deg]", "L/D", al, P, lr, 1)) return 1;
    printf("best L/D %.1f, trim V %.1f m/s\n", best_glide_ratio(&a), s.v);
    return 0;
}

static int snapshot(void)
{
    Aircraft a = glider_default();
    State s = {0.0, 300.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    if (!render_frame(&a, &s, RUNWAY_X, buf, sizeof buf)) return 1;
    fputs(buf, stdout);
    return 0;
}

static int gif_colour(char c)
{
    if (c == '=') return 2;
    if (c == '>' || c == '/' || c == '\\') return 3;
    if (c == '_' || c == '|') return 4;
    return 1;
}

/* Scripted landing: a 4 degree glide from 100 m, with the runway placed where a dry run touches down. */
static int gif_demo(const char *path)
{
    Aircraft a = glider_default();
    State s = {0.0, 100.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    State probe = s;
    Status st = FLYING;
    while (st == FLYING) {
        flight_step(&a, &probe, deg(4.0), 0.0, DT);
        st = check_touchdown(&probe);
    }
    double runway_x = probe.x - 100.0;

    unsigned char pal[GIF_COLOURS][3] = {{10, 14, 28}, {235, 235, 235}, {250, 200, 60}, {90, 200, 255},
                                         {70, 150, 80}};
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    int w = RENDER_COLS * 6 * 3, h = RENDER_ROWS * 9 * 3;
    static unsigned char px[RENDER_COLS * 6 * 3 * RENDER_ROWS * 9 * 3];
    Gif *g = gif_open(path, w, h, pal, 6);
    if (!g) return 1;
    st = FLYING;
    for (int k = 0; st == FLYING; k++) {
        if (k % 50 == 0) {
            render_frame(&a, &s, runway_x, buf, sizeof buf);
            gif_draw_text(px, w, h, buf, 3, gif_colour);
            if (gif_frame(g, px, 0)) return 1;
        }
        flight_step(&a, &s, deg(4.0), 0.0, DT);
        st = check_touchdown(&s);
    }
    s.h = 0.0;
    render_frame(&a, &s, runway_x, buf, sizeof buf);
    gif_draw_text(px, w, h, buf, 3, gif_colour);
    if (gif_frame(g, px, 200) || gif_close(g)) return 1;
    printf("touchdown at x=%.0f m (runway %.0f..%.0f m)\n", s.x, runway_x, runway_x + 300.0);
    return 0;
}

static struct termios saved;
static void restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &saved); fputs("\033[?25h", stdout); }

static int play(void)
{
    if (!isatty(STDIN_FILENO)) { fputs("interactive mode needs a terminal; try --snapshot\n", stderr); return 1; }
    tcgetattr(STDIN_FILENO, &saved);
    struct termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restore);
    fputs("\033[2J\033[?25l", stdout);

    Aircraft a = glider_default();
    State s = {0.0, 400.0, 0.0, 0.0, 0.0};
    trim_glide(&a, deg(4.0), &s);
    double cmd = deg(4.0), throttle = 0.0;
    int paused = 0;
    Status st = FLYING;
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    struct timespec tick = {0, 40 * 1000000L};
    while (st == FLYING) {
        char c;
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q') return 0;
            if (c == 'w') cmd += deg(1.0);
            if (c == 's') cmd -= deg(1.0);
            if (c == 'r') throttle = fmin(1.0, throttle + 0.1);
            if (c == 'f') throttle = fmax(0.0, throttle - 0.1);
            if (c == 'p') paused = !paused;
        }
        if (cmd > deg(20.0)) cmd = deg(20.0);
        if (cmd < deg(-10.0)) cmd = deg(-10.0);
        if (!paused) {
            for (int k = 0; k < 2 && st == FLYING; k++) {
                flight_step(&a, &s, cmd, throttle, DT);
                st = check_touchdown(&s);
            }
        }
        render_frame(&a, &s, RUNWAY_X, buf, sizeof buf);
        printf("\033[H%s", buf);
        printf("alt %5.0f m  V %5.1f m/s  alpha %4.1f deg  cmd %4.1f  thr %3.0f%%%s  \n",
               s.h, s.v, s.alpha * 57.2958, cmd * 57.2958, throttle * 100.0,
               s.alpha > a.alpha_stall ? "  STALL!" : "");
        puts("w/s pitch  r/f throttle  p pause  q quit    land softly on the === runway");
        fflush(stdout);
        nanosleep(&tick, NULL);
    }
    int on_runway = s.x >= RUNWAY_X && s.x <= RUNWAY_X + 300.0;
    printf("\n%s at x=%.0f m (%s)\n", st == LANDED ? "Landed" : "Crashed", s.x,
           on_runway ? "on the runway" : "off the runway");
    return st == LANDED && on_runway ? 0 : 2;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--headless") == 0) return headless(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0) return snapshot();
    if (argc > 1 && strcmp(argv[1], "--gif") == 0) return gif_demo(argc > 2 ? argv[2] : "demo.gif");
    return play();
}
