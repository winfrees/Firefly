# Verification report: firmware rev 0.1

**Result: all automated verification passes. Hardware validation (L5) is still to do.**

| Level | Checks | Result | Evidence |
| --- | --- | --- | --- |
| Build | no warnings with `-Werror`; 792 / 1024 bytes flash, 4 / 24 bytes static RAM | pass | `make check` |
| L1 Unit | 7 tests, 1,006,616 checks | pass | [results/unit.txt](results/unit.txt) |
| L2 Design search | Model matches `ff_tick()` on 1 M steps; shipped constants are among the 82 of 432 that always lock | pass | [results/param-sweep.md](results/param-sweep.md) |
| L3 Swarm | 9 scenarios × 100 trials; all 900 trials synced | pass | [results/swarm.md](results/swarm.md) |
| L4 Firmware in simulator | 14 tests, 23 checks on the shipped `firefly.elf` | pass | [results/sim.md](results/sim.md) |
| L5 Bench | BT-01 … BT-12 | **not run yet** | [bench-validation.md](bench-validation.md) |

**Configuration under test:**
- Firmware source as committed with this report.
- avr-gcc 7.3.0, avr-libc 2.0.0, simavr 1.6.
- ELF MD5 `8c57069f3acdea6068dbe943f4d329dd`. This is for this toolchain; other compiler versions produce different binaries.

## Key results

### Synchronisation
- **Two boards, exhaustive (UT-06):** all 6084 start-phase pairs lock within 2.4 cycles (≈ 6 s) to ≤ 1 tick apart.
- **Real firmware in the simulator:**
  - It follows a neighbour 8 % or 20 % faster with every flash 27–28 ms after the neighbour's (IT-08, IT-09).
  - Two simulated chips started 1.234 s apart lock to 14 ms (IT-14).
- **Groups (swarm simulation, ±10 % clocks, 10 % of flashes missed):**

| Scenario | Synced | Time to sync, median / p95 | Steady (worst trial) | Spread |
| --- | --- | --- | --- | --- |
| 2 boards | 100/100 | 2.3 s / 16.8 s | 89.0 % | 18 ms |
| 10 boards, all see all | 100/100 | 4.0 s / 7.2 s | 100 % | 30 ms |
| 30 boards, all see all | 100/100 | 4.7 s / 7.9 s | 100 % | 32 ms |
| 4 × 4 grid (neighbours only) | 100/100 | 5.1 s / 10.2 s | 95.5 % | 74 ms |
| 8 in a line (neighbours only) | 100/100 | 7.0 s / 27.8 s | 61.8 % | 104 ms |
| 10 boards, 50 % of flashes missed | 100/100 | 3.9 s / 7.2 s | 98.5 % | 38 ms |
| 10 boards, ±20 % clocks | 100/100 | 4.8 s / 12.6 s | 100 % | 31 ms |

FW-SYNC-6 asks for ≥ 95 % of trials synced, median < 10 s, p95 < 60 s and steady ≥ 95 %. All boards-see-all scenarios meet it. Two scenarios fall short of the steady criterion, which they aren't held to:
- **2 boards:** with only one neighbour, a missed flash (10 % of the time) is a missed volley. Lowest trial: 89 % of volleys in sync.
- **Long chains:** these ripple. Lag adds up hop by hop and a miss near one end disturbs the rest. They are characterised here, not required; see "Limitations".

### Timing, modes and energy (L4, real binary)

| Behaviour | Result |
| --- | --- |
| Free-running period | 2.496 s exactly (78 × 32 ms) |
| Flash length | 15.0 ms |
| Boot flashes | 2, 165 ms apart |
| Awake time at night (including flashes) | 1.05 % (budget 2 %) |
| Awake time by day | 0.015 % (budget 0.05 %) |
| Stack depth | 8 bytes (51 bytes spare) |
| Daylight | no flashes for 59 s |
| Dawn | stops within 1.5 s |
| Dusk | starts 13.9 s after the panel drops, inside the simulator limit; see SIM-L2 |
| Low supercap (2.1 V) | silent, including at boot; resumes 12.3 s after recovery |
| Sag to 2.2 V | stops within 1.5 s |
| Cold start at 2.5 V | silent (hysteresis) |
| Constant light | keeps its 2.496 s rhythm |
| Light during refractory window | flash times unchanged |
| Tap | flash 0 ms after the tap; rattle 0.3 s later ignored; one flash by day |

## Traceability

