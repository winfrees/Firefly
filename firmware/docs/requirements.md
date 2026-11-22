# Firmware requirements

Requirements for the Firefly firmware rev 0.1 (ATtiny13A on board rev 0.1). Each has an ID used in the source code, the tests and the [verification report](verification-report.md).

**Verification methods:** **T** = test (host or simulator), **A** = analysis, **I** = inspection of code or build output, **B** = bench test on real hardware ([bench-validation.md](bench-validation.md)).

## Synchronisation

| ID | Requirement | Rationale | Method |
| --- | --- | --- | --- |
| FW-SYNC-1 | With no light seen, a board flashes every `FF_PERIOD` = 78 ticks (2.50 s at 32 ms per tick). | A steady rhythm is the baseline that others lock onto. | T |
| FW-SYNC-2 | When a neighbour's flash is seen outside the refractory window, the phase jumps forward by phase/2 + 3 ticks. If that reaches the period, the board flashes on that tick. | Pulse-coupled oscillator: larger jumps late in the cycle pull boards together. | T |
| FW-SYNC-3 | For 34 ticks (1.09 s) after its own flash, a board ignores light. | Stops reactions to its own reflection and to boards that flashed with it, and removes the anti-phase trap (DR-1). | T |
| FW-SYNC-4 | The start phase is seeded from ADC readings, so boards don't start in step by accident. | Spreads start phases; the algorithm still has to sync them. | I, T |
| FW-SYNC-5 | Two boards with equal clocks that see each other end up flashing ≤ 1 tick apart from **every** pair of start phases, within 3 cycles. | Guarantees there is no stable "not together" state. | T (exhaustive) |
| FW-SYNC-6 | Up to 30 mutually visible boards with ±10 % clock spread and 90 % flash detection sync in ≥ 95 % of trials within 60 s (median < 10 s), and stay in sync for ≥ 95 % of later cycles. | The real use case: a group in a garden. | T (swarm simulation) |

## Light sensing and LED

| ID | Requirement | Rationale | Method |
| --- | --- | --- | --- |
| FW-SENSE-1 | At night, before each tick's sleep, the LED is reverse-charged (PB1 high, PB0 low) and PB1 is left floating with a pin-change interrupt. A discharge before the next tick counts as "light this tick". | Sensing with the same LED needs no extra parts and almost no power. | T, B |
| FW-SENSE-2 | Only a dark → light change between consecutive ticks counts as a neighbour flash. Steady light (moon, streetlight) causes no reactions. | Ambient light must not drive the rhythm. | T |
| FW-LED-1 | A flash drives PB0 high and PB1 low for 15 ms (±10 %). | Visible, and energy-bounded. | T |
| FW-LED-2 | At power-up with enough energy, the board gives 2 quick flashes. With low energy it gives none. | Bench aid: shows the chip is alive. | T |

## Power and energy

| ID | Requirement | Rationale | Method |
| --- | --- | --- | --- |
| FW-PWR-1 | Daylight: with the panel above 2.5 V the board doesn't flash, and checks again every 8 s. | No point flashing in daylight; this saves energy for the night. | T |
| FW-PWR-2 | Night: with the panel below 1.2 V and enough energy, the board runs the flash/sense loop. Between 1.2 and 2.5 V it keeps its current mode (hysteresis). | Clean switching at dusk and dawn. | T |
| FW-PWR-3 | Energy guard: below 2.3 V on the supercap the board stops flashing, and it resumes only above 2.7 V. | Keeps the MCU above brown-out and stops on/off chatter. | T |
| FW-PWR-4 | Between ticks the MCU is in power-down sleep. ADC, Timer0 and the analog comparator are off, digital input buffers on ADC pins are off, and BOD is disabled during sleep. | Sleep current dominates the daytime budget. | I |
| FW-PWR-5 | The CPU is awake ≤ 2 % of the time at night (including the 15 ms flash) and ≤ 0.05 % by day. | Keeps MCU current small next to the LED (see [design.md](design.md#energy-budget)). | T, A, B |

## Tap switch (optional SW1)

| ID | Requirement | Rationale | Method |
| --- | --- | --- | --- |
| FW-TAP-1 | At night a tap (PB4 low) makes the board flash within one tick and restart its cycle. Further taps are ignored for ~1 s. | A playful "startle" that ripples through the group; holdoff stops wind rattle from draining energy. | T |
| FW-TAP-2 | By day a tap gives one flash if energy is OK. In low-energy mode it gives none. | Lets you check a board in daylight. | T |

## Safety and resources

| ID | Requirement | Rationale | Method |
| --- | --- | --- | --- |
| FW-SAFE-1 | The watchdog runs in interrupt-then-reset mode and is re-armed only from the main loop, so a hung main loop resets the chip. | Unattended outdoor device. | I |
| FW-SAFE-2 | After any reset, including a watchdog reset, the firmware disables the watchdog before its slow boot sequence. | A WDT reset leaves the WDT on at 16 ms, which would cause a reset loop. | I |
| FW-RES-1 | Flash ≤ 1024 bytes, static RAM ≤ 24 bytes, and at least 16 bytes of stack headroom measured in simulation. | ATtiny13A has 1 kB flash and 64 B RAM. | I (`make check`), T |
| FW-RES-2 | Runs on factory fuses (9.6 MHz RC ÷ 8 = 1.2 MHz). The 1.8 V BOD fuse is optional. | No fuse step needed to get going. | I |
| FW-BUILD-1 | Builds with `-Wall -Wextra -Werror`, with no warnings. | | I |

## Hardware-level targets (validated on the bench, not by firmware tests)

| ID | Target | Method |
| --- | --- | --- |
| HW-V1 | Two boards detect each other's flashes in darkness at ≥ 1 m. | B (BT-07) |
| HW-V2 | Average current ≤ 80 µA at night at 3.5 V, and ≤ 10 µA by day. | B (BT-04, BT-05) |
| HW-V3 | A full supercap (5.3 V) runs at least 8 h of night flashing. | A, B (BT-10) |
| HW-V4 | Dusk and dawn switching happen at sensible light levels for the panel used. | B (BT-08) |
