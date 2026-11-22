/*
 * firefly.h - the synchronisation algorithm, with no hardware in it.
 *
 * Each board is a pulse-coupled oscillator (Mirollo & Strogatz, 1990).
 * A phase counter climbs one step per tick; at FF_PERIOD the board
 * flashes and starts over. Seeing a neighbour's flash pushes the phase
 * forward by a fraction of where it already is (half, plus a few ticks), so boards that are
 * nearly ready get pulled into firing together. A short refractory
 * window after each flash stops a board reacting to its own light or
 * to neighbours that flashed with it.
 *
 * Pure C with fixed-width types: the firmware and the host tests
 * compile exactly the same code.
 */
#ifndef FIREFLY_H
#define FIREFLY_H

#include <stdint.h>
#include "config.h"

typedef struct {
    uint8_t phase;       /* 0 .. FF_PERIOD-1                    */
    uint8_t refractory;  /* ticks left in which light is ignored */
} ff_state_t;

/* Start at a given phase (taken modulo FF_PERIOD), not refractory. */
void ff_init(ff_state_t *s, uint8_t seed);

/* Advance one tick. `saw_flash` is non-zero if a neighbour's flash was
 * seen during the tick that just ended. Returns 1 if this board should
 * flash now. */
uint8_t ff_tick(ff_state_t *s, uint8_t saw_flash);

/* Force an immediate flash (used for the tap/startle reaction). */
void ff_fire_now(ff_state_t *s);

#endif
