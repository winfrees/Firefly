/*
 * fwsim.c - runs the real firmware binary (build/firefly.elf) on simavr's
 * ATtiny13A model and checks its behaviour against the requirements.
 *
 * The harness plays the outside world:
 *   - panel and supercap voltages on ADC1 / ADC3 (millivolts at the node,
 *     scaled by the board's dividers),
 *   - a neighbour's flash: while "light" is on, the floating LED cathode
 *     (PB1, input) is pulled low, which is what photocurrent does,
 *   - the tap switch on PB4.
 * It records every flash (PB0 high) and checks timing, and measures stack
 * depth and the fraction of time the CPU is awake.
 *
 * Usage: fwsim firefly.elf [report.md] [static_ram_bytes]
 * Exit code = number of failed checks.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_adc.h>
#include <simavr/sim_cycle_timers.h>

#define F_CPU     1200000.0
#define PINB_A    0x36
#define DDRB_A    0x37
#define PORTB_A   0x38
#define SPL_A     0x5D
#define RAMEND    0x9F
static int STATIC_RAM_TOP = 0x60;   /* set from argv: 0x60 + .data + .bss */

/* board constants: must match src/config.h */
#define TICK_S    0.032
#define PERIOD    78
#define CYCLE_S   (PERIOD * TICK_S)
#define FLASH_S   0.015

static const char *elf_path;
static int failures, checks;
static FILE *report;

#define CHECK(cond, ...) do { checks++; char _m[256]; snprintf(_m, sizeof _m, __VA_ARGS__); \
    printf("    [%s] %s\n", (cond) ? "pass" : "FAIL", _m); \
    if (report) fprintf(report, "| %s | %s | %s |\n", cur_test, _m, (cond) ? "pass" : "**FAIL**"); \
    if (!(cond)) failures++; } while (0)

static const char *cur_test;

/* ---- one simulated board ------------------------------------------- */
typedef struct {
    avr_t *avr;
    avr_irq_t *pb0, *pb1, *pb4, *adc_panel, *adc_cap;
    double flash_on[4096], flash_off[4096];
    int nflash;
    int pb0_state;
    uint64_t run_cycles, sleep_cycles;
    uint8_t min_sp;
    int panel_mv, cap_mv;
    int tap_level;
    int charged;
    uint8_t ext;
    double meter_from;                          /* awake-time metering starts here */
} board_t;

static double now(board_t *b) { return b->avr->cycle / F_CPU; }

static void pb0_cb(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    board_t *b = param;
    if (value && !b->pb0_state && b->nflash < 4096)
        b->flash_on[b->nflash] = now(b), b->flash_off[b->nflash] = -1;
    if (!value && b->pb0_state && b->nflash < 4096)
        b->flash_off[b->nflash++] = now(b);
    b->pb0_state = value != 0;
}

static void set_panel(board_t *b, int mv)
{   /* PB2 sees the panel through 100k / 22k */
    if (mv == b->panel_mv) return;
    b->panel_mv = mv;
    avr_raise_irq(b->adc_panel, (uint32_t)(mv * 22 / 122));
}

static void set_cap(board_t *b, int mv)
{   /* PB3 sees the supercap through 3.9M / 1M */
    if (mv == b->cap_mv) return;
    b->cap_mv = mv;
    avr_raise_irq(b->adc_cap, (uint32_t)(mv * 10 / 49));
}

/* simavr's default sleep hook waits in real time; we want simulated time only */
static void no_wait(avr_t *avr, avr_cycle_count_t how_long) { (void)avr; (void)how_long; }

/* A 1 ms heartbeat: simavr would otherwise skip a whole 32 ms sleep in one
 * step, and a 15 ms neighbour flash inside it would never reach the pin. */
static avr_cycle_count_t heartbeat(avr_t *avr, avr_cycle_count_t when, void *param)
{
    (void)param;
    return when + avr->frequency / 1000;
}

