/* Story bench: flies "The Last Ferry" with a human-like pilot and writes
 *   docs/narrative/play-order.json  scenes in the order they opened, per seed
 *   docs/narrative/ablation.json    story numbers on/off, one beat off, each choice path
 *   docs/narrative/tension.json     per-hop crises and outlandings, one row per run
 * Usage: story_bench <dir>          write all three files (make story)
 *        story_bench --check <dir>  re-run a few rows and require them verbatim in the files (make test)
 * Every run is deterministic. */
#include "ferry_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { uint64_t key; unsigned effects; int route, pulled; } Job;

static RunResult run_job(Job j)
{
    RunResult r;
    run_ferry(j.key, j.effects, j.route, j.pulled, &r);
    return r;
}

/* ------------------------------------------------------------------ rows */
static const char *ending_id(int e) { return e == 0 ? "end-pulled" : e == 1 ? "end-whole" : "none"; }

static void play_row(char *buf, size_t cap, int i, const RunResult *r)
{
    int route, pulled;
    play_choices(i, &route, &pulled);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"choices\": {\"weir\": %d, \"mill\": %d}, \"order\": [", i, route, pulled);
    for (int k = 0; k < r->order_len; k++) n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", k ? ", " : "", SCENE_ID[r->order[k]]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"won\": %s, \"ending\": \"%s\", \"quiet\": [", r->completed ? "true" : "false", ending_id(r->ending));
    int first = 1;
    for (int k = 0; k < r->order_len; k++) {
        int s = r->order[k];
        if (ferry_heavy(s) && r->quiet[s]) { n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", SCENE_ID[s]); first = 0; }
    }
    snprintf(buf + n, cap - n, "]}");
}

static void ablation_row(char *buf, size_t cap, int i, const RunResult *r)
{
    snprintf(buf, cap, "{\"seed\": \"%d\", \"seconds\": %.2f, \"low\": %.2f, \"outlandings\": %d}", i, r->seconds, r->low, r->outlandings);
}

static void tension_row(char *buf, size_t cap, int i, int run, const RunResult *r)
{
    int route, pulled;
    tension_choices(run, &route, &pulled);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"run\": %d, \"path\": {\"weir\": \"%d\", \"mill\": \"%d\"}, \"completed\": %s, \"crisis\": [",
                                i, run, route, pulled, r->completed ? "true" : "false");
    for (int k = 0; k < F_HOPS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k] > 0 ? r->failure[k] : r->crisis[k]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"failure\": [");
    for (int k = 0; k < F_HOPS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k]);
    snprintf(buf + n, cap - n, "]}");
}

/* ------------------------------------------------------------------ arms */
enum { ARM_ON, ARM_OFF, ARM_CELLS, ARM_ORCHARD, ARM_WIND, ARM_DOORS, ARM_QUARRY, ARM_PULL, ARMS };
static const unsigned ARM_EFFECTS[ARMS] = {
    FE_ALL, 0u, FE_ALL & ~FE_CELLS, FE_ALL & ~FE_ORCHARD, FE_ALL & ~FE_WIND, FE_ALL & ~FE_DOORS, FE_ALL, FE_ALL};
static const int ARM_ROUTE[ARMS] = {DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, 0, DEFAULT_ROUTE};
static const int ARM_PULLED[ARMS] = {DEFAULT_PULLED, DEFAULT_PULLED, DEFAULT_PULLED, DEFAULT_PULLED, DEFAULT_PULLED, DEFAULT_PULLED, DEFAULT_PULLED, 0};

static Job ablation_job(int arm, int i) { return (Job){ablation_key(i), ARM_EFFECTS[arm], ARM_ROUTE[arm], ARM_PULLED[arm]}; }
static Job play_job(int i) { int r, t; play_choices(i, &r, &t); return (Job){play_key(i), FE_ALL, r, t}; }
static Job tension_job(int i, int run) { int r, t; tension_choices(run, &r, &t); return (Job){tension_key(i, run), FE_ALL, r, t}; }