| Requirement | Verified by | Result |
| --- | --- | --- |
| FW-SYNC-1 | UT-01, IT-01 | pass |
| FW-SYNC-2 | UT-02, UT-04, IT-08, IT-09 | pass |
| FW-SYNC-3 | UT-03, IT-10 | pass |
| FW-SYNC-4 | UT-05; inspection of `main.c` (seed from ADC) | pass |
| FW-SYNC-5 | UT-06 (exhaustive), PS-02, IT-14 | pass |
| FW-SYNC-6 | SW-01 … SW-09 | pass |
| FW-SENSE-1 | IT-08, IT-09, IT-14; BT-06, BT-07 | pass (sim); bench open |
| FW-SENSE-2 | IT-11; BT-06 | pass (sim); bench open |
| FW-LED-1 | IT-01 | pass |
| FW-LED-2 | IT-01, IT-05 | pass |
| FW-PWR-1 | IT-02, IT-04; BT-02, BT-08 | pass (sim); bench open |
| FW-PWR-2 | IT-03, UT-07; BT-08 | pass (sim); bench open |
| FW-PWR-3 | IT-05, IT-06, IT-07, UT-07; BT-09 | pass (sim); bench open |
| FW-PWR-4 | inspection of `main.c`: `PRR`, `ACSR`, `DIDR0`, `sleep_bod_disable()`, `SLEEP_MODE_PWR_DOWN` | pass |
| FW-PWR-5 | IT-01, IT-02; energy budget in [design.md](design.md#energy-budget); BT-04, BT-05 | pass (sim); bench open |
| FW-TAP-1 | IT-12, UT-05 | pass |
| FW-TAP-2 | IT-13 | pass |
| FW-SAFE-1 | inspection: `WDTIE` re-armed only in the main loop after a tick; WDT in interrupt + reset mode (`wdt_set`) | pass |
| FW-SAFE-2 | inspection: `MCUSR = 0; wdt_disable();` first in `main()`, WDT started after the boot flashes | pass |
| FW-RES-1 | `make check`; IT-01 stack check | pass |
| FW-RES-2 | inspection: `F_CPU` 1.2 MHz, no fuse-dependent code; `make fuses` optional | pass |
| FW-BUILD-1 | `make` with `-Werror` | pass |
| HW-V1 … HW-V4 | BT-07, BT-04/05, BT-10, BT-08 | **open** |

## Defects found and fixed

All of these were found by the automated tests before any hardware existed.

| ID | Found by | Defect | Effect on hardware if shipped | Fix |
| --- | --- | --- | --- | --- |
| DR-1 | UT-06 | First-draft constants (jump phase/4 + 2, refractory 13 ticks) let some start pairs settle into a stable anti-phase pattern | Some pairs or groups would never sync | Constants chosen by exhaustive search (L2) and compared in the swarm simulation (L3): jump phase/2 + 3, refractory 34 |
| DR-2 | IT-01 | The watchdog was started before the 0.3 s boot flashes; two timeouts passed without a re-arm | Endless reset loop at power-up: the board would only ever do boot flashes | Start the WDT after the boot sequence |
| DR-3 | IT-01, IT-12 | Every wake-up, including pin-change wake-ups from light and tap, was treated as a 32 ms tick | Timebase runs fast whenever light or taps arrive | WDT interrupt sets `g_tick`; other wake-ups skip the tick |
| DR-4 | IT-08 | `sense_arm()` made PB1 an output while its port bit was 0, with the interrupt still enabled, so the pin glitched low | The board "saw light" on every tick and almost never detected a real neighbour flash: no sync | Disable the interrupt and set PB1 high before driving it |
| DR-5 | inspection | After a watchdog reset the WDT stays on at 16 ms | Reset loop after any watchdog reset | `MCUSR = 0; wdt_disable();` at the start of `main()` |

## Test harness issues found and fixed

These are bugs in the tests, not the firmware, recorded because each one at first made results look wrong.

| ID | Issue | Fix |
| --- | --- | --- |
| H-1 | simavr reads a floating input with no pull-up as 0, so the LED looked discharged at once | Model the junction charge, and drive it with simavr's "external pin level" API |
| H-2 | simavr skips a whole 32 ms sleep in one step, so 15 ms neighbour flashes inside it were never shown to the chip | 1 ms heartbeat timer in the harness |
| H-3 | simavr books sleep time in the step that executes `SLEEP`; awake time was metered wrongly | Classify by the state after the step |
| H-4 | simavr's default sleep hook waits in real time (a 10-minute test run) | No-op sleep hook |
| H-5 | The first swarm "sync" metric reset on any single missed flash, so every scenario looked unsynced | Metric redefined: time to first sync plus steady-state share (verification plan, L3) |

## Deviations and limitations

1. **Lag of up to one tick (32 ms) between synced boards.** This is inherent to sampling light once per tick. FW-SYNC-5 and the tests use "≤ 1 tick apart" as the definition of locked.
2. **Long chains of boards don't flash in unison.** With only neighbour-to-neighbour visibility, lag adds up per hop. An 8-board line shows about 100 ms spread and in-sync volleys 62–93 % of the time. Place boards so that most can see several others.
3. **Day → night switch time in simulation (SIM-L2).** IT-03 measured 13.9 s; on hardware it should be ≤ 8.2 s plus up to one cycle. BT-08 confirms this.
4. **Everything analog is unverified until L5:** LED sensing range, currents, reference tolerance, supercap behaviour and the outdoor environment (SIM-L3 … L6).

## Open items

| Item | Needed for | Owner |
| --- | --- | --- |
| Run BT-01 … BT-12 on first boards; fill in the record sheet | HW-V1 … V4, bench confirmation of FW-SENSE, FW-PWR | builder |
| If BT-07 shows under 1 m range: tune `FLASH_MS`, R1, LED type (see BT-07) | HW-V1 | builder |
| Calibrate dusk thresholds against the real panel (BT-08) | HW-V4 | builder |