static board_t *board_new(int panel_mv, int cap_mv)
{
    static elf_firmware_t fw;
    static int loaded;
    if (!loaded) {
        if (elf_read_firmware(elf_path, &fw)) { fprintf(stderr, "cannot read %s\n", elf_path); exit(99); }
        loaded = 1;
    }
    board_t *b = calloc(1, sizeof *b);
    b->avr = avr_make_mcu_by_name("attiny13a");
    avr_init(b->avr);
    b->avr->frequency = (uint32_t)F_CPU;
    b->avr->vcc = b->avr->avcc = 3500;
    b->avr->log = LOG_NONE;
    avr_load_firmware(b->avr, &fw);
    b->avr->sleep = no_wait;
    avr_cycle_timer_register_usec(b->avr, 1000, heartbeat, b);
    b->avr->frequency = (uint32_t)F_CPU;
    b->pb0 = avr_io_getirq(b->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0);
    b->pb1 = avr_io_getirq(b->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 1);
    b->pb4 = avr_io_getirq(b->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4);
    b->adc_panel = avr_io_getirq(b->avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC1);
    b->adc_cap = avr_io_getirq(b->avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC3);
    avr_irq_register_notify(b->pb0, pb0_cb, b);
    b->panel_mv = b->cap_mv = -1;
    set_panel(b, panel_mv);
    set_cap(b, cap_mv);
    b->min_sp = 0xff;
    b->meter_from = 1.0;                        /* skip the boot flashes */
    b->ext = 0xff;                              /* force the first update */
    return b;
}

static void board_free(board_t *b) { avr_terminate(b->avr); free(b); }

/* environment callback: called before every step with the board and time */
typedef void (*env_fn)(board_t *b, double t, int *light, int *tap);

/* advance one board by one simulator step, given what the world looks like */
static int step(board_t *b, int light, int tap)
{
    uint8_t ddr = b->avr->data[DDRB_A], pin = b->avr->data[PINB_A];
    uint8_t port = b->avr->data[PORTB_A];
    /* LED junction model: driven high = charged; once floating it keeps
     * its charge until light discharges it. The tap switch pulls PB4 low.
     * Both are given to simavr as the "external" level of input pins, so
     * port writes don't fight the model. */
    if (ddr & 2)
        b->charged = (port & 2) != 0;
    else if (light)
        b->charged = 0;
    uint8_t ext = (uint8_t)((b->charged ? 0x02 : 0) | (tap ? 0 : 0x10));
    if (ext != b->ext) {
        avr_ioport_external_t e = { .name = 'B', .mask = 0x12, .value = ext };
        avr_ioctl(b->avr, AVR_IOCTL_IOPORT_SET_EXTERNAL('B'), &e);
        if (!(ddr & 0x02) && ((ext ^ pin) & 0x02)) avr_raise_irq(b->pb1, (ext >> 1) & 1);
        if (!(ddr & 0x10) && ((ext ^ pin) & 0x10)) avr_raise_irq(b->pb4, (ext >> 4) & 1);
        b->ext = ext;
    }
    uint64_t c0 = b->avr->cycle;
    int st = avr_run(b->avr);
    uint64_t dc = b->avr->cycle - c0;
    /* simavr books the whole sleep in the call that executes SLEEP */
    int sleeping = b->avr->state == cpu_Sleeping;
    if (now(b) >= b->meter_from) {
        if (sleeping) b->sleep_cycles += dc; else b->run_cycles += dc;
    }
    uint8_t sp = b->avr->data[SPL_A];
    if (b->avr->state == cpu_Running && sp >= 0x60 && sp < b->min_sp) b->min_sp = sp;
    if (st == cpu_Done || st == cpu_Crashed) { printf("    simulator stopped (%d)\n", st); return 0; }
    return 1;
}

static void run(board_t *b, double until, env_fn env)
{
    while (now(b) < until) {
        int light = 0, tap = 0;
        if (env) env(b, now(b), &light, &tap);
        if (!step(b, light, tap)) break;
    }
}

/* two boards that see each other's LED; always step the one that is behind */
static void run_pair(board_t *a, board_t *b, double until)
{
    while (now(a) < until || now(b) < until) {
        board_t *x = now(a) <= now(b) ? a : b, *y = x == a ? b : a;
        if (!step(x, y->pb0_state, 0)) break;
    }
}

static int flashes_between(board_t *b, double t0, double t1)
{
    int n = 0;
    for (int i = 0; i < b->nflash; i++) if (b->flash_on[i] >= t0 && b->flash_on[i] < t1) n++;
    return n;
}

static double first_flash_after(board_t *b, double t0)
{
    for (int i = 0; i < b->nflash; i++) if (b->flash_on[i] >= t0) return b->flash_on[i];
    return -1;
}