/* ------------------------------------------------------------------ full output */
static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    return f;
}

static void write_rows(FILE *f, const char *indent, const RunResult *res, int arm)
{
    char row[256];
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        ablation_row(row, sizeof row, i, &res[arm * ABLATION_SEEDS + i]);
        fprintf(f, "%s%s%s\n", indent, row, i + 1 < ABLATION_SEEDS ? "," : "");
    }
}

static void write_arm_pair(FILE *f, const char *name, const RunResult *res, int off, int last)
{
    fprintf(f, "    \"%s\": {\n      \"on\": [\n", name);
    write_rows(f, "        ", res, ARM_ON);
    fprintf(f, "      ],\n      \"off\": [\n");
    write_rows(f, "        ", res, off);
    fprintf(f, "      ]\n    }%s\n", last ? "" : ",");
}

static const char *GEN = "tests/story_bench.c (make story)";
static const char *POLICY =
    "A pilot who reads the instruments on screen, not the code: it glances at the glide-needed gauge (distance to the middle "
    "of the strip over height) every 0.2-0.6 s. Above 23-28 it runs the motor at the best-glide angle until the gauge reads "
    "2-7 lower or the battery is flat (with no motor it stretches the glide); below 13-21 it lowers the nose to 0-2.5 deg to "
    "lose height, and over the strip it keeps the nose down; otherwise it holds a cruise angle of 3-7 deg. Below 3-12 m it "
    "flares to 6-12 deg. Its hand comes from the run's own stirred seed, re-drawn for every hop and every relaunch. Choices "
    "follow the path field.";

static void summary(const RunResult *res, int np, int na, int nt);

