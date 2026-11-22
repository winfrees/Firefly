/*
 * Host unit tests for the sync algorithm (src/firefly.c) and the
 * compile-time thresholds in src/config.h. Builds with plain gcc.
 * Each test names the requirement it verifies (docs/requirements.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include "../src/config.h"
#include "../src/firefly.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define TEST(name) printf("%s\n", name)

/* FW-SYNC-1: free-running period is exactly FF_PERIOD ticks, from any phase. */
static void test_free_running_period(void)
{
    TEST("UT-01 free-running period (FW-SYNC-1)");
    for (unsigned start = 0; start < FF_PERIOD; start++) {
        ff_state_t s;
        ff_init(&s, (uint8_t)start);
        int first = -1, second = -1;
        for (int t = 1; t <= (int)(3 * FF_PERIOD) && second < 0; t++)
            if (ff_tick(&s, 0)) {
                if (first < 0) first = t; else second = t;
            }
        CHECK(first == (int)(FF_PERIOD - start), "start %u: first flash at %d", start, first);
        CHECK(second - first == FF_PERIOD, "start %u: period %d", start, second - first);
    }
}

/* FW-SYNC-2: a seen flash advances the phase by phase/4 + kick (+1 tick). */
static void test_phase_jump(void)
{
    TEST("UT-02 phase response (FW-SYNC-2)");
    for (unsigned p = 0; p < FF_PERIOD; p++) {
        ff_state_t s = { (uint8_t)p, 0 };
        uint8_t fired = ff_tick(&s, 1);
        unsigned expect = p + (p >> FF_COUPLING_SHIFT) + FF_KICK + 1;
        if (expect >= FF_PERIOD)
            CHECK(fired && s.phase == 0 && s.refractory == FF_REFRACTORY,
                  "p=%u should fire now (absorption)", p);
        else
            CHECK(!fired && s.phase == expect, "p=%u -> %u, expected %u", p, s.phase, expect);
    }
}

/* FW-SYNC-3: light seen during the refractory window changes nothing. */
static void test_refractory(void)
{
    TEST("UT-03 refractory window ignores light (FW-SYNC-3)");
    ff_state_t a, b;
    ff_init(&a, FF_PERIOD - 1);
    ff_init(&b, FF_PERIOD - 1);
    CHECK(ff_tick(&a, 0) && ff_tick(&b, 0), "both should fire");
    for (unsigned t = 0; t < FF_REFRACTORY; t++) {
        ff_tick(&a, 1);                       /* light every tick */
        ff_tick(&b, 0);
        CHECK(a.phase == b.phase, "tick %u: refractory board moved (%u vs %u)", t, a.phase, b.phase);
    }
    ff_tick(&a, 1);
    ff_tick(&b, 0);
    CHECK(a.phase > b.phase, "after refractory, light must advance phase");
}

/* FW-SYNC-2 robustness: no overflow or out-of-range phase for any input. */
static void test_ranges(void)
{
    TEST("UT-04 phase always in range, any input sequence");
    srand(1);
    ff_state_t s;
    ff_init(&s, 0);
    for (long t = 0; t < 1000000; t++) {
        ff_tick(&s, (uint8_t)(rand() & 1));
        CHECK(s.phase < FF_PERIOD && s.refractory <= FF_REFRACTORY,
              "t=%ld phase %u refr %u", t, s.phase, s.refractory);
        if (failures) break;
    }
}

/* FW-SYNC-4 / ff_init: seeds map into range. FW-TAP-1: fire_now resets. */
static void test_init_and_fire_now(void)
{
    TEST("UT-05 init seeds and fire_now");
    for (unsigned seed = 0; seed < 256; seed++) {
        ff_state_t s;
        ff_init(&s, (uint8_t)seed);
        CHECK(s.phase == seed % FF_PERIOD && s.refractory == 0, "seed %u", seed);
    }
    ff_state_t s = { 40, 0 };
    ff_fire_now(&s);
    CHECK(s.phase == 0 && s.refractory == FF_REFRACTORY, "fire_now");
}