/* ---- environments --------------------------------------------------- */
static double g_light_period, g_light_phase, g_tap_t[4];
static int g_ntap;

static void env_dark(board_t *b, double t, int *l, int *tap) { (void)b; (void)t; (void)l; (void)tap; }

static void env_dusk_at_20(board_t *b, double t, int *l, int *tap)
{ (void)l; (void)tap; set_panel(b, t < 20 ? 4000 : 0); }

static void env_dawn_at_20(board_t *b, double t, int *l, int *tap)
{ (void)l; (void)tap; set_panel(b, t < 20 ? 0 : 4000); }

static void env_cap_recovers_at_30(board_t *b, double t, int *l, int *tap)
{ (void)l; (void)tap; set_cap(b, t < 30 ? 2100 : 3000); }

static void env_cap_sags_at_20(board_t *b, double t, int *l, int *tap)
{ (void)l; (void)tap; set_cap(b, t < 20 ? 3500 : 2200); }

static void env_neighbour(board_t *b, double t, int *l, int *tap)
{   /* a neighbour flashing for FLASH_S every g_light_period, from g_light_phase */
    (void)b; (void)tap;
    if (t < g_light_phase) return;
    double x = fmod(t - g_light_phase, g_light_period);
    *l = x < FLASH_S;
}

static void env_streetlight(board_t *b, double t, int *l, int *tap) { (void)b; (void)t; (void)tap; *l = 1; }

static void env_taps(board_t *b, double t, int *l, int *tap)
{
    (void)b; (void)l;
    for (int i = 0; i < g_ntap; i++) if (t >= g_tap_t[i] && t < g_tap_t[i] + 0.050) *tap = 1;
}

/* ---- tests ---------------------------------------------------------- */
static void t_boot_and_free_run(void)
{
    cur_test = "IT-01 boot + free-running flash at night";
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 4000);
    run(b, 60, env_dark);
    CHECK(b->nflash >= 2 && b->flash_on[1] - b->flash_on[0] < 0.25,
          "2 boot flashes within 0.25 s (got %d flashes, gap %.3f s)", b->nflash,
          b->nflash > 1 ? b->flash_on[1] - b->flash_on[0] : -1);
    double mn = 1e9, mx = 0, wmin = 1e9, wmax = 0;
    for (int i = 3; i < b->nflash; i++) {
        double p = b->flash_on[i] - b->flash_on[i - 1];
        if (p < mn) mn = p;
        if (p > mx) mx = p;
    }
    for (int i = 2; i < b->nflash; i++) {
        double w = b->flash_off[i] - b->flash_on[i];
        if (w < wmin) wmin = w;
        if (w > wmax) wmax = w;
    }
    CHECK(fabs(mn - CYCLE_S) < 0.010 && fabs(mx - CYCLE_S) < 0.010,
          "period %.3f..%.3f s, expected %.3f s (78 ticks x 32 ms)", mn, mx, CYCLE_S);
    CHECK(wmin > 0.0135 && wmax < 0.0170, "flash width %.1f..%.1f ms, expected 15 ms", wmin * 1e3, wmax * 1e3);
    double duty = 100.0 * b->run_cycles / (double)(b->run_cycles + b->sleep_cycles);
    /* FW-PWR-5: at ~0.4 mA active, 2 % awake is ~8 uA average */
    CHECK(duty < 2.0, "CPU awake %.3f %% of the time at night incl. 15 ms flashes (budget 2 %%)", duty);
    CHECK(b->min_sp > STATIC_RAM_TOP + 8, "stack low-water 0x%02X: %d bytes used, %d bytes spare",
          b->min_sp, RAMEND - b->min_sp, b->min_sp - STATIC_RAM_TOP);
    printf("    (night: awake %.3f %%, %d flashes in 60 s)\n", duty, b->nflash);
    board_free(b);
}

static void t_day_is_quiet(void)
{
    cur_test = "IT-02 daylight: no flashing";
    printf("%s\n", cur_test);
    board_t *b = board_new(4000, 4000);
    run(b, 60, env_dark);
    CHECK(flashes_between(b, 1.0, 60) == 0, "0 flashes in 59 s of daylight (got %d)", flashes_between(b, 1.0, 60));
    double duty = 100.0 * b->run_cycles / (double)(b->run_cycles + b->sleep_cycles);
    CHECK(duty < 0.05, "CPU awake %.4f %% by day, after boot (target < 0.05 %%)", duty);
    board_free(b);
}

