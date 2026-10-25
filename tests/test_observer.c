#include "../src/observer.h"
#include "../src/flight.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static int reference_trace(void)
{
    puts("mode,step,x,h,v,gamma,alpha,time,command,throttle,lift,drag,ld,sink,energy,status,runway,on_runway,mode_value,stall");
    for (int mode = 0; mode < 3; mode++) {
        lp_reset(mode);
        for (int i = 0; i <= 6000; i++) {
            if (i % 20 == 0) {
                printf("%d,%d", mode, i);
                for (int field = 0; field < LP_FIELDS; field++) printf(",%.17g", lp_value(field));
                putchar('\n');
            }
            if (mode == 0 && i == 100) lp_controls(7.0, 0.2);
            if (mode == 0 && i == 400) lp_controls(3.0, 0.0);
            if (mode == 0 && i == 800) lp_controls(4.0, 0.0);
            lp_step(1);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--trace") == 0) return reference_trace();
    /* The adapter must produce the unmodified point-mass model, not a JS approximation. */
    Aircraft a = glider_default();
    State direct = {0.0, 100.0, 0.0, 0.0, 0.0};
    const double deg = 3.14159265358979 / 180.0;
    trim_glide(&a, 4.0 * deg, &direct);
    lp_reset(0);
    for (int i = 0; i < 1200; i++) {
        double cmd = i < 500 ? 5.0 : 3.5;
        double power = i < 500 ? 0.1 : 0.0;
        lp_controls(cmd, power);
        lp_step(1);
        flight_step(&a, &direct, cmd * deg, power, 0.02);
        CHECK(fabs(lp_value(LP_X) - direct.x) < 1e-9);
        CHECK(fabs(lp_value(LP_H) - direct.h) < 1e-9);
        CHECK(fabs(lp_value(LP_V) - direct.v) < 1e-9);
    }
    lp_controls(INFINITY, 0.5);
    CHECK(lp_value(LP_THROTTLE) == 0.0);
    lp_controls(200.0, 5.0);
    CHECK(fabs(lp_value(LP_COMMAND) - 20.0) < 1e-12);
    CHECK(lp_value(LP_THROTTLE) == 1.0);
    lp_step(100);
    CHECK(lp_value(LP_STALL) == 1.0);
    lp_reset(1);
    lp_controls(20.0, 1.0);
    CHECK(lp_value(LP_COMMAND) == 4.0 && lp_value(LP_THROTTLE) == 0.0);
    for (int i = 0; i < 30000 && lp_value(LP_STATUS) == FLYING; i++) lp_step(1);
    CHECK(lp_value(LP_STATUS) == LANDED);
    CHECK(lp_value(LP_ON_RUNWAY) == 1.0);
    CHECK(lp_value(LP_SINK) < 2.0);
    printf("trimmed landing: t=%.2f s x=%.6f m sink=%.6f m/s\n", lp_value(LP_TIME), lp_value(LP_X), lp_value(LP_SINK));
    double t = lp_value(LP_TIME);
    lp_step(100);
    CHECK(lp_value(LP_TIME) == t);
    lp_reset(0);
    lp_controls(-10.0, 0.0);
    for (int i = 0; i < 30000 && lp_value(LP_STATUS) == FLYING; i++) lp_step(1);
    CHECK(lp_value(LP_STATUS) == CRASHED);
    lp_reset(2);
    lp_step(-1);
    CHECK(lp_value(LP_TIME) == 0.0);
    lp_step(10000);
    CHECK(fabs(lp_value(LP_TIME) - 5.0) < 1e-9);
    CHECK(lp_value(LP_H) > 2900.0);
    printf("observer: %d checks passed\n", checks);
    return 0;
}
