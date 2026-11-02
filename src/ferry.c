#include "ferry.h"

#include <math.h>
#include <string.h>

#define DEG (3.14159265358979 / 180.0)
#define F_DT 0.02              /* one frame: one RK4 step, as in manual flight */
#define STRIP_LEN 600.0        /* an ordinary grass strip [m] */
#define BASE_CHARGE 5.0        /* battery at Upper Field, in seconds of full throttle */
#define TRAILER_AFTER 3        /* outlandings in one hop before a trailer takes the Kestrel on by road */
#define HOP_LIMIT 360.0        /* s without landing: counted as an outlanding */
#define F_GLIDE_CAP 60.0       /* the glide-made readout tops out here (a climb reads 60) */
#define F_MAKING_LAG 5.0       /* s: the glide-made readout is smoothed like a needle */
#define F_AIM 80.0             /* m: the gauge aims this far past the threshold */
#define F_REF_SPEED 25.0       /* m/s: the speed at which the glide gauge counts height as height */

#define CELLS_CHARGE 26.0      /* FE_CELLS: Joost's spare cells, s of full throttle */
#define QUARRY_STRIP 2500.0    /* FE_ROUTE: the strip across the quarry [m] */
#define QUARRY_LIFT (-0.50)    /* FE_ROUTE: the air over the quarry sinks [m/s] */
#define RIDGE_STRIP 3500.0     /* FE_ROUTE: the strip round Hob Ridge [m] */
#define RIDGE_CLIMB 15.0       /* FE_ROUTE: the climb-out along the slope ends this much higher [m] */
#define ORCHARD_CHARGE 15.0    /* FE_ORCHARD: Ros's solar panels, s of full throttle */
#define VALLEY_WIND (-3.0)     /* FE_WIND: the valley wind on the nose, hops 4 and 5 [m/s] */
#define CLEAN_CD0 0.0140       /* FE_MOTOR: no motor pylon or folded propeller (from 0.015) */
#define WINCH_H 145.0          /* FE_MOTOR: the mill's winch launch [m] */
#define SEA_LIFT (-0.15)       /* FE_DOORS: the sea breeze sinks the air on the last hop [m/s] */
#define SEA_WIND (-2.0)        /* FE_DOORS: the sea breeze adds this much on the nose [m/s] */
#define YARD_LEN 250.0         /* FE_DOORS: the breaker's yard strip [m] */

/* Strip thresholds and the height at the top of the climb-out, hop by hop. */
static const double STRIP[F_HOPS] = {2500.0, 3000.0, 2600.0, 3000.0, 2800.0};
static const double START_H[F_HOPS] = {82.0, 120.0, 98.0, 80.0, 88.0};

/* The buyer's letter, the mill and the emptied hangar get the screen to themselves: no "strip made" banner. */
static const int HEAVY[FS_COUNT] = {0, 0, 0, 1, 1, 1, 0};

int ferry_options(int scene) { return scene == FS_WEIR || scene == FS_MILL ? 2 : 1; }
int ferry_heavy(int scene) { return scene >= 0 && scene < FS_COUNT && HEAVY[scene]; }

int ferry_ending(const Ferry *f)
{
    return f->scene == FS_COAST || f->hop >= F_HOPS ? f->pulled : -1;
}

static int on(const Ferry *f, unsigned effect) { return (f->effects & effect) != 0; }

/* Energy height, as a sailplane's total-energy gauge reads it: speed above the reference speed counts as height. */
static double energy_height(const State *s)
{
    return s->h + (s->v * s->v - F_REF_SPEED * F_REF_SPEED) / (2.0 * GRAVITY);
}

double ferry_need(const Ferry *f)
{
    if (f->hop >= F_HOPS || f->state.x >= f->strip) return 0.0;
    double aim = f->strip + F_AIM - f->state.x;
    return aim / fmax(energy_height(&f->state), 0.1);
}

int ferry_low_on(const Ferry *f)
{
    return f->scene == FS_NONE && f->hop < F_HOPS && ferry_need(f) > f->making;
}

/* The glide being made over the ground in the last step, from the change in energy height (so trading speed for
 * height does not move it): ground covered over energy lost, capped at 60 (a climb, or no loss, reads 60). */
static double glide_step(double dx, double de)
{
    if (dx <= 0.0) return 0.0;
    if (-de <= dx / F_GLIDE_CAP) return F_GLIDE_CAP;
    return dx / -de;
}