static void t_dusk(void)
{
    cur_test = "IT-03 dusk: starts flashing";
    printf("%s\n", cur_test);
    board_t *b = board_new(4000, 4000);
    run(b, 40, env_dusk_at_20);
    double f = first_flash_after(b, 20);
    CHECK(flashes_between(b, 1, 20) == 0, "quiet before dusk");
    /* simavr 1.6 applies a new watchdog period only after the pending 8.2 s
     * timeout; hardware restarts at once (see verification report, SIM-L2).
     * Limit: up to two 8.2 s periods + one cycle in simulation. */
    CHECK(f > 0 && f - 20 < 2 * 8.2 + CYCLE_S, "first flash %.2f s after dusk (sim limit: 2 x 8.2 s + one cycle)", f - 20);
    board_free(b);
}

static void t_dawn(void)
{
    cur_test = "IT-04 dawn: stops flashing";
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 4000);
    run(b, 40, env_dawn_at_20);
    CHECK(flashes_between(b, 5, 20) >= 5, "flashing before dawn (%d)", flashes_between(b, 5, 20));
    CHECK(flashes_between(b, 21.5, 40) == 0, "no flashes from 1.5 s after dawn (got %d)", flashes_between(b, 21.5, 40));
    board_free(b);
}

static void t_low_energy(void)
{
    cur_test = "IT-05 low supercap: silent until recovered";
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 2100);
    run(b, 50, env_cap_recovers_at_30);
    CHECK(flashes_between(b, 0, 30) == 0, "no flashes (not even boot) at 2.1 V (got %d)", flashes_between(b, 0, 30));
    double f = first_flash_after(b, 30);
    CHECK(f > 0 && f - 30 < 2 * 8.2 + CYCLE_S, "flashing resumes %.2f s after cap reaches 3.0 V (sim limit: 2 x 8.2 s + one cycle)", f - 30);
    board_free(b);

    cur_test = "IT-06 supercap sags at night: stops";
    printf("%s\n", cur_test);
    b = board_new(0, 3500);
    run(b, 40, env_cap_sags_at_20);
    CHECK(flashes_between(b, 21.5, 40) == 0, "no flashes from 1.5 s after the cap falls to 2.2 V (got %d)",
          flashes_between(b, 21.5, 40));
    board_free(b);

    cur_test = "IT-07 hysteresis: 2.5 V boot stays silent";
    printf("%s\n", cur_test);
    b = board_new(0, 2500);
    run(b, 30, env_dark);
    CHECK(b->nflash == 0, "2.5 V is between 2.3 V and 2.7 V: no flashes after a cold start (got %d)", b->nflash);
    board_free(b);
}

static void t_entrainment(double period, const char *name)
{
    cur_test = name;
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 4000);
    g_light_period = period;
    g_light_phase = 3.0;
    run(b, 90, env_neighbour);
    int matched = 0, total = 0;
    double worst = 0;
    for (int i = 0; i < b->nflash; i++) {
        if (b->flash_on[i] < 60) continue;
        total++;
        double since = fmod(b->flash_on[i] - g_light_phase, period);   /* time after neighbour flash */
        if (since > period / 2) since -= period;
        if (since >= -0.001 && since <= TICK_S * 1.2) matched++;
        if (fabs(since) > worst) worst = fabs(since);
    }
    CHECK(total > 0 && matched == total,
          "neighbour every %.2f s: last %d flashes all within one tick after it (%d matched, worst %.0f ms)",
          period, total, matched, worst * 1e3);
    board_free(b);
}

static void t_refractory(void)
{
    cur_test = "IT-10 light right after own flash is ignored";
    printf("%s\n", cur_test);
    /* free-run reference */
    board_t *a = board_new(0, 4000);
    run(a, 20, env_dark);
    /* same board, with a pulse 0.2 s after every own flash: phase must not move */
    board_t *b = board_new(0, 4000);
    g_light_period = CYCLE_S;
    g_light_phase = a->flash_on[3] + 0.2;
    run(b, 20, env_neighbour);
    int same = a->nflash == b->nflash;
    for (int i = 0; same && i < a->nflash; i++) same = fabs(a->flash_on[i] - b->flash_on[i]) < 0.001;
    CHECK(same, "flash times identical to free-running (%d vs %d flashes)", a->nflash, b->nflash);
    board_free(a);
    board_free(b);
}