/* FW-SYNC-5: two boards with the same clock lock from every pair of start
 * phases. A board can only react on the tick after it sees a flash, so in
 * this lock-step model "locked" means: the two flashes land within one
 * tick of each other for 3 cycles running. Exhaustive: 78 x 78 pairs. */
#define FF_SYNC_MAX_CYCLES 10
#define MAXF 64
static int paired(int t, const int *other, int n)   /* other flash within 1 tick? */
{
    for (int i = 0; i < n; i++)
        if (abs(other[i] - t) <= 1) return 1;
    return 0;
}

static void test_two_board_lock_exhaustive(void)
{
    TEST("UT-06 two boards lock (<= 1 tick apart) from all 6084 start pairs (FW-SYNC-5)");
    int worst = 0, same_tick = 0, one_tick = 0;
    const int T = (int)(FF_SYNC_MAX_CYCLES * FF_PERIOD);
    for (unsigned pa = 0; pa < FF_PERIOD; pa++)
        for (unsigned pb = 0; pb < FF_PERIOD; pb++) {
            ff_state_t a, b;
            ff_init(&a, (uint8_t)pa);
            ff_init(&b, (uint8_t)pb);
            uint8_t fa = 0, fb = 0;
            int A[MAXF], B[MAXF], na_ = 0, nb_ = 0;
            for (int t = 0; t < T; t++) {
                uint8_t na = ff_tick(&a, fb);   /* each sees the other's  */
                uint8_t nb = ff_tick(&b, fa);   /* flash from last tick   */
                if (na && na_ < MAXF) A[na_++] = t;
                if (nb && nb_ < MAXF) B[nb_++] = t;
                fa = na; fb = nb;
            }
            /* locked from the first flash after which every flash of either
             * board has a partner within one tick */
            int lock = -1;
            for (int i = na_ - 1; i >= 0; i--) {
                if (!paired(A[i], B, nb_)) break;
                lock = A[i];
            }
            for (int i = nb_ - 1; i >= 0; i--) {
                if (B[i] < lock) break;
                if (!paired(B[i], A, na_)) { lock = -1; break; }
            }
            int tail = 0;                       /* need >= 3 locked cycles */
            for (int i = 0; i < na_; i++) if (lock >= 0 && A[i] >= lock) tail++;
            CHECK(lock >= 0 && tail >= 3, "pair %u,%u not locked after %d cycles", pa, pb, FF_SYNC_MAX_CYCLES);
            if (!(lock >= 0 && tail >= 3)) return;
            if (lock > worst) worst = lock;
            if (A[na_ - 1] == B[nb_ - 1]) same_tick++; else one_tick++;
        }
    printf("  all pairs locked; worst case by tick %d (%.1f cycles); %d end in the same tick, %d one tick apart\n",
           worst, (double)worst / FF_PERIOD, same_tick, one_tick);
}

/* Thresholds: in ADC range and ordered so the hysteresis bands exist. */
static void test_thresholds(void)
{
    TEST("UT-07 thresholds in range and ordered (FW-PWR-1..3)");
    printf("  ADC counts: panel night<%u day>%u, cap low<%u resume>=%u\n",
           ADC_PANEL_NIGHT, ADC_PANEL_DAY, ADC_CAP_LOW, ADC_CAP_RESUME);
    CHECK(ADC_PANEL_NIGHT < ADC_PANEL_DAY, "panel hysteresis");
    CHECK(ADC_CAP_LOW < ADC_CAP_RESUME, "cap hysteresis");
    CHECK(ADC_PANEL_DAY < 1023 && ADC_CAP_RESUME < 1023, "in range");
    /* the cap at its 5.3 V clamp must still read below full scale */
    CHECK(MV_TO_ADC(5300, CAP_NUM, CAP_DEN) < 1023, "5.3 V cap fits the ADC (%u)",
          MV_TO_ADC(5300, CAP_NUM, CAP_DEN));
    /* a 1.8 V brown-out must sit below the low-energy threshold */
    CHECK(CAP_LOW_MV > 1800 + 300, "margin above BOD");
}

int main(void)
{
    test_free_running_period();
    test_phase_jump();
    test_refractory();
    test_ranges();
    test_init_and_fire_now();
    test_two_board_lock_exhaustive();
    test_thresholds();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
