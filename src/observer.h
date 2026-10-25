#ifndef OBSERVER_H
#define OBSERVER_H

enum {
    LP_X, LP_H, LP_V, LP_GAMMA, LP_ALPHA, LP_TIME, LP_COMMAND, LP_THROTTLE,
    LP_LIFT, LP_DRAG, LP_LD, LP_SINK, LP_ENERGY, LP_STATUS, LP_RUNWAY,
    LP_ON_RUNWAY, LP_MODE, LP_STALL, LP_FIELDS
};
/* mode: 0 manual approach, 1 trimmed landing demonstration, 2 phugoid. */
void lp_reset(int mode);
void lp_controls(double command_degrees, double throttle);
void lp_step(int frames);
double lp_value(int field);

#endif
