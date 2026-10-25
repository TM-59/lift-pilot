#include "observer.h"
#include "flight.h"
#include <math.h>

#define DEG (3.14159265358979 / 180.0)
#define DT 0.02
static Aircraft aircraft;
static State state;
static double elapsed, command, power, runway;
static int mode;
static Status status;

void lp_reset(int next_mode)
{
    aircraft = glider_default();
    mode = next_mode == 1 || next_mode == 2 ? next_mode : 0;
    state = (State){0.0, mode == 2 ? 3000.0 : 100.0, 0.0, 0.0, 0.0};
    trim_glide(&aircraft, 4.0 * DEG, &state);
    if (mode == 2) state.v *= 1.1;
    command = 4.0 * DEG;
    power = elapsed = 0.0;
    status = FLYING;
    /* Same dry-run placement as the terminal GIF; scenery does not set collision. */
    State probe = {0.0, 100.0, 0.0, 0.0, 0.0};
    trim_glide(&aircraft, 4.0 * DEG, &probe);
    for (int i = 0; i < 30000 && check_touchdown(&probe) == FLYING; i++)
        flight_step(&aircraft, &probe, 4.0 * DEG, 0.0, DT);
    runway = probe.x - 100.0;
}

void lp_controls(double command_degrees, double throttle)
{
    if (mode || !isfinite(command_degrees) || !isfinite(throttle)) return;
    command = fmax(-10.0, fmin(20.0, command_degrees)) * DEG;
    power = fmax(0.0, fmin(1.0, throttle));
}

void lp_step(int frames)
{
    if (frames < 0) return;
    if (frames > 250) frames = 250;
    for (int i = 0; i < frames && status == FLYING; i++) {
        flight_step(&aircraft, &state, command, power, DT);
        elapsed += DT;
        status = check_touchdown(&state);
    }
}

double lp_value(int field)
{
    switch (field) {
    case LP_X: return state.x;
    case LP_H: return state.h;
    case LP_V: return state.v;
    case LP_GAMMA: return state.gamma;
    case LP_ALPHA: return state.alpha;
    case LP_TIME: return elapsed;
    case LP_COMMAND: return command / DEG;
    case LP_THROTTLE: return power;
    case LP_LIFT: return lift_force(&aircraft, &state);
    case LP_DRAG: return drag_force(&aircraft, &state);
    case LP_LD: return lift_coeff(&aircraft, state.alpha) / drag_coeff(&aircraft, state.alpha);
    case LP_SINK: return -state.v * sin(state.gamma);
    case LP_ENERGY: return specific_energy(&state);
    case LP_STATUS: return status;
    case LP_RUNWAY: return runway;
    case LP_ON_RUNWAY: return status != FLYING && state.x >= runway && state.x <= runway + 300.0;
    case LP_MODE: return mode;
    case LP_STALL: return state.alpha > aircraft.alpha_stall;
    default: return 0.0;
    }
}
