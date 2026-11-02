/* "The Last Ferry" story layer: scene order, the numbers each story beat changes, quiet heavy scenes, choices,
 * and that the flight underneath is the unchanged model in flight.c. */
#include "ferry_run.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const int DESIGN[FS_COUNT] = {FS_HANGAR, FS_WEIR, FS_ORCHARD, FS_BUYER, FS_MILL, FS_DOORS, FS_COAST};
static const double DEG = 3.14159265358979 / 180.0;

static int close_to(double a, double b) { return fabs(a - b) <= 1e-9 * fmax(1.0, fabs(b)); }

/* Answer scenes with the given choices and put the glider on the strip at the end of each hop, until `target` opens.
 * Landing is done by hand (no flying) so the numbers the scenes set can be read exactly. */
static void reach(Ferry *f, int target, int route, int pulled)
{
    for (int guard = 0; f->scene != target; guard++) {
        assert(guard < 100 && f->hop < F_HOPS);
        if (f->scene != FS_NONE) {
            assert(ferry_answer(f, f->scene == FS_WEIR ? route : f->scene == FS_MILL ? pulled : 0) == 0);
            continue;
        }
        f->state.x = f->strip + 10.0;
        f->state.h = 0.001;
        f->state.gamma = -0.05;
        ferry_tick(f, 1, 4.0, 0.0);
    }
}

static char *slurp(const char *path)
{
    static char text[2][1 << 18];
    static int slot;
    FILE *fp = fopen(path, "rb");
    assert(fp);
    char *buf = text[slot++ & 1];
    size_t len = fread(buf, 1, sizeof text[0] - 1, fp);
    fclose(fp);
    buf[len] = 0;
    return buf;
}

/* Two-sided sign test; ties count in n (they are seeds where the effect did nothing). */
static double sign_p(int k, int n)
{
    double tail = 0.0;
    for (int x = k; x <= n; x++) {
        double comb = 1.0;
        for (int y = 0; y < x; y++) comb = comb * (n - y) / (y + 1);
        tail += comb;
    }
    return fmin(1.0, 2.0 * tail / pow(2.0, n));
}

/* Reads `count` rows of seconds/low after `head` (inside `from`). */
static const char *read_rows(const char *at, double v[ABLATION_SEEDS][2])
{
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        at = strstr(at, "\"seconds\": ");
        assert(at && sscanf(at, "\"seconds\": %lf, \"low\": %lf", &v[i][0], &v[i][1]) == 2);
        at++;
    }
    return at;
}

/* One metric of b against a: does b differ consistently (sign test p <= 0.05, relative difference >= 5 %)?
 * Returns +1 when b is higher, -1 when lower, 0 for no consistent difference. */
static int differs(double a[ABLATION_SEEDS][2], double b[ABLATION_SEEDS][2], int m, int *k_out)
{
    int hi = 0, lo = 0;
    double sa = 0.0, sb = 0.0;
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        hi += b[i][m] > a[i][m];
        lo += b[i][m] < a[i][m];
        sa += a[i][m];
        sb += b[i][m];
    }
    int k = hi > lo ? hi : lo;
    *k_out = k;
    double rel = fabs(sb - sa) / fmax(fabs(sa), ABLATION_SEEDS * 1.0);
    if (sign_p(k, ABLATION_SEEDS) > 0.05 || rel < 0.05) return 0;
    return hi > lo ? 1 : -1;
}

static void read_pair(const char *text, const char *section, const char *name, const char *first, const char *second,
                      double a[ABLATION_SEEDS][2], double b[ABLATION_SEEDS][2])
{
    char head[64];
    const char *at = strstr(text, section);
    assert(at);
    snprintf(head, sizeof head, "\"%s\": {", name);
    at = strstr(at, head);
    assert(at);
    at = strstr(at, first);
    assert(at);
    at = read_rows(at, a);
    at = strstr(at, second);
    assert(at);
    read_rows(at, b);
}

