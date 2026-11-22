/*
 * swarm.c - continuous-time simulation of many Firefly boards running the
 * real sync code (src/firefly.c, compiled in unchanged).
 *
 * Models what the firmware actually does:
 *   - each board wakes on its own watchdog tick, TICK_MS * (1 + drift),
 *     drift uniform in +/- DRIFT, with a random start offset;
 *   - a flash lasts FLASH_MS from the wake at which the board fires;
 *   - a board "sees light" in a tick if any visible neighbour's flash
 *     overlaps that tick (each sighting kept with probability PDET);
 *   - only a dark -> light change counts as a neighbour flash
 *     (FW-SENSE-2), exactly as main.c does it.
 *
 * Usage: swarm <topology> <nodes> <drift> <pdet> <trials> <max_s> [csv_trace]
 *   topology: all | ring | grid | line
 * Prints one CSV line per trial:
 *   topology,nodes,drift,pdet,trial,t_sync_s,steady_pct,median_spread_ms
 * A "volley" is the group of flashes around one cycle. A volley is good when
 * every board flashed in it and first-to-last spread <= the sync threshold
 * (100 ms for all-to-all, plus 1.25 ticks per extra hop of network diameter).
 *   t_sync_s   = start of the first run of 5 good volleys (-1 if never)
 *   steady_pct = % of good volleys in the second half of the trial
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/config.h"
#include "../src/firefly.h"

#define MAXN 64
#define SYNC_VOLLEYS 5

static int N, adj[MAXN][MAXN];
static double tick_s[MAXN], next_wake[MAXN], last_wake[MAXN], fire_t[MAXN];
static ff_state_t st[MAXN];
static uint8_t prev_light[MAXN];
static double DRIFT, PDET, SYNC_SPREAD_S;


static double urand(void) { return rand() / (RAND_MAX + 1.0); }

static void topology(const char *t)
{
    memset(adj, 0, sizeof adj);
    int side = (int)ceil(sqrt(N));
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            if (i == j) continue;
            if (!strcmp(t, "all")) adj[i][j] = 1;
            else if (!strcmp(t, "ring")) adj[i][j] = (abs(i - j) == 1) || (abs(i - j) == N - 1);
            else if (!strcmp(t, "line")) adj[i][j] = abs(i - j) == 1;
            else if (!strcmp(t, "grid")) {
                int xi = i % side, yi = i / side, xj = j % side, yj = j / side;
                adj[i][j] = abs(xi - xj) + abs(yi - yj) == 1;
            }
        }
}

/* run one trial */
static double spreads[20000];
static int cmpd(const void *a, const void *b) { double d = *(double *)a - *(double *)b; return d < 0 ? -1 : d > 0; }

static double trial(double max_s, double *steady_pct, double *med_spread_ms, FILE *trace)
{
    double volley_start = -1, volley_last = 0, first_sync = -1, run_start = 0;
    int seen = 0, good = 0, late_total = 0, late_good = 0, ns = 0;
    uint64_t seen_mask = 0;

    for (int i = 0; i < N; i++) {
        tick_s[i] = TICK_MS / 1000.0 * (1.0 + DRIFT * (2 * urand() - 1));
        last_wake[i] = urand() * tick_s[i];
        next_wake[i] = last_wake[i] + tick_s[i];
        fire_t[i] = -1e9;
        prev_light[i] = 1;
        ff_init(&st[i], (uint8_t)(rand() & 0xff));
    }
    for (;;) {
        int i = 0;
        for (int k = 1; k < N; k++) if (next_wake[k] < next_wake[i]) i = k;
        double now = next_wake[i];
        if (now > max_s) break;

        uint8_t light = 0;
        for (int j = 0; j < N; j++)
            if (adj[i][j] && fire_t[j] + FLASH_MS / 1000.0 > last_wake[i] && fire_t[j] <= now
                && urand() < PDET)
                light = 1;
        uint8_t fire = ff_tick(&st[i], light && !prev_light[i]);
        prev_light[i] = light;
        if (fire) {
            fire_t[i] = now;
            if (trace) fprintf(trace, "%.4f,%d\n", now, i);
            if (volley_start < 0 || now - volley_start > FF_PERIOD * TICK_MS / 2000.0) {
                if (volley_start >= 0) {             /* close the previous volley */
                    double spread = volley_last - volley_start;
                    int ok = seen == N && spread <= SYNC_SPREAD_S;
                    if (seen == N && ns < 20000) spreads[ns++] = spread;
                    if (ok) { if (good++ == 0) run_start = volley_start; }
                    else good = 0;
                    if (good >= SYNC_VOLLEYS && first_sync < 0) first_sync = run_start;
                    if (volley_start > max_s / 2) { late_total++; late_good += ok; }
                }
                volley_start = now;
                seen_mask = 0;
                seen = 0;
            }
            if (!(seen_mask & (1ull << i))) { seen_mask |= 1ull << i; seen++; }
            volley_last = now;
        }
        last_wake[i] = now;
        next_wake[i] = now + tick_s[i];
    }
    *steady_pct = late_total ? 100.0 * late_good / late_total : 0;
    if (ns) { qsort(spreads, ns, sizeof(double), cmpd); *med_spread_ms = spreads[ns / 2] * 1000; }
    else *med_spread_ms = -1;
    return first_sync;
}

int main(int argc, char **argv)
{
    if (argc < 7) {
        fprintf(stderr, "usage: swarm topology nodes drift pdet trials max_s [trace.csv]\n");
        return 2;
    }
    N = atoi(argv[2]);
    DRIFT = atof(argv[3]);
    PDET = atof(argv[4]);
    int trials = atoi(argv[5]);
    double max_s = atof(argv[6]);
    FILE *trace = argc > 7 ? fopen(argv[7], "w") : NULL;
    const char *top = argv[1];
    topology(top);
    /* all-to-all: every board sees every flash, so allow ~1 tick of reaction
     * lag plus clock spread. Multi-hop: allow one tick per hop of diameter. */
    int diam = !strcmp(top, "all") ? 1 : !strcmp(top, "ring") ? N / 2
             : !strcmp(top, "line") ? N - 1 : 2 * ((int)ceil(sqrt(N)) - 1);
    SYNC_SPREAD_S = 0.100 + (diam - 1) * TICK_MS * 1.25 / 1000.0;
    srand(12345);
    for (int t = 0; t < trials; t++) {
        double steady, med;
        double s = trial(max_s, &steady, &med, t == 0 ? trace : NULL);
        printf("%s,%d,%.2f,%.2f,%d,%.2f,%.1f,%.1f\n", top, N, DRIFT, PDET, t, s, steady, med);
    }
    if (trace) fclose(trace);
    return 0;
}
