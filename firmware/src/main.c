/*
 * Firefly firmware for the ATtiny13A (board rev 0.1).
 *
 *   PB0  LED1 anode, through R1 150R   flash: drive high
 *   PB1  LED1 cathode                   flash: low; sense: reverse-charge, float
 *   PB2  ADC1  panel voltage (100k/22k)   day/night
 *   PB3  ADC3  supercap voltage (3.9M/1M) energy guard
 *   PB4  optional tilt/vibration switch to GND (internal pull-up)
 *   PB5  RESET (kept, for ISP)
 *
 * Everything runs from the watchdog: the chip sleeps in power-down and wakes
 * every 32 ms at night (8 s by day). Light is sensed with the LED itself:
 * charge its junction in reverse, float the cathode and let a pin-change
 * interrupt fire if light discharges it before the next tick. See
 * docs/design.md for the reasoning and docs/requirements.md for IDs.
 */
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "config.h"
#include "firefly.h"

#define LED_A     PB0
#define LED_K     PB1
#define TAP       PB4
#define CH_PANEL  1          /* ADC1 = PB2 */
#define CH_CAP    3          /* ADC3 = PB3 */
#define LED_MASK  ((1 << LED_A) | (1 << LED_K))

#define WDT_32MS  (1 << WDP0)
#define WDT_8S    ((1 << WDP3) | (1 << WDP0))

enum { MODE_NIGHT, MODE_DAY, MODE_LOW };

static volatile uint8_t g_tick;    /* watchdog tick elapsed           */
static volatile uint8_t g_light;   /* LED discharged during this tick */
static volatile uint8_t g_tap;     /* tap switch closed               */
static uint8_t mode;

/* ---- interrupts ----------------------------------------------------- */

/* Marks a tick. WDTIE is cleared by hardware here and re-armed by the main
 * loop, so if the main loop ever hangs the next timeout resets the chip
 * (FW-SAFE-1). */
ISR(WDT_vect)
{
    g_tick = 1;
}

ISR(PCINT0_vect)
{
    uint8_t pins = PINB;
    if ((PCMSK & (1 << LED_K)) && !(pins & (1 << LED_K))) {
        g_light = 1;                         /* FW-SENSE-1 */
        PCMSK &= ~(1 << LED_K);
        DDRB |= (1 << LED_K);                /* hold low: no half-level input current */
    }
    if ((PCMSK & (1 << TAP)) && !(pins & (1 << TAP)))
        g_tap = 1;
}

/* ---- helpers -------------------------------------------------------- */

static void wdt_set(uint8_t wdp)
{
    cli();
    wdt_reset();
    WDTCR = (1 << WDCE) | (1 << WDE);
    WDTCR = (1 << WDTIE) | (1 << WDE) | wdp;  /* interrupt first, reset second */
    sei();
}

static void sleep_now(void)
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
#if defined(BODS) && defined(BODSE)
    sleep_bod_disable();                     /* FW-PWR-4 */
#endif
    sei();
    sleep_cpu();
    sleep_disable();
}

/* Average of 4 conversions against the 1.1 V bandgap; first one discarded. */
static uint16_t adc_read(uint8_t ch)
{
    uint16_t sum = 0;
    PRR &= ~(1 << PRADC);
    ADMUX = (1 << REFS0) | ch;
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);   /* 1.2 MHz / 8 */
    for (uint8_t i = 0; i < 5; i++) {
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC))
            ;
        if (i)
            sum += ADC;
    }
    ADCSRA = 0;
    PRR |= (1 << PRADC);                     /* FW-PWR-4 */
    return sum >> 2;
}

static void led_idle(void)
{
    PCMSK &= ~(1 << LED_K);
    PORTB &= ~LED_MASK;
    DDRB |= LED_MASK;                        /* both ends low: no bias */
}

static void flash(void)                      /* FW-LED-1 */
{
    PCMSK &= ~(1 << LED_K);
    DDRB |= LED_MASK;
    PORTB = (PORTB & ~(1 << LED_K)) | (1 << LED_A);
    _delay_ms(FLASH_MS);
    led_idle();
}

static void sense_arm(void)                  /* FW-SENSE-1 */
{
    /* Order matters: with the interrupt still enabled, making PB1 an output
     * while its PORT bit is 0 glitches it low and fakes a light event on
     * every tick (found in simulation, test IT-08). */
    PCMSK &= ~(1 << LED_K);
    PORTB = (PORTB & ~(1 << LED_A)) | (1 << LED_K);   /* K high first...     */
    DDRB |= LED_MASK;                                 /* ...then drive: reverse-charge */
    DDRB &= ~(1 << LED_K);                            /* float cathode...    */
    PORTB &= ~(1 << LED_K);                           /* ...with no pull-up  */
    GIFR = (1 << PCIF);
    PCMSK |= (1 << LED_K);
}