static void open_scene(Ferry *f, int scene)
{
    f->scene = scene;
    if (f->order_len < FS_COUNT) f->order[f->order_len++] = scene;
    if (ferry_heavy(scene)) { f->banner = FB_NONE; f->banner_left = 0.0; }
}

static void raise_banner(Ferry *f, int banner)
{
    f->banner = banner;
    f->banner_left = 2.0;
    if (banner == FB_CLEAR) f->loud++;
}

/* The top of the climb-out (or of the winch launch): a trimmed 4 degree glide, the strip ahead. */
static void launch(Ferry *f)
{
    f->state = (State){0.0, f->start_h, 0.0, 0.0, 0.0};
    trim_glide(&f->aircraft, 4.0 * DEG, &f->state);
    f->command = 4.0 * DEG;
    f->throttle = 0.0;
    f->hop_time = 0.0;
    /* The readout starts at the steady glide of the launch: ground speed over sink through the moving air. */
    double down = -f->state.v * sin(f->state.gamma) - f->lift;
    f->making = glide_step(f->state.v * cos(f->state.gamma) + f->wind, down);
}

static void start_hop(Ferry *f)
{
    int h = f->hop;
    f->strip = STRIP[h];
    f->length = STRIP_LEN;
    f->start_h = START_H[h];
    f->lift = f->wind = 0.0;
    if (h == 1 && f->route >= 0 && on(f, FE_ROUTE)) {
        f->strip = f->route == 0 ? QUARRY_STRIP : RIDGE_STRIP;
        if (f->route == 0) f->lift = QUARRY_LIFT;
        else f->start_h += RIDGE_CLIMB;
    }
    if (h >= 3 && on(f, FE_WIND)) f->wind = VALLEY_WIND;
    if (h >= 3 && !f->motor) f->start_h = WINCH_H;
    if (h == 4 && on(f, FE_DOORS)) { f->lift = SEA_LIFT; f->wind += SEA_WIND; f->length = YARD_LEN; }
    launch(f);
}

void ferry_init(Ferry *f, unsigned effects)
{
    memset(f, 0, sizeof *f);
    f->aircraft = glider_default();
    f->effects = effects;
    f->route = f->pulled = -1;
    f->motor = 1;
    f->charge = BASE_CHARGE;
    f->scene = FS_NONE;
    f->start_h = START_H[0];
    f->strip = STRIP[0];
    f->length = STRIP_LEN;
    launch(f);
    open_scene(f, FS_HANGAR);
}

static void clear_hop(Ferry *f, int made)
{
    static const int next[F_HOPS] = {FS_WEIR, FS_ORCHARD, FS_MILL, FS_DOORS, FS_COAST};
    int scene = next[f->hop];
    f->hop++;
    /* The heavy scenes and the ending get the screen to themselves: no "strip made" banner. */
    if (made && !ferry_heavy(scene) && scene != FS_COAST) raise_banner(f, FB_CLEAR);
    open_scene(f, scene);
}

/* Landed short, long, hard, or wandered too long: a farmer's trailer brings the Kestrel back to the start of the hop
 * and it is flown again from the same height; the battery is not recharged. After TRAILER_AFTER outlandings in one hop
 * the trailer takes her on to the strip by road: the hop is over, with no "strip made" banner. */
static void outland(Ferry *f)
{
    f->outlandings++;
    f->out_hop[f->hop]++;
    f->crisis_hop[f->hop] = 1;
    raise_banner(f, FB_OUT);
    if (f->out_hop[f->hop] >= TRAILER_AFTER) { f->trailered++; clear_hop(f, 0); return; }
    launch(f);
}

