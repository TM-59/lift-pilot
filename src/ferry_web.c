#include "ferry.h"

/* Browser bridge for "The Last Ferry": the same C ferry flight the story bench flies. */
static Ferry ferry;

void fw_reset(void) { ferry_init(&ferry, FE_ALL); }
void fw_tick(int frames, double command_degrees, double throttle) { ferry_tick(&ferry, frames, command_degrees, throttle); }
int fw_answer(int option) { return ferry_answer(&ferry, option); }
double fw_value(int field) { return ferry_value(&ferry, field); }
