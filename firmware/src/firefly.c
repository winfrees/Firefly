/* firefly.c - pulse-coupled oscillator. See firefly.h. */
#include "firefly.h"

/* the largest jump, from phase FF_PERIOD-1, must still fit in a uint8_t */
#if (FF_PERIOD - 1) + ((FF_PERIOD - 1) >> FF_COUPLING_SHIFT) + FF_KICK + 1 > 255
#error "FF_PERIOD too large for the phase jump to fit in uint8_t"
#endif
#if FF_REFRACTORY >= FF_PERIOD
#error "FF_REFRACTORY must be shorter than FF_PERIOD"
#endif

void ff_init(ff_state_t *s, uint8_t seed)
{
    s->phase = seed % FF_PERIOD;
    s->refractory = 0;
}

uint8_t ff_tick(ff_state_t *s, uint8_t saw_flash)
{
    uint8_t p = s->phase;

    if (s->refractory) {
        s->refractory--;                      /* FW-SYNC-3: ignore echoes */
    } else if (saw_flash) {
        /* FW-SYNC-2: jump forward; bigger jumps late in the cycle.
         * The #if above proves this cannot overflow. */
        p = (uint8_t)(p + (p >> FF_COUPLING_SHIFT) + FF_KICK);
    }

    p++;
    if (p >= FF_PERIOD) {                     /* FW-SYNC-1: fire and restart */
        s->phase = 0;
        s->refractory = FF_REFRACTORY;
        return 1;
    }
    s->phase = p;
    return 0;
}

void ff_fire_now(ff_state_t *s)
{
    s->phase = 0;
    s->refractory = FF_REFRACTORY;
}
