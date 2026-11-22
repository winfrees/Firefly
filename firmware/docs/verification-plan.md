# Verification and validation plan

How the Firefly firmware is checked: what is verified where, with which tools, and what counts as passing. Results are in [verification-report.md](verification-report.md).

## Strategy

The firmware is checked at four levels. Each level catches what the one below can't.

| Level | What runs | Where | Checks | Tool |
| --- | --- | --- | --- | --- |
| L1 Unit | `firefly.c` compiled for the PC | host | Algorithm rules, ranges, exhaustive two-board lock, threshold arithmetic | `test/test_firefly.c` |
| L2 Design search | Parameterised copy of `ff_tick()` (checked identical to the real one) | host | Which sync constants always lock | `test/param_sweep.c` |
| L3 System simulation | `firefly.c` in a continuous-time model of many boards | host | Group behaviour with clock spread, missed flashes, topologies | `test/swarm.c`, `swarm_report.py` |
| L4 Firmware-in-simulator | **The shipped `firefly.elf`** on simavr's ATtiny13A model, with the outside world (panel, supercap, neighbour light, tap) played by a harness | host | Timing, modes, energy guard, sensing logic, interrupts, stack, awake time, two real boards locking | `test/sim/fwsim.c` |
| L5 Bench validation | Real chips on the ISP jig and on firefly boards | bench, outdoors | Analog behaviour the simulator can't model: LED sensing range, currents, thresholds, run time | [bench-validation.md](bench-validation.md) |

L1–L4 are automated: `make test` in `firmware/` runs all of them and writes the evidence to `docs/results/`. L5 is a manual procedure with recorded results.

## Test environment

| Item | Version |
| --- | --- |
| Compiler | avr-gcc 7.3.0 (Atmel 3.7.0), avr-libc 2.0.0 |
| Simulator | simavr 1.6 (`attiny13a` core) |
| Host tests | gcc, C99; Python 3 + matplotlib for the plots |
| Target | ATtiny13A, factory fuses (1.2 MHz), 3.5 V in simulation |

## Test cases

### L1: unit (`test/test_firefly.c`)

| ID | Verifies | Pass criteria |
| --- | --- | --- |
| UT-01 | FW-SYNC-1 | From every start phase, first flash at `78 − start` ticks and then every 78 |
| UT-02 | FW-SYNC-2 | For every phase, the jump equals phase/2 + 3 + 1, or the board fires at once |
| UT-03 | FW-SYNC-3 | Light during the 34 refractory ticks changes nothing; light after it does |
| UT-04 | FW-SYNC-2 robustness | 1,000,000 random inputs: phase always < 78 and refractory ≤ 34 |
| UT-05 | FW-SYNC-4, FW-TAP-1 | Every seed maps into range; `ff_fire_now` restarts the cycle and the refractory window |
| UT-06 | FW-SYNC-5 | All 6084 start pairs of two boards end ≤ 1 tick apart for 3+ cycles within 10 cycles |
| UT-07 | FW-PWR-1..3 | Thresholds within ADC range, hysteresis bands exist, the 5.3 V cap doesn't saturate the ADC, and the cut-off has margin above BOD |

### L2: design search (`test/param_sweep.c`)

| ID | Verifies | Pass criteria |
| --- | --- | --- |
| PS-01 | model fidelity | The sweep model equals `ff_tick()` for 1,000,000 random steps |
| PS-02 | FW-SYNC-5 (design margin) | The shipped constants are in the "always locks" set |

### L3: swarm (`test/swarm.c`)

| ID | Scenario | Pass criteria |
| --- | --- | --- |
| SW-01..03 | 2, 10, 30 boards that all see each other, ±10 % clocks, 90 % detection | ≥ 95 % of trials synced; median < 10 s; p95 < 60 s; steady ≥ 95 % (FW-SYNC-6) |
| SW-04..06 | 10 in a ring, 4 × 4 grid, 8 in a line | Report only: multi-hop behaviour is characterised, not required |
| SW-07 | 50 % of flashes missed | ≥ 95 % synced |
| SW-08 | ±20 % clocks | ≥ 95 % synced |
| SW-09 | Ideal (no drift, no misses) | 100 % synced |

### L4: firmware in simulator (`test/sim/fwsim.c`)

