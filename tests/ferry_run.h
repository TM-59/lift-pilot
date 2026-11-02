#ifndef FERRY_RUN_H
#define FERRY_RUN_H

/* One whole run with the human-like pilot; shared by the story test and bench. */
#include "ferry_policy.h"

#include <string.h>

enum { RUN_GUARD_FRAMES = 200000 };  /* 4,000 s of flying: counted as "did not finish" */

typedef struct {
    double seconds, low;
    int outlandings, completed, ending;
    int crisis[F_HOPS], failure[F_HOPS];
    int order[FS_COUNT], order_len;
    int quiet[FS_COUNT];  /* heavy scene shown with no "strip made" banner, before or after it */
} RunResult;

static const char *const SCENE_ID[FS_COUNT] = {"hangar", "weir", "orchard", "buyer", "mill", "doors", "coast"};

static inline void run_ferry(uint64_t key, unsigned effects, int route, int pulled, RunResult *out)
{
    Ferry f;
    Pilot p;
    memset(out, 0, sizeof *out);
    out->ending = -1;
    ferry_init(&f, effects);
    pilot_init(&p, key);
    int frames = 0;
    double throttle;
    while (f.hop < F_HOPS && frames < RUN_GUARD_FRAMES) {
        if (f.scene != FS_NONE) {
            int scene = f.scene, loud = f.loud, calm = f.banner == FB_NONE;
            int option = scene == FS_WEIR ? route : scene == FS_MILL ? pulled : 0;
            ferry_answer(&f, option);
            if (ferry_heavy(scene)) {
                if (f.scene == FS_NONE) {
                    double cmd = pilot_command(&p, &f, &throttle);
                    ferry_tick(&f, 1, cmd, throttle);  /* the first frame after the scene closes */
                    frames++;
                }
                out->quiet[scene] = calm && f.loud == loud && f.banner != FB_CLEAR;
            }
            continue;
        }
        double cmd = pilot_command(&p, &f, &throttle);
        ferry_tick(&f, 1, cmd, throttle);
        frames++;
    }
    out->completed = f.hop >= F_HOPS;
    out->seconds = f.time;
    out->low = f.low;
    out->outlandings = f.outlandings;
    out->ending = ferry_ending(&f);
    for (int h = 0; h < F_HOPS; h++) { out->crisis[h] = f.crisis_hop[h]; out->failure[h] = f.out_hop[h]; }
    out->order_len = f.order_len;
    memcpy(out->order, f.order, sizeof f.order);
}

/* Seeds: every row and run gets its own stirred key (no XOR or sums of raw keys). */
#define STORY_BASE 20261103ULL
enum { SALT_SEED = 1, SALT_PLAY = 2, SALT_ABLATION = 3, SALT_TENSION = 4 };
static inline uint64_t story_seed(int i) { return sub_seed(STORY_BASE, (uint64_t)i, SALT_SEED); }
static inline uint64_t play_key(int i) { return sub_seed(story_seed(i), 0, SALT_PLAY); }
static inline uint64_t ablation_key(int i) { return sub_seed(story_seed(i), 0, SALT_ABLATION); }
static inline uint64_t tension_key(int i, int run) { return sub_seed(story_seed(i), (uint64_t)run, SALT_TENSION); }

enum { PLAY_SEEDS = 12, ABLATION_SEEDS = 40, TENSION_SEEDS = 16, TENSION_RUNS = 12 };
/* Default choices for the ablation rows: round Hob Ridge, and flying on whole. */
enum { DEFAULT_ROUTE = 1, DEFAULT_PULLED = 1 };
static inline void play_choices(int i, int *route, int *pulled) { *route = i % 2; *pulled = (i / 2) % 2; }
static inline void tension_choices(int run, int *route, int *pulled) { *route = run % 2; *pulled = (run / 2) % 2; }

#endif