/* Decide day / night / low-energy from the panel and supercap (FW-PWR-1..3).
 * Bands between the thresholds keep the current mode (hysteresis). */
static void update_mode(void)
{
    uint16_t cap = adc_read(CH_CAP);
    uint16_t pan = adc_read(CH_PANEL);

    if (cap < ADC_CAP_LOW)
        mode = MODE_LOW;
    else if (mode == MODE_LOW && cap < ADC_CAP_RESUME)
        ;                                    /* still recovering */
    else if (pan > ADC_PANEL_DAY)
        mode = MODE_DAY;
    else if (pan < ADC_PANEL_NIGHT)
        mode = MODE_NIGHT;
    else if (mode == MODE_LOW)
        mode = MODE_DAY;                     /* recovered at twilight: wait */
}

/* ---- main ----------------------------------------------------------- */

int main(void)
{
    ff_state_t ff;
    uint8_t prev_light = 1;   /* no "edge" on the first tick */
    uint8_t measure = 0;
    uint8_t tap_hold = 0;

    MCUSR = 0;                               /* clear WDRF before touching WDE */
    wdt_disable();                           /* after a WDT reset it is still on (16 ms) */
    ACSR = (1 << ACD);                       /* analog comparator off */
    DIDR0 = (1 << ADC1D) | (1 << ADC3D);     /* no digital buffers on analog pins */
    PRR = (1 << PRTIM0) | (1 << PRADC);
    PORTB = (1 << TAP);                      /* pull-up on the tap switch */
    DDRB = LED_MASK;
    GIMSK = (1 << PCIE);
    PCMSK = (1 << TAP);
    sei();

    {
        uint16_t c = adc_read(CH_CAP);
        uint16_t p = adc_read(CH_PANEL);
        ff_init(&ff, (uint8_t)(c ^ (c >> 3) ^ (p << 2)));   /* FW-SYNC-4 */
        if (c >= ADC_CAP_RESUME)
            for (uint8_t i = 0; i < BOOT_FLASHES; i++) {    /* FW-LED-2 */
                flash();
                _delay_ms(150);
            }
    }
    mode = MODE_LOW;
    update_mode();
    /* The watchdog starts only now: the boot flashes take longer than two
     * timeouts and would otherwise reset the chip (found in simulation). */
    if (mode == MODE_NIGHT) {
        wdt_set(WDT_32MS);
        sense_arm();
    } else {
        wdt_set(WDT_8S);
    }

    for (;;) {
        sleep_now();

        cli();
        uint8_t tick = g_tick, tap = g_tap;
        g_tick = 0;
        g_tap = 0;
        sei();

        if (tap) {                           /* FW-TAP-1: react now, hold off rattles */
            PCMSK &= ~(1 << TAP);
            tap_hold = TAP_HOLDOFF_TICKS;
            if (mode == MODE_NIGHT) {
                ff_fire_now(&ff);
                flash();
                sense_arm();
            } else if (mode == MODE_DAY) {
                flash();                     /* FW-TAP-2: "I'm alive" */
            }
        }
        if (!tick)
            continue;                        /* a pin-change wake: not a tick */
        WDTCR |= (1 << WDTIE);               /* FW-SAFE-1: we are alive */

        cli();
        uint8_t light = g_light;
        g_light = 0;
        sei();

        if (mode == MODE_NIGHT) {
            /* FW-SENSE-2: count a flash only on dark -> light */
            uint8_t fire = ff_tick(&ff, light && !prev_light);
            prev_light = light;
            if (fire)
                flash();
            if (tap_hold && --tap_hold == 0)
                PCMSK |= (1 << TAP);
            if (++measure >= MEASURE_EVERY_TICKS) {
                measure = 0;
                update_mode();
            }
            if (mode == MODE_NIGHT) {
                sense_arm();
            } else {
                led_idle();
                wdt_set(WDT_8S);
            }
        } else {
            tap_hold = 0;                    /* a timed 8 s wake: taps allowed again */
            PCMSK |= (1 << TAP);
            update_mode();
            if (mode == MODE_NIGHT) {
                prev_light = 1;
                measure = 0;
                wdt_set(WDT_32MS);
                sense_arm();
            }
        }
    }
}