static void t_streetlight(void)
{
    cur_test = "IT-11 steady light (streetlight) does not cause runaway";
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 4000);
    run(b, 30, env_streetlight);
    double mn = 1e9;
    for (int i = 3; i < b->nflash; i++) {
        double p = b->flash_on[i] - b->flash_on[i - 1];
        if (p < mn) mn = p;
    }
    CHECK(fabs(mn - CYCLE_S) < 0.010, "still flashes at the nominal period under constant light (min %.3f s)", mn);
    board_free(b);
}

static void t_tap(void)
{
    cur_test = "IT-12 tap switch: immediate flash, rattles held off";
    printf("%s\n", cur_test);
    board_t *b = board_new(0, 4000);
    g_ntap = 2;
    g_tap_t[0] = 10.30;
    g_tap_t[1] = 10.60;                     /* rattle 0.3 s later */
    run(b, 12, env_taps);
    double f = first_flash_after(b, 10.30);
    CHECK(f > 0 && f - 10.30 < TICK_S * 1.1, "flash %.0f ms after tap (limit one tick)", (f - 10.30) * 1e3);
    CHECK(flashes_between(b, 10.50, 11.2) == 0, "second tap within 1 s ignored (%d flashes)", flashes_between(b, 10.50, 11.2));
    board_free(b);

    cur_test = "IT-13 tap by day: one 'alive' flash";
    printf("%s\n", cur_test);
    b = board_new(4000, 4000);
    g_ntap = 1;
    g_tap_t[0] = 20.0;
    run(b, 30, env_taps);
    CHECK(flashes_between(b, 19.9, 20.2) == 1 && flashes_between(b, 20.2, 30) == 0,
          "exactly one flash right after the tap, then quiet");
    board_free(b);
}

static void t_two_boards(void)
{
    cur_test = "IT-14 two real boards see each other and lock";
    printf("%s\n", cur_test);
    board_t *a = board_new(0, 4000);
    run(a, 1.234, env_dark);                 /* B powers up 1.234 s after A */
    board_t *b = board_new(0, 4000);
    b->avr->cycle = a->avr->cycle;           /* same timeline */
    run_pair(a, b, 60);
    int pairs = 0, ok = 0;
    double worst = 0;
    for (int i = 0; i < a->nflash; i++) {
        if (a->flash_on[i] < 40) continue;
        double best = 1e9;
        for (int j = 0; j < b->nflash; j++) {
            double d = fabs(b->flash_on[j] - a->flash_on[i]);
            if (d < best) best = d;
        }
        pairs++;
        if (best <= TICK_S * 1.1) ok++;
        if (best > worst) worst = best;
    }
    double first_a = a->nflash > 3 ? a->flash_on[3] : -1, first_b = b->nflash > 3 ? b->flash_on[3] : -1;
    printf("    (A first free flash %.3f s, B %.3f s)\n", first_a, first_b);
    CHECK(pairs > 5 && ok == pairs, "from 40 s on, every flash of A has one from B within one tick (%d/%d, worst %.0f ms)",
          ok, pairs, worst * 1e3);
    board_free(a);
    board_free(b);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    elf_path = argc > 1 ? argv[1] : "../../build/firefly.elf";
    report = argc > 2 && argv[2][0] ? fopen(argv[2], "w") : NULL;
    if (argc > 3) STATIC_RAM_TOP = 0x60 + atoi(argv[3]);
    if (report) fprintf(report, "| Test | Check | Result |\n| --- | --- | --- |\n");
    t_boot_and_free_run();
    t_day_is_quiet();
    t_dusk();
    t_dawn();
    t_low_energy();
    t_entrainment(CYCLE_S * 0.92, "IT-08 locks to a neighbour 8 % faster");
    t_entrainment(CYCLE_S * 0.80, "IT-09 locks to a neighbour 20 % faster");
    t_refractory();
    t_streetlight();
    t_tap();
    t_two_boards();
    printf("\n%d checks, %d failures\n", checks, failures);
    if (report) fclose(report);
    return failures;
}