static int write_all(const char *dir)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = calloc((size_t)(np + na + nt), sizeof *res);
    if (!res) return 1;
    for (int i = 0; i < np; i++) res[i] = run_job(play_job(i));
    for (int a = 0; a < ARMS; a++)
        for (int i = 0; i < ABLATION_SEEDS; i++) res[np + a * ABLATION_SEEDS + i] = run_job(ablation_job(a, i));
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int k = 0; k < TENSION_RUNS; k++) res[np + na + i * TENSION_RUNS + k] = run_job(tension_job(i, k));
    char row[1024];

    FILE *f = open_out(dir, "play-order.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Scenes open at fixed progress points: the start of the run and the end of a hop. "
               "weir 0 = across the quarry, 1 = round Hob Ridge; mill 0 = the motor pulled, 1 = flown on whole. quiet lists the heavy scenes "
               "shown with no 'strip made' banner raised before the next frame of flight. Do not edit by hand.\",\n  \"seeds\": [\n", GEN);
    for (int i = 0; i < np; i++) { play_row(row, sizeof row, i, &res[i]); fprintf(f, "    %s%s\n", row, i + 1 < np ? "," : ""); }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    const RunResult *ab = res + np;
    f = open_out(dir, "ablation.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Same %d seeds in every arm. on = all story numbers, round Hob Ridge and flying on whole; "
               "off = scenes still open but every story number is neutral; beats = only that scene's numbers removed; choices = one choice fixed per path, "
               "the other at its default. outlandings is printed for reading, not a metric. Do not edit by hand.\",\n", GEN, ABLATION_SEEDS);
    fprintf(f, "  \"metrics\": {\n"
               "    \"seconds\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"s on the flight clock at the coast; every second is flown (a hop that ends in an outlanding is flown again from the top of the climb, no flat additions)\"},\n"
               "    \"low\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"s flown with the LOW gauge red: the glide needed to reach the middle of the strip is steeper than 1:25\"}\n"
               "  },\n  \"effects\": {\n    \"on\": [\n");
    write_rows(f, "      ", ab, ARM_ON);
    fprintf(f, "    ],\n    \"off\": [\n");
    write_rows(f, "      ", ab, ARM_OFF);
    fprintf(f, "    ]\n  },\n  \"beats\": {\n");
    write_arm_pair(f, "hangar", ab, ARM_CELLS, 0);
    write_arm_pair(f, "orchard", ab, ARM_ORCHARD, 0);
    write_arm_pair(f, "buyer", ab, ARM_WIND, 0);
    write_arm_pair(f, "doors", ab, ARM_DOORS, 1);
    fprintf(f, "  },\n  \"choices\": {\n    \"weir\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_QUARRY);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }},\n    \"mill\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_PULL);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }}\n  }\n}\n");
    fclose(f);

    const RunResult *tn = res + np + na;
    f = open_out(dir, "tension.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"policy\": \"%s\",\n", GEN, POLICY);
    fprintf(f, "  \"crisis\": \"the LOW gauge red (glide needed steeper than 1:25) at any moment in the hop, or an outlanding (an outlanding counts as a crisis)\",\n"
               "  \"failure\": \"an outlanding: the Kestrel touched down short of the strip, past its end or too hard (sink 2 m/s or more, path steeper than 0.15 rad), "
               "or was still in the air after 360 s; a farmer's trailer brings her back and the hop is flown again\",\n"
               "  \"segments\": [\"Upper Field to Weir Strip\", \"to Orchard (quarry or ridge)\", \"Orchard to Mill Strip\", \"Mill to Ferry Point, in the valley wind\", \"to the breaker's yard on the coast\"],\n"
               "  \"note\": \"%d seeds x %d runs; every run has its own stirred seed; the path comes from the run number.\",\n  \"seeds\": [\n",
            TENSION_SEEDS, TENSION_RUNS);
    for (int i = 0; i < nt; i++) {
        tension_row(row, sizeof row, i / TENSION_RUNS, i % TENSION_RUNS, &tn[i]);
        fprintf(f, "    %s%s\n", row, i + 1 < nt ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    summary(res, np, na, nt);
    free(res);
    return 0;
}


/* ------------------------------------------------------------------ summary (printed by make story) */
static double sign_p2(int k, int n)
{
    double tail = 0.0;
    for (int x = k; x <= n; x++) {
        double comb = 1.0;
        for (int y = 0; y < x; y++) comb = comb * (n - y) / (y + 1);
        tail += comb;
    }
    return fmin(1.0, 2.0 * tail / pow(2.0, n));
}

static void compare(const char *name, const RunResult *a, const RunResult *b)
{
    /* b - a per metric */
    for (int m = 0; m < 3; m++) {
        int hi = 0, lo = 0;
        double sa = 0, sb = 0;
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            double x = m == 0 ? a[i].seconds : m == 1 ? a[i].low : a[i].outlandings;
            double y = m == 0 ? b[i].seconds : m == 1 ? b[i].low : b[i].outlandings;
            hi += y > x; lo += y < x; sa += x; sb += y;
        }
        printf("  %-8s %-11s second higher %2d lower %2d  means %8.2f -> %8.2f  p %.4f  rel %+.1f%%\n", name,
               m == 0 ? "seconds" : m == 1 ? "low" : "outlandings", hi, lo, sa / ABLATION_SEEDS, sb / ABLATION_SEEDS,
               sign_p2(hi > lo ? hi : lo, ABLATION_SEEDS), 100.0 * (sb - sa) / fmax(sa, ABLATION_SEEDS));
    }
}

static void summary(const RunResult *res, int np, int na, int nt)
{
    const RunResult *ab = res + np, *tn = res + np + na;
    printf("story bench: %d runs\n", np + na + nt);
    static const char *const arm[ARMS] = {"on", "off", "cells", "orchard", "wind", "doors", "quarry", "pull"};
    for (int a = 1; a < ARMS; a++) compare(arm[a], ab, ab + a * ABLATION_SEEDS);
    int nofail = 0, stuck = 0, crisis[F_HOPS] = {0}, fail[F_HOPS] = {0}, late = 0;
    for (int i = 0; i < nt; i++) {
        int f = 0;
        for (int h = 0; h < F_HOPS; h++) { crisis[h] += tn[i].crisis[h] || tn[i].failure[h]; fail[h] += tn[i].failure[h] > 0; f += tn[i].failure[h]; }
        nofail += f == 0;
        stuck += !tn[i].completed;
        late += tn[i].crisis[3] || tn[i].crisis[4] || tn[i].failure[3] || tn[i].failure[4];
    }
    printf("tension %d runs: no failure %d (%.0f%%), stuck %d, late crisis %.0f%%\n  crisis per hop:", nt, nofail, 100.0 * nofail / nt, stuck, 100.0 * late / nt);
    for (int h = 0; h < F_HOPS; h++) printf(" %.2f", (double)crisis[h] / nt);
    printf("\n  failure per hop:");
    for (int h = 0; h < F_HOPS; h++) printf(" %.2f", (double)fail[h] / nt);
    printf("\n");
}