int main(void)
{
    /* [flight] the ferry flies the unmodified point-mass model: in still air with no story numbers, its state follows
     * flight_step exactly, frame for frame. */
    {
        Ferry f;
        ferry_init(&f, 0u);
        assert(ferry_answer(&f, 0) == 0 && f.hop == 0);
        Aircraft a = glider_default();
        State s = f.state;
        for (int i = 0; i < 1500; i++) {
            double cmd = i < 600 ? 5.5 : 3.0, power = i < 300 ? 0.2 : 0.0;
            ferry_tick(&f, 1, cmd, power);
            flight_step(&a, &s, cmd * DEG, power, 0.02);
            assert(f.state.x == s.x && f.state.h == s.h && f.state.v == s.v && f.state.gamma == s.gamma);
        }
        assert(close_to(f.charge, 5.0 - 0.2 * 300 * 0.02));
        puts("[flight] still air, no story numbers: identical to flight_step for 1,500 frames");
    }

    /* [order] the bench's play-order.json (re-checked against the code by story_bench --check): every seed meets the
     * scenes in the designed order (the promise before it is broken, the bottom before the end) and both endings are
     * reached; every heavy scene is recorded as shown quietly. */
    {
        const char *text = slurp("docs/narrative/play-order.json");
        char want[256] = "\"order\": [";
        for (int k = 0; k < FS_COUNT; k++) {
            strcat(want, k ? ", \"" : "\"");
            strcat(want, SCENE_ID[DESIGN[k]]);
            strcat(want, "\"");
        }
        strcat(want, "], \"won\": true");
        int seeds = 0, orders = 0;
        for (const char *c = text; (c = strstr(c, "\"seed\": ")); c++) seeds++;
        for (const char *c = text; (c = strstr(c, want)); c++) orders++;
        assert(seeds >= 8 && orders == seeds);
        static const char all[] = "\"quiet\": [\"buyer\", \"mill\", \"doors\"]";
        assert(strstr(text, "\"ending\": \"end-pulled\", \"quiet\": [\"buyer\", \"mill\", \"doors\"]"));
        assert(strstr(text, "\"ending\": \"end-whole\", \"quiet\": [\"buyer\", \"mill\", \"doors\"]"));
        for (const char *c = text; (c = strstr(c, "\"quiet\": [")); c++) assert(strncmp(c, all, sizeof all - 1) == 0);
        printf("[order] %d seeds: hangar .. coast in the designed order; both endings reached\n", seeds);
    }

    /* [mech] hangar: Joost's spare cells, battery 5 -> 26 s of full throttle. The first hop starts at 82 m, 2,500 m from
     * the strip. */
    {
        Ferry a, b;
        ferry_init(&a, FE_ALL); ferry_init(&b, FE_ALL & ~FE_CELLS);
        assert(a.scene == FS_HANGAR && ferry_options(FS_HANGAR) == 1 && !ferry_heavy(FS_HANGAR));
        assert(close_to(a.charge, 5.0) && close_to(b.charge, 5.0));
        assert(ferry_answer(&a, 0) == 0 && ferry_answer(&b, 0) == 0);
        assert(close_to(a.charge, 26.0) && close_to(b.charge, 5.0));
        assert(a.strip == 2500.0 && a.state.h == 82.0 && a.motor == 1);
        puts("[mech] hangar: battery 5 -> 26 s (Joost's cells)");
    }

    /* [mech] weir: across the quarry the strip is 2,500 m away and the air sinks 0.5 m/s; round Hob Ridge it is 3,500 m
     * away and the climb-out ends 15 m higher, in still air. With no route effect: 3,000 m, still air, 120 m. */
    {
        Ferry a, b, c;
        ferry_init(&a, FE_ALL); ferry_init(&b, FE_ALL); ferry_init(&c, FE_ALL & ~FE_ROUTE);
        reach(&a, FS_WEIR, 0, 1); reach(&b, FS_WEIR, 1, 1); reach(&c, FS_WEIR, 1, 1);
        assert(ferry_options(FS_WEIR) == 2 && !ferry_heavy(FS_WEIR) && a.banner == FB_CLEAR);
        assert(ferry_answer(&a, 0) == 0 && ferry_answer(&b, 1) == 0 && ferry_answer(&c, 1) == 0);
        assert(a.route == 0 && b.route == 1);
        assert(a.strip == 2500.0 && close_to(a.lift, -0.5) && a.start_h == 120.0);
        assert(b.strip == 3500.0 && b.lift == 0.0 && b.start_h == 135.0);
        assert(c.strip == 3000.0 && c.lift == 0.0 && c.start_h == 120.0);
        /* The sinking air is applied to the height after each unchanged step. */
        Aircraft ac = glider_default();
        State s = a.state;
        ferry_tick(&a, 1, 4.0, 0.0);
        flight_step(&ac, &s, 4.0 * DEG, 0.0, 0.02);
        assert(close_to(a.state.h, s.h - 0.5 * 0.02) && a.state.x == s.x);
        puts("[mech] weir: quarry 2,500 m in air sinking 0.5 m/s; ridge 3,500 m from a climb-out 15 m higher");
    }

    /* [mech] orchard: Ros's panels add 15 s of charge; then the buyer's letter opens.
     * [quiet] buyer: the orchard is a reward and keeps its "strip made" banner, but the letter that follows gets the
     * screen to itself: the banner is gone when it opens and no banner is raised after it closes. */
    {
        Ferry a, b;
        ferry_init(&a, FE_ALL); ferry_init(&b, FE_ALL & ~FE_ORCHARD);
        reach(&a, FS_ORCHARD, 1, 1); reach(&b, FS_ORCHARD, 1, 1);
        assert(a.banner == FB_CLEAR && a.banner_left > 0.0 && !ferry_heavy(FS_ORCHARD));
        double ca = a.charge, cb = b.charge;
        int loud = a.loud;
        assert(ferry_answer(&a, 0) == 0 && ferry_answer(&b, 0) == 0);
        assert(close_to(a.charge - ca, 15.0) && close_to(b.charge, cb));
        assert(a.scene == FS_BUYER && ferry_heavy(FS_BUYER) && a.banner == FB_NONE && a.banner_left == 0.0);
        assert(ferry_answer(&a, 0) == 0);
        ferry_tick(&a, 1, 4.0, 0.0);
        assert(a.loud == loud && a.banner != FB_CLEAR);
        puts("[mech] orchard: +15 s of charge");
        puts("[quiet] buyer: shown with no strip-made banner, before or after");
    }

    /* [mech] buyer: from Mill Strip on, the valley wind is 3 m/s on the nose: the ground position falls back 3 m/s
     * behind the airspeed. The hop to Mill Strip itself is still calm. */
    {
        Ferry a, b;
        ferry_init(&a, FE_ALL); ferry_init(&b, FE_ALL & ~FE_WIND);
        reach(&a, FS_MILL, 1, 1); reach(&b, FS_MILL, 1, 1);
        assert(a.hop == 3 && b.hop == 3);
        assert(ferry_answer(&a, 1) == 0 && ferry_answer(&b, 1) == 0);
        assert(close_to(a.wind, -3.0) && b.wind == 0.0);
        Aircraft ac = glider_default();
        State s = a.state;
        ferry_tick(&a, 1, 4.0, 0.0);
        flight_step(&ac, &s, 4.0 * DEG, 0.0, 0.02);
        assert(close_to(a.state.x, s.x - 3.0 * 0.02));
        Ferry c;
        ferry_init(&c, FE_ALL);
        reach(&c, FS_BUYER, 1, 1);
        assert(ferry_answer(&c, 0) == 0 && c.hop == 2 && c.wind == 0.0);
        puts("[mech] buyer: valley wind 3 m/s on the nose for the last two hops");
    }

    /* [mech] mill, both paths. Pulled: no motor (throttle does nothing), a cleaner airframe (CD0 0.014) and the mill's
     * winch launch to 145 m. Whole: the motor and the battery stay, the launch is the usual 80 m climb-out.
     * [quiet] mill: no "strip made" banner when it opens, nor after. */
    for (int pulled = 0; pulled < 2; pulled++) {
        Ferry a;
        ferry_init(&a, FE_ALL);
        reach(&a, FS_MILL, 1, pulled);
        assert(ferry_options(FS_MILL) == 2 && ferry_heavy(FS_MILL) && a.banner == FB_NONE);
        int loud = a.loud;
        double charge = a.charge;
        assert(ferry_answer(&a, pulled) == 0 && a.pulled == pulled);
        assert(a.motor == pulled && close_to(a.aircraft.cd0, pulled ? 0.015 : 0.014));
        assert(close_to(a.start_h, pulled ? 80.0 : 145.0) && close_to(a.charge, pulled ? charge : 0.0));
        double v = a.state.v;
        ferry_tick(&a, 1, 4.0, 1.0);
        assert(a.loud == loud && a.banner != FB_CLEAR);
        assert(pulled ? a.throttle == 1.0 && a.state.v > v - 0.05 : a.throttle == 0.0);
    }
    puts("[mech] mill: pulled = no motor, CD0 0.014, winch to 145 m; whole = motor, 80 m climb-out");
    puts("[quiet] mill: shown with no strip-made banner");

    /* [mech] doors: the last hop has the sea breeze (2 m/s more on the nose, air sinking 0.15 m/s) and the breaker's
     * yard strip is 250 m long, not 600 m. [quiet] doors: no banner when it opens, nor after. */
    {
        Ferry a, b;
        ferry_init(&a, FE_ALL); ferry_init(&b, FE_ALL & ~FE_DOORS);
        reach(&a, FS_DOORS, 1, 1); reach(&b, FS_DOORS, 1, 1);
        assert(ferry_heavy(FS_DOORS) && a.banner == FB_NONE && b.banner == FB_NONE);
        int loud = a.loud;
        assert(ferry_answer(&a, 0) == 0 && ferry_answer(&b, 0) == 0);
        assert(a.hop == 4 && close_to(a.wind, -5.0) && close_to(a.lift, -0.15) && a.length == 250.0);
        assert(close_to(b.wind, -3.0) && b.lift == 0.0 && b.length == 600.0);
        ferry_tick(&a, 1, 4.0, 0.0);
        assert(a.loud == loud && a.banner != FB_CLEAR);
        puts("[mech] doors: sea breeze 2 m/s more on the nose, air sinking 0.15 m/s, a 250 m yard");
        puts("[quiet] doors: shown with no strip-made banner");
    }

    /* [mech] an outlanding: short, long or hard is a failure; the hop is flown again from the same height with what is
     * left in the battery; the third outlanding in a hop ends it on a trailer, with no strip-made banner. */
    {
        Ferry a;
        ferry_init(&a, FE_ALL);
        assert(ferry_answer(&a, 0) == 0);
        double charge = a.charge;
        a.state.x = 1000.0; a.state.h = 0.001; a.state.gamma = -0.05;
        ferry_tick(&a, 1, 4.0, 0.0);
        assert(a.outlandings == 1 && a.out_hop[0] == 1 && a.hop == 0 && a.banner == FB_OUT);
        assert(a.state.x == 0.0 && a.state.h == 82.0 && close_to(a.charge, charge) && a.crisis_hop[0]);
        a.state.x = 3200.0; a.state.h = 0.001; a.state.gamma = -0.05;  /* past the strip end */
        ferry_tick(&a, 1, 4.0, 0.0);
        a.state.x = 2600.0; a.state.h = 0.001; a.state.gamma = -0.4;   /* on the strip, too steep */
        ferry_tick(&a, 1, 4.0, 0.0);
        assert(a.outlandings == 3 && a.trailered == 1 && a.hop == 1 && a.scene == FS_WEIR && a.loud == 0);
        puts("[mech] outlanding: short, long and hard landings re-fly the hop; the third ends it on a trailer");
    }

    /* [mech] in the bench: removing each story beat's numbers on its own changes an outcome in most seeds (the same
     * test the scorer applies: sign test p <= 0.05, relative difference >= 5 %). */
    {
        const char *text = slurp("docs/narrative/ablation.json");
        static double on[ABLATION_SEEDS][2], off[ABLATION_SEEDS][2];
        static const char *const beat[] = {"hangar", "orchard", "buyer", "doors"};
        /* The direction each beat's numbers should push, when removed: the rewards raise time in the red; the
         * punishments, when taken away, make the flight quicker. */
        static const int want_m[] = {1, 1, 0, 0}, want_dir[] = {1, 1, -1, -1};
        for (int b = 0; b < 4; b++) {
            read_pair(text, "\"beats\": {", beat[b], "\"on\": [", "\"off\": [", on, off);
            int k, d = differs(on, off, want_m[b], &k);
            printf("[mech] %s off - on, %s: %d/%d seeds one way\n", beat[b], want_m[b] ? "low" : "seconds", k, ABLATION_SEEDS);
            assert(d == want_dir[b]);
        }
        /* All story numbers off against on: both outcomes differ. */
        const char *at = strstr(text, "\"effects\": {");
        at = read_rows(strstr(at, "\"on\": ["), on);
        read_rows(strstr(at, "\"off\": ["), off);
        int k0, k1;
        assert(differs(on, off, 0, &k0) != 0 && differs(on, off, 1, &k1) != 0);
        printf("[mech] all story numbers off: seconds %d/%d, low %d/%d seeds one way\n", k0, ABLATION_SEEDS, k1, ABLATION_SEEDS);
    }

    /* [choice] both paths of each choice win one outcome in the bench, and neither is better or equal on both outcomes
     * in 85 % of seeds. Weir: the quarry is quicker, the ridge keeps the gauge out of the red. Mill: pulling the motor
     * keeps the gauge out of the red (the clean wing and the winch), keeping it is quicker (no long floats). */
    {
        const char *text = slurp("docs/narrative/ablation.json");
        static const char *const name[2] = {"weir", "mill"};
        for (int c = 0; c < 2; c++) {
            static double p0[ABLATION_SEEDS][2], p1[ABLATION_SEEDS][2];
            read_pair(text, "\"choices\": {", name[c], "\"0\": [", "\"1\": [", p0, p1);
            int ks, kl, ds = differs(p0, p1, 0, &ks), dl = differs(p0, p1, 1, &kl);
            /* path 1 against path 0: weir 1 (ridge) is slower and lower in the red; mill 1 (whole) is quicker and longer in the red */
            assert(c == 0 ? ds == 1 && dl == -1 : ds == -1 && dl == 1);
            int dom0 = 0, dom1 = 0;
            for (int i = 0; i < ABLATION_SEEDS; i++) {
                dom0 += p0[i][0] <= p1[i][0] && p0[i][1] <= p1[i][1];
                dom1 += p1[i][0] <= p0[i][0] && p1[i][1] <= p0[i][1];
            }
            assert(dom0 < 0.85 * ABLATION_SEEDS && dom1 < 0.85 * ABLATION_SEEDS);
            printf("[choice] %s: seconds %d/%d, low %d/%d one way; better or equal on both %d and %d of %d\n", name[c], ks,
                   ABLATION_SEEDS, kl, ABLATION_SEEDS, dom0, dom1, ABLATION_SEEDS);
        }
    }

    /* An ordinary made strip is loud, so the quiet checks above are not vacuous; out-of-range answers are refused. */
    {
        Ferry a;
        ferry_init(&a, FE_ALL);
        reach(&a, FS_WEIR, 0, 0);
        assert(a.banner == FB_CLEAR && a.loud == 1);
        assert(ferry_answer(&a, 2) == 1 && ferry_answer(&a, -1) == 1);
        ferry_tick(&a, 1, 4.0, 0.0);
        assert(a.scene == FS_WEIR);
        assert(ferry_ending(&a) == -1);
    }
    puts("ferry tests passed");
    return 0;
}
