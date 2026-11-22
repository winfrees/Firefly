/*
 * config.h - every tunable number in the Firefly firmware, in one place.
 *
 * Each value has a requirement ID (see docs/requirements.md) where one
 * applies. Voltage thresholds are written in millivolts at the real node
 * (panel or supercap) and converted to ADC counts here, at compile time.
 */
#ifndef FIREFLY_CONFIG_H
#define FIREFLY_CONFIG_H

/* ---- Clock -------------------------------------------------------------
 * Factory fuses: 9.6 MHz RC oscillator / 8 = 1.2 MHz. No fuse change needed. */
#ifndef F_CPU
#define F_CPU 1200000UL
#endif

/* ---- Timebase (watchdog oscillator, 128 kHz nominal, +/-10 %) ----------- */
#define TICK_MS              32u   /* night tick; WDT 32 ms setting        */
#define DAY_SLEEP_S           8u   /* day / low-energy check interval       */

/* ---- Firefly oscillator (FW-SYNC-*) ------------------------------------ */
#ifndef FF_PERIOD
#define FF_PERIOD            78u   /* ticks per flash cycle: 78*32ms = 2.5 s */
#endif
#ifndef FF_REFRACTORY
#define FF_REFRACTORY        34u   /* ticks after own flash that ignore light */
#endif
#ifndef FF_COUPLING_SHIFT
#define FF_COUPLING_SHIFT     1u   /* phase jump = phase >> 1 (50 %) ...      */
#endif
#ifndef FF_KICK
#define FF_KICK               3u   /* ... plus this many ticks                */
#endif
/* Chosen by exhaustive search (test/param_sweep.c) and the swarm simulation:
 * see docs/verification-report.md, section "Choosing the sync constants". */

/* ---- Flash (FW-LED-*) --------------------------------------------------- */
#define FLASH_MS             15u   /* LED on-time per flash                  */
#define BOOT_FLASHES          2u   /* quick flashes at power-up (bench aid)  */
#define TAP_HOLDOFF_TICKS    31u   /* ~1 s between accepted taps             */

/* ---- Energy and daylight thresholds (FW-PWR-*) ------------------------- */
#define PANEL_DAY_MV       2500u   /* panel above this -> day (sleep)        */
#define PANEL_NIGHT_MV     1200u   /* panel below this -> night (active)     */
#define CAP_LOW_MV         2300u   /* supercap below this -> stop flashing   */
#define CAP_RESUME_MV      2700u   /* supercap above this -> resume          */
#define MEASURE_EVERY_TICKS  31u   /* ~1 s between ADC checks at night       */

/* ---- Hardware constants (from hardware/kicad/design.py) ---------------- */
#define VREF_MV            1100u   /* internal bandgap, +/-10 %              */
/* Panel divider R2 = 100k, R3 = 22k  -> ratio 22/122                       */
#define PANEL_NUM            22ul
#define PANEL_DEN           122ul
/* Cap divider R4 = 3.9M, R5 = 1M     -> ratio 10/49                        */
#define CAP_NUM              10ul
#define CAP_DEN              49ul

/* mV at the node -> 10-bit ADC counts against the 1.1 V reference */
#define MV_TO_ADC(mv, num, den) \
    ((uint16_t)(((uint32_t)(mv) * (num) * 1024ul) / ((uint32_t)(den) * VREF_MV)))

#define ADC_PANEL_DAY    MV_TO_ADC(PANEL_DAY_MV,   PANEL_NUM, PANEL_DEN)
#define ADC_PANEL_NIGHT  MV_TO_ADC(PANEL_NIGHT_MV, PANEL_NUM, PANEL_DEN)
#define ADC_CAP_LOW      MV_TO_ADC(CAP_LOW_MV,     CAP_NUM,   CAP_DEN)
#define ADC_CAP_RESUME   MV_TO_ADC(CAP_RESUME_MV,  CAP_NUM,   CAP_DEN)

#endif
