# Firefly firmware

Firmware for the ATtiny13A on the Firefly board. Each board sleeps by day, wakes at dusk, and flashes its red LED every 2.5 s. It watches for its neighbours' flashes with the same LED and pulls itself into step with them, so a group ends up flashing together, like fireflies.

- **Size:** 792 of 1024 bytes of flash, 4 bytes of static RAM.
- **Clock:** factory fuses (1.2 MHz). No fuse change is needed.
- **Verification:** unit tests, an exhaustive two-board check, a swarm simulation, and tests of the compiled binary on a simulated ATtiny13A. All pass. Bench validation is still to do. See the [verification report](docs/verification-report.md).

## What it does

| Situation | Behaviour |
| --- | --- |
| Power-up with enough energy | 2 quick flashes |
| Daylight (panel > 2.5 V) | Dark, asleep; checks every 8 s |
| Night (panel < 1.2 V) | Flashes every 2.5 s, and moves toward neighbours' flashes until they flash together (usually within 5–10 s) |
| Supercap below 2.3 V | Stops flashing until it's back above 2.7 V |
| Steady light at night (streetlight, moon) | Ignored; keeps its own rhythm |
| Tap (optional vibration switch) | Night: flashes at once and restarts its cycle, and neighbours follow. Day: one "I'm alive" flash |

## Build

Install the toolchain:

| System | Command |
| --- | --- |
| Debian/Ubuntu | `sudo apt install gcc-avr avr-libc avrdude` |
| macOS | `brew tap osx-cross/avr && brew install avr-gcc avrdude` |
| Windows | Microchip's AVR 8-bit toolchain plus avrdude, or WSL with the Debian packages |

Then:

```sh
cd firmware
make          # build/firefly.hex, prints size
make check    # fails if over the flash/RAM budget
```

## Program a chip (ISP jig + AVRISP mkII)

1. Put the chip in the [ISP jig](../hardware/isp-jig/README.md): JP1 at USB, **JP2 removed**.
2. Plug in USB power, then the mkII. Its LED goes green.
3. Flash it. A prebuilt image is in `release/firefly-rev0.1.hex` if you'd rather not install the compiler:

   ```sh
   avrdude -c avrispmkII -P usb -p t13 -B 8 -U flash:w:release/firefly-rev0.1.hex:i
   ```

   Or build and flash in one step:

   ```sh
   make flash    # avrdude -c avrispmkII -P usb -p t13 -B 8 -U flash:w:build/firefly.hex:i
   ```

4. **Optional:** `make fuses` turns on the 1.8 V brown-out detector (hfuse 0xFD). The firmware switches it off during sleep, so it costs nothing at night.

**To try it on the jig before moving the chip:**
1. Fit JP2.
2. Add 10 kΩ from J4 pin 2 to pin 8 (supercap OK) and 10 kΩ from J4 pin 7 to pin 4 (night).
3. Expect 2 quick flashes, then one every 2.5 s.

Without these two resistors the sense pins float and the mode is unpredictable. See [bench-validation.md](docs/bench-validation.md), BT-02.

## Test

```sh
sudo apt install gcc libsimavr-dev libelf-dev pkg-config python3-matplotlib
make test     # unit + sweep + simulator + swarm; writes docs/results/
make -C test quick   # same, with a shorter swarm run
```

## Files

| Path | What |
| --- | --- |
| `src/config.h` | All tunables (timing, sync constants, thresholds in mV) |
| `src/firefly.c/.h` | Sync algorithm (pure C, host-testable) |
| `src/main.c` | Hardware: sleep/watchdog, ADC, LED flash and sense, tap switch, modes |
| `test/test_firefly.c` | Unit tests |
| `test/param_sweep.c` | Exhaustive search that chose the sync constants |
| `test/swarm.c`, `test/swarm_report.py` | Many-board simulation and report |
| `test/sim/fwsim.c` | Runs the real `firefly.elf` on simavr and checks it |
| `docs/` | [Design](docs/design.md), [requirements](docs/requirements.md), [V&V plan](docs/verification-plan.md), [V&V report](docs/verification-report.md), [bench validation](docs/bench-validation.md), [results](docs/results/) |

## Tuning

Everything adjustable is in `src/config.h`; the [design doc](docs/design.md#tuning) says what each knob does. After a change, run `make check test`: the tests re-check the sync constants and the timing.

## Pin use

| Pin | Port | Use |
| --- | --- | --- |
| 5 | PB0 | LED anode (through 150 Ω): high to flash |
| 6 | PB1 | LED cathode: low to flash; reverse-charged and floating to sense light |
| 7 | PB2 / ADC1 | Panel voltage (100k/22k): day or night |
| 2 | PB3 / ADC3 | Supercap voltage (3.9M/1M): energy guard |
| 3 | PB4 | Optional tap switch to GND (internal pull-up) |
| 1 | PB5 / RESET | Reset; kept so the chip stays programmable |