/* ------------------------------------------------------------------ check mode */
static char *slurp(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "missing %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) exit(1);
    buf[size] = 0;
    fclose(f);
    return buf;
}

static int cmp_key(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static int check(const char *dir)
{
    int failures = 0;
    /* [seed] every pilot key in the bench is distinct (paired arms share a seed by design). */
    int nk = PLAY_SEEDS + ABLATION_SEEDS + TENSION_SEEDS * TENSION_RUNS, k = 0;
    uint64_t *keys = malloc(sizeof *keys * (size_t)nk);
    for (int i = 0; i < PLAY_SEEDS; i++) keys[k++] = play_key(i);
    for (int i = 0; i < ABLATION_SEEDS; i++) keys[k++] = ablation_key(i);
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int r = 0; r < TENSION_RUNS; r++) keys[k++] = tension_key(i, r);
    qsort(keys, (size_t)nk, sizeof *keys, cmp_key);
    for (int i = 1; i < nk; i++) if (keys[i] == keys[i - 1]) { fputs("[seed] duplicate run key\n", stderr); failures++; }
    free(keys);

    enum { CHECKS = 8 };
    Job jobs[CHECKS] = {play_job(0), play_job(PLAY_SEEDS - 1), ablation_job(ARM_ON, 0), ablation_job(ARM_OFF, 0),
                        ablation_job(ARM_QUARRY, ABLATION_SEEDS - 1), ablation_job(ARM_PULL, ABLATION_SEEDS - 1),
                        tension_job(0, 0), tension_job(TENSION_SEEDS - 1, TENSION_RUNS - 1)};
    char *play = slurp(dir, "play-order.json"), *abl = slurp(dir, "ablation.json"), *ten = slurp(dir, "tension.json");
    char row[1024];
    /* How many times each re-run row must appear: the default arm is reused by every beat and as one path of each choice. */
    static const int copies[CHECKS] = {1, 1, 7, 1, 1, 1, 1, 1};
    for (int i = 0; i < CHECKS; i++) {
        RunResult r = run_job(jobs[i]);
        const char *hay, *file;
        if (i < 2) { play_row(row, sizeof row, i ? PLAY_SEEDS - 1 : 0, &r); hay = play; file = "play-order"; }
        else if (i < 6) { ablation_row(row, sizeof row, i < 4 ? 0 : ABLATION_SEEDS - 1, &r); hay = abl; file = "ablation"; }
        else { tension_row(row, sizeof row, i == 6 ? 0 : TENSION_SEEDS - 1, i == 6 ? 0 : TENSION_RUNS - 1, &r); hay = ten; file = "tension"; }
        int found = 0;
        for (const char *c = hay; (c = strstr(c, row)); c++) found++;
        if (found != copies[i]) {
            fprintf(stderr, "[bench] %s.json does not match the code (%d of %d copies): %s\n", file, found, copies[i], row);
            failures++;
        }
    }
    free(play); free(abl); free(ten);
    if (failures) { fprintf(stderr, "story bench check: %d failure(s); run make story to regenerate\n", failures); return 1; }
    puts("story bench check: 8 re-run rows match docs/narrative; all run seeds distinct");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--check") == 0) return check(argv[2]);
    if (argc == 2) return write_all(argv[1]);
    fputs("usage: story_bench <dir> | story_bench --check <dir>\n", stderr);
    return 2;
}
