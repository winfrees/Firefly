/*
 * param_sweep.c - why the sync constants are what they are.
 *
 * For each (coupling shift, kick, refractory) in a grid, run two boards in
 * lock-step from every pair of start phases (78 x 78) and report the
 * settings for which every pair ends with flashes <= 1 tick apart.
 * With too short a refractory window, or too weak a jump, some start pairs
 * fall into a stable anti-phase pattern and never sync.
 *
 * The model below is a parameterised copy of ff_tick(); main() first checks
 * that, for the constants in config.h, it agrees with the real ff_tick() on
 * a million random inputs, so the sweep is about the code that ships.
 */
#include <stdio.h>
#include <stdlib.h>
#include "../src/config.h"
#include "../src/firefly.h"

typedef struct { int p, r; } S;
static int P = FF_PERIOD, R, SH, K;

static int tick(S *s, int ev)
{
    int p = s->p;
    if (s->r) s->r--;
    else if (ev) p = p + (p >> SH) + K;
    p++;
    if (p >= P) { s->p = 0; s->r = R; return 1; }
    s->p = p;
    return 0;
}

static int paired(int t, const int *o, int n)
{
    for (int i = 0; i < n; i++) if (abs(o[i] - t) <= 1) return 1;
    return 0;
}

/* lock tick, or -1 if the pair has not locked after `cyc` cycles */
static int run(int pa, int pb, int cyc)
{
    S a = { pa, 0 }, b = { pb, 0 };
    int fa = 0, fb = 0, A[256], B[256], na = 0, nb = 0;
    for (int t = 0; t < cyc * P; t++) {
        int xa = tick(&a, fb), xb = tick(&b, fa);
        if (xa && na < 256) A[na++] = t;
        if (xb && nb < 256) B[nb++] = t;
        fa = xa; fb = xb;
    }
    int lock = -1;
    for (int i = na - 1; i >= 0; i--) { if (!paired(A[i], B, nb)) break; lock = A[i]; }
    for (int i = nb - 1; i >= 0; i--) { if (B[i] < lock) break; if (!paired(B[i], A, na)) return -1; }
    int tail = 0;
    for (int i = 0; i < na; i++) if (lock >= 0 && A[i] >= lock) tail++;
    return lock >= 0 && tail >= 3 ? lock : -1;
}

int main(void)
{
    /* 1. the model must match the shipped ff_tick() */
    SH = FF_COUPLING_SHIFT; K = FF_KICK; R = FF_REFRACTORY;
    srand(7);
    ff_state_t real; S model;
    ff_init(&real, 0); model.p = 0; model.r = 0;
    for (long i = 0; i < 1000000; i++) {
        int ev = rand() & 1;
        int fr = ff_tick(&real, (uint8_t)ev), fm = tick(&model, ev);
        if (fr != fm || real.phase != model.p || real.refractory != model.r) {
            printf("model disagrees with ff_tick at step %ld\n", i);
            return 1;
        }
    }
    printf("model == ff_tick() for 1,000,000 random steps\n\n");

    /* 2. the sweep */
    printf("| shift | kick | refractory | all 6084 pairs lock? | worst case (cycles) |\n");
    printf("| --- | --- | --- | --- | --- |\n");
    int pass = 0, total = 0;
    for (SH = 1; SH <= 3; SH++)
        for (K = 0; K <= 8; K += 1)
            for (R = 10; R <= 40; R += 2) {
                int fail = 0, worst = 0;
                for (int pa = 0; pa < P && !fail; pa++)
                    for (int pb = 0; pb < P; pb++) {
                        int lt = run(pa, pb, 20);
                        if (lt < 0) { fail = 1; break; }
                        if (lt > worst) worst = lt;
                    }
                total++;
                int shipped = SH == FF_COUPLING_SHIFT && K == FF_KICK && R == FF_REFRACTORY;
                if (!fail) pass++;
                if (!fail || shipped) {
                    char w[16] = "-";
                    if (!fail) snprintf(w, sizeof w, "%.1f", worst / (double)P);
                    printf("| %d | %d | %d | %s | %s%s |\n", SH, K, R, fail ? "no" : "yes", w,
                           shipped ? " **(shipped)**" : "");
                }
            }
    printf("\n%d of %d settings lock from every start pair.\n", pass, total);
    return 0;
}