| ID | Verifies | Pass criteria |
| --- | --- | --- |
| IT-01 | FW-SYNC-1, FW-LED-1/2, FW-PWR-5, FW-RES-1 | 2 boot flashes; period 2.496 s ± 10 ms; width 15 ms ± 10 %; awake ≤ 2 %; ≥ 8 bytes of stack spare |
| IT-02 | FW-PWR-1, FW-PWR-5 | Panel 4 V: no flashes for 59 s; awake ≤ 0.05 % |
| IT-03 | FW-PWR-2 | Panel drops at 20 s: quiet before, flashing after (see SIM-L2 for the time limit) |
| IT-04 | FW-PWR-1 | Panel rises at 20 s: no flashes after 21.5 s |
| IT-05 | FW-PWR-3, FW-LED-2 | Cap 2.1 V: no flashes, not even at boot; flashing resumes when the cap reaches 3.0 V |
| IT-06 | FW-PWR-3 | Cap sags to 2.2 V at night: flashing stops within 1.5 s |
| IT-07 | FW-PWR-3 (hysteresis) | Cold start at 2.5 V: silent |
| IT-08 | FW-SYNC-2, FW-SENSE-1 | Neighbour 8 % faster: every own flash ≤ 1 tick after the neighbour's, from 60 s on |
| IT-09 | FW-SYNC-2, FW-SENSE-1 | Same, neighbour 20 % faster |
| IT-10 | FW-SYNC-3 | A pulse 0.2 s after each own flash leaves every flash time unchanged |
| IT-11 | FW-SENSE-2 | Constant light: still flashes at the nominal period |
| IT-12 | FW-TAP-1 | Flash ≤ 1 tick after a tap; a second tap 0.3 s later is ignored |
| IT-13 | FW-TAP-2 | By day: exactly one flash per tap |
| IT-14 | FW-SYNC-5, FW-SENSE-1 | Two simulated chips running the real firmware, started 1.234 s apart, each seeing the other's LED: from 40 s on every flash is paired within 1 tick |

### L5: bench

See [bench-validation.md](bench-validation.md): BT-01 to BT-11.

## Entry and exit criteria

- **To merge a firmware change:** `make check test` passes. That means the build has no warnings, it fits the budget, every L1/L2/L4 check passes, and SW-01..03, 07, 08 and 09 meet their criteria. Commit the regenerated `docs/results/`.
- **To call a board revision validated:** additionally BT-01 to BT-08 pass on at least two boards and BT-10 passes on one.

## Known limits of the automated checks

| ID | Limitation | Covered by |
| --- | --- | --- |
| SIM-L1 | simavr's watchdog is exact, while real ones vary ±10 % | Swarm simulation (SW-01..08) with drift |
| SIM-L2 | simavr 1.6 applies a new watchdog period only after the pending timeout, so a day → night switch can look up to 8.2 s slower than on hardware | IT-03/IT-05 limits allow two 8.2 s periods; BT-08 measures the real delay |
| SIM-L3 | No analog model of the LED: the harness turns "light" into an instant discharge of PB1 | BT-06, BT-07 (real sensing and range) |
| SIM-L4 | No current measurement: awake time is a proxy | BT-04, BT-05 |
| SIM-L5 | Brown-out, supercap ESR and the real ADC reference tolerance (±10 %) are not modelled | BT-03, BT-09, BT-10 |
| SIM-L6 | The swarm model assumes flashes are seen or missed independently; no geometry, occlusion or reflections | BT-11 (outdoor group test) |

## Risks

| Risk | Effect | Likelihood | Mitigation |
| --- | --- | --- | --- |
| LED sensing range too short | Boards don't see each other; no sync | Medium | BT-07 early; options: brighter or longer flash, water-clear high-brightness LED, a phototransistor on PB4 |
| Ambient light at night (streetlight) | A board is "blind" and free-runs | Medium | FW-SENSE-2 keeps it rhythmic; site choice |
| Internal 1.1 V reference ±10 % | Thresholds shift ±10 % | High | Margins in thresholds; per-board calibration possible (BT-09) |
| Clock spread larger than ±20 % | Slower or no sync | Low | SW-08 shows ±20 % works; BT-11 |
| Supercap leakage larger than assumed | Shorter nights | Medium | BT-10; a bigger cap or a longer period |
| Moisture or condensation | Leakage on PB1 looks like "light" (FW-SENSE-2 then blinds the board), or corrosion | Medium | Conformal coating; enclosure drain; BT-11 over several nights |
