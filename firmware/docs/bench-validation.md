# Bench validation procedure

These tests cover what simulation can't (see SIM-L1..L6 in the [verification plan](verification-plan.md)): real LED sensing, real currents, real thresholds and real nights. Run them on the first boards you build, and record results in the sheet at the end.

**You'll need:** the ISP jig with an AVRISP mkII, a bench supply (0–6 V), a multimeter with a µA range, a dark room, a flashlight, a stopwatch or phone, and two or more assembled firefly boards. A scope helps but isn't required.

## Programming and jig tests

### BT-01 Program and verify
1. On the jig: JP1 at USB, JP2 removed, chip in the socket. Connect the mkII; its LED goes green.
2. In `firmware/`, run `make flash`. avrdude reports `verified`.
3. **Pass:** the signature reads `0x1e9007` and the flash verifies.

### BT-02 Firmware runs on the jig
The jig has no panel or supercap dividers, so set the two sense inputs on J4 with resistors:

| To simulate | J4 wiring |
| --- | --- |
| Supercap OK | 10 kΩ from pin 2 (PB3) to pin 8 (VCC) |
| Night | 10 kΩ from pin 7 (PB2) to pin 4 (GND) |
| Day | 10 kΩ from pin 7 (PB2) to pin 8 (VCC) |

1. Wire "supercap OK" and "night". Fit JP2. Power up.
2. Expect 2 quick flashes, then a flash every 2.5 s. Time 10 flashes.
3. Move the PB2 resistor to "day". Flashing should stop within about 1 s.
4. Move it back to "night". Flashing should restart within about 8 s.
5. **Pass:** boot flashes are seen; 10 periods take 25 s ± 2.5 s (the watchdog clock is ±10 %); day and night switch as described.

### BT-03 Fuses (optional brown-out detector)
1. Run `make fuses`. This writes lfuse 0x6A (factory) and hfuse 0xFD (BOD at 1.8 V).
2. Read them back: `avrdude -c avrispmkII -P usb -p t13 -B 8 -U hfuse:r:-:h`.
3. **Pass:** reads `0xfd`.

## Board tests (assembled firefly board)

For BT-04, BT-05 and BT-09, leave SC1 unfitted (or disconnected) and feed its pads from the bench supply through the multimeter.

### BT-04 Day current (sleep floor)
1. Supply 3.5 V to the SC1 pads. Wire the panel pad PV+ to the same 3.5 V; with D1 seeing 0 V, nothing flows through it, and the panel divider reads "day".
2. Wait 20 s, then read the current on the µA range.
3. Subtract the panel divider current, 3.5 V / 122 kΩ = 29 µA. The divider only draws in daylight on the real board, and then from the panel.
4. **Pass:** ≤ 10 µA after the subtraction (HW-V2). Expected about 5–8 µA.

### BT-05 Night current
1. As BT-04, but short PV+ to GND (dark).
2. The current pulses: about 9 mA for 15 ms every 2.5 s, on a floor of a few µA. A typical meter shows a jumpy value. Either:
   - read the average with a meter that has an averaging/"AVG" mode, or
   - measure across a 10 Ω shunt with a scope and integrate, or
   - skip it and rely on BT-10, which measures the same thing as run time.
3. **Pass:** average ≤ 80 µA at 3.5 V (HW-V2). Expected about 60–70 µA.

### BT-06 Sensing works
1. Dark room; the board at night, running from its own charged supercap or the bench supply.
2. Blink a flashlight at the LED once every 2 s or so, from about 30 cm.
3. **Pass:** within a few blinks, the board flashes right after each blink.
4. Then hold the flashlight on steadily. **Pass:** the board goes back to its own 2.5 s rhythm (steady light is ignored).

### BT-07 Range between boards
1. Dark room. Two boards facing each other at 0.25 m. Power one up 1–2 s after the other so they start out of step.
2. Within 30 s they should flash together.
3. Repeat at 0.5, 1, 2 and 3 m, and at 45° off-axis. Record the largest distance that syncs.
4. **Pass:** syncs at ≥ 1 m head-on (HW-V1).
5. If it fails, try in this order:
   1. Make sure the LEDs are water-clear, high-brightness red (diffused ones sense poorly).
   2. Raise `FLASH_MS` to 25.
   3. Lower R1 to 100 Ω (check the LED's current rating).
   4. Shade the LED from ambient light.

### BT-08 Dusk and dawn switching
1. Bench: feed PV+ from a variable supply through 1 kΩ, with the supercap charged or SC1 fed.
2. Lower PV+ from 4 V in 0.1 V steps, waiting 10 s per step. Note the voltage at which flashing starts.
3. Raise it again and note the voltage at which flashing stops.
4. **Pass:** starts at 1.2 V ± 15 %, stops at 2.5 V ± 15 % (the reference is ±10 %).
5. Outdoors: note the time flashing starts after sunset and ends at dawn, then adjust `PANEL_NIGHT_MV` / `PANEL_DAY_MV` if needed (HW-V4).

### BT-09 Energy guard
1. Bench supply on the SC1 pads; PV+ shorted to GND (night).
2. Lower from 3.5 V in 0.05 V steps (wait 3 s each) and note where flashing stops.
3. Raise it again and note where it resumes (up to 8.2 s per step).
4. Power-cycle at 2.5 V: there should be no boot flashes and no flashing.
5. **Pass:** stops at 2.3 V ± 10 %, resumes at 2.7 V ± 10 %, and stays silent at 2.5 V after a cold start.

### BT-10 Run time on one charge
1. Fit SC1. Charge to about 5.3 V (sun, or bench supply on PV+ at 5.6 V through 100 Ω).
2. Cover the panel completely (night) and start a timer, or a timelapse with a phone.
3. Note when flashing stops.
4. **Pass:** ≥ 8 h (HW-V3). Expected 10–14 h.

### BT-11 Outdoor group
1. Five or more boards in a garden for at least 3 nights, with some in direct view of each other and some only of a neighbour.
2. Each night: do they start at dusk, sync within about a minute, stay silent by day, and recover after rain?
3. **Pass:** all of the above on every night with enough sun to charge; note any board that drops out.

### BT-12 Tap (if SW1 is fitted)
1. At night, tap a board. It flashes at once, and its neighbours follow within a cycle or two.
2. By day, tap it. It gives one flash.
3. **Pass:** both behave as described.

## Record sheet

| Test | Board / chip | Date | Measured | Pass? | Notes |
| --- | --- | --- | --- | --- | --- |
| BT-01 | | | signature: | | |
| BT-02 | | | 10 periods: s | | |
| BT-03 | | | hfuse: | | |
| BT-04 | | | µA (after −29 µA) | | |
| BT-05 | | | µA avg | | |
| BT-06 | | | | | |
| BT-07 | | | max distance: m | | |
| BT-08 | | | night at V, day at V | | |
| BT-09 | | | stop V, resume V | | |
| BT-10 | | | h | | |
| BT-11 | | | nights OK: / | | |
| BT-12 | | | | | |