void ferry_tick(Ferry *f, int frames, double command_degrees, double throttle)
{
    if (frames < 0 || f->scene != FS_NONE || f->hop >= F_HOPS) return;
    if (frames > 250) frames = 250;
    if (!isfinite(command_degrees)) command_degrees = 4.0;
    if (!isfinite(throttle)) throttle = 0.0;
    double cmd = fmax(-10.0, fmin(20.0, command_degrees)) * DEG;
    throttle = fmax(0.0, fmin(1.0, throttle));
    for (int k = 0; k < frames && f->scene == FS_NONE && f->hop < F_HOPS; k++) {
        double power = f->motor && f->charge > 0.0 ? fmin(throttle, f->charge / F_DT) : 0.0;
        double x0 = f->state.x, e0 = energy_height(&f->state);
        f->command = cmd;
        f->throttle = power;
        flight_step(&f->aircraft, &f->state, cmd, power, F_DT);
        /* The air mass moves: a uniform wind and vertical air motion carry the glider with it. */
        f->state.x += f->wind * F_DT;
        f->state.h += f->lift * F_DT;
        f->charge = fmax(0.0, f->charge - power * F_DT);
        double made = glide_step(f->state.x - x0, energy_height(&f->state) - e0);
        f->making += (made - f->making) * F_DT / F_MAKING_LAG;
        f->time += F_DT;
        f->hop_time += F_DT;
        f->frames++;
        if (f->banner_left > 0.0 && (f->banner_left -= F_DT) <= 0.0) f->banner = FB_NONE;
        if (ferry_low_on(f)) { f->low += F_DT; f->crisis_hop[f->hop] = 1; }
        Status st = check_touchdown(&f->state);
        if (st == LANDED && f->state.x >= f->strip && f->state.x <= f->strip + f->length) clear_hop(f, 1);
        else if (st != FLYING || f->hop_time > HOP_LIMIT) outland(f);
    }
}

int ferry_answer(Ferry *f, int option)
{
    int scene = f->scene;
    if (scene == FS_NONE || option < 0 || option >= ferry_options(scene)) return 1;
    f->scene = FS_NONE;
    switch (scene) {
    case FS_HANGAR:
        if (on(f, FE_CELLS)) f->charge = CELLS_CHARGE;
        start_hop(f);
        break;
    case FS_WEIR:
        f->route = option;
        start_hop(f);
        break;
    case FS_ORCHARD:
        if (on(f, FE_ORCHARD)) f->charge += ORCHARD_CHARGE;
        open_scene(f, FS_BUYER);
        break;
    case FS_BUYER:
        start_hop(f);
        break;
    case FS_MILL:
        f->pulled = option;
        if (option == 0 && on(f, FE_MOTOR)) {
            f->motor = 0;
            f->charge = 0.0;
            f->aircraft.cd0 = CLEAN_CD0;
        }
        start_hop(f);
        break;
    case FS_DOORS:
        start_hop(f);
        break;
    case FS_COAST:
        break;
    default:
        return 1;
    }
    return 0;
}

double ferry_value(const Ferry *f, int field)
{
    const State *s = &f->state;
    const Aircraft *a = &f->aircraft;
    switch (field) {
    case FV_X: return s->x;
    case FV_H: return s->h;
    case FV_V: return s->v;
    case FV_GAMMA: return s->gamma;
    case FV_ALPHA: return s->alpha;
    case FV_TIME: return f->time;
    case FV_COMMAND: return f->command / DEG;
    case FV_THROTTLE: return f->throttle;
    case FV_LIFT: return lift_force(a, s);
    case FV_DRAG: return drag_force(a, s);
    case FV_LD: return lift_coeff(a, s->alpha) / drag_coeff(a, s->alpha);
    case FV_SINK: return -s->v * sin(s->gamma);
    case FV_STALL: return s->alpha > a->alpha_stall;
    case FV_HOP: return f->hop;
    case FV_SCENE: return f->scene;
    case FV_BANNER: return f->banner;
    case FV_STRIP: return f->strip;
    case FV_LENGTH: return f->length;
    case FV_START_H: return f->start_h;
    case FV_AIR_LIFT: return f->lift;
    case FV_WIND: return f->wind;
    case FV_CHARGE: return f->charge;
    case FV_MOTOR: return f->motor;
    case FV_LOW: return f->low;
    case FV_LOW_ON: return ferry_low_on(f);
    case FV_NEED: return ferry_need(f);
    case FV_OUTLANDINGS: return f->outlandings;
    case FV_ROUTE: return f->route;
    case FV_PULLED: return f->pulled;
    case FV_ENDING: return ferry_ending(f);
    case FV_LOUD: return f->loud;
    case FV_HEAVY: return ferry_heavy(f->scene);
    case FV_GROUND_SPEED: return s->v * cos(s->gamma) + f->wind;
    case FV_MAKING: return f->making;
    case FV_HOP_TIME: return f->hop_time;
    case FV_TRAILERED: return f->trailered;
    case FV_OUT_HOP: return f->hop < F_HOPS ? f->out_hop[f->hop] : 0;
    default: return 0.0;
    }
}
