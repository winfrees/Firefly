# Firefly

Small, cheap, solar-powered boards that live outdoors and talk to each other with light. Each board charges a supercapacitor during the day. At dusk it wakes up and flashes a red LED, and it watches for its neighbours' flashes with the same LED. Over time the boards fall into sync, like fireflies.

**Target:** about $5 per board, with a home 3D-printed case, an ordered PCB, and through-hole assembly at home.

![PCB front](hardware/images/pcb-front.png)

## Status

Rev 0.1 is a draft. The schematic, BOM and a routed 50 × 36 mm two-layer PCB are done, and the board passes DRC. The firmware is not started, and nothing has been built or tested yet.

## Hardware at a glance

![Schematic](hardware/images/kicad-schematic.png)

| Block | Parts | Notes |
| --- | --- | --- |
| MCU | ATtiny13A, DIP-8, socketed | Runs from 1.8–5.5 V. A plain ATtiny13 needs ≥2.7 V |
| Power | 5 V 60 mA panel → BAT46 → 1 F 5.5 V supercap | No battery and no charging IC |
| Clamp | 5.6 V 1 W zener on the panel side | Holds the cap near 5.3 V; no drain at night |
| Light I/O | Red 5 mm water-clear LED between PB0 and PB1 | Drive it forward to flash; reverse-bias it to sense light |
| Sensing | Panel divider → PB2/ADC1, cap divider → PB3/ADC3 | Dusk detection and low-energy shutdown |
| Optional | Vibration switch on PB4 | Wakes the board on a tap or wind |

### Pin map

| Pin | Port | Job |
| --- | --- | --- |
| 1 | PB5 / RESET | 10k pull-up, kept as reset for ISP |
| 2 | PB3 / ADC3 | Cap voltage (3.9M / 1M divider) |
| 3 | PB4 | Optional tilt switch to GND |
| 4 | GND | Ground |
| 5 | PB0 | LED anode through 150 Ω |
| 6 | PB1 | LED cathode / sense input |
| 7 | PB2 / ADC1 | Panel voltage (100k / 22k divider) |
| 8 | VCC | VCAP rail, 100 nF at the pin |

### Rough energy budget (estimates, to be checked on the bench)

- Usable energy, 5.3 V → 1.8 V on 1 F: about 12 J
- Time to fill in full sun: about 1.5 min
- Run time flashing every 3 s: about 12 h
- Run time asleep with no flashing: about 4 days

## Cost

About **$3.57 per board** for the parts you buy, or **$4.57** if you also buy the MCU. See [`hardware/bom.csv`](hardware/bom.csv). Prices are estimates for 20–50 boards, and the solar panel is the item most worth shopping around for.

## Files

| Path | What it is |
| --- | --- |
| `hardware/kicad/` | KiCad 7 project: `firefly.kicad_sch`, `firefly.kicad_pcb`, custom footprint library `firefly.pretty` |
| `hardware/fab/firefly-rev0.1-gerbers.zip` | Gerbers and drill files, ready to upload to JLCPCB, PCBWay or OSH Park |
| `hardware/fab/firefly-schematic.pdf` | Printable schematic |
| `hardware/images/` | Board renders and schematic images |
| `hardware/schematic.html` | Annotated schematic page with the pin map, energy budget and layout notes |
| `hardware/bom.csv` | Bill of materials |
| `hardware/kicad/design.py` | The part, net and placement list both the schematic and board are generated from |
| `hardware/isp-jig/` | Programming and power jig for the AVRISP mkII (KiCad project, Gerbers, BOM, how-to) |
| `tools/kicadgen/` | Scripts that turn a `design.py` into a KiCad schematic and routed board |

## PCB

- 50 × 36 mm, two layers, 1.6 mm FR-4, all through-hole, with two M3 mounting holes
- 0.4 mm tracks, 0.25 mm clearance, and a ground pour on both sides
- Autorouted with Freerouting; DRC passes with no errors and no unconnected pads
- Ordering: upload the Gerber zip and keep the fab's default settings (2 layers, 1.6 mm, HASL)

| Front | Back |
| --- | --- |
| ![front](hardware/images/pcb-front.png) | ![back](hardware/images/pcb-back.png) |

Opening it in KiCad: open `hardware/kicad/firefly.kicad_pro`. The footprints use KiCad's stock libraries, plus `firefly.pretty` for the supercap, which the project's `fp-lib-table` points to. The files were made with KiCad 7 and also open in KiCad 8 and 9.

Regenerating it: edit `hardware/kicad/design.py` and run the pipeline in [`tools/kicadgen`](tools/kicadgen/README.md).

## Assembly notes

- U1 is socketed and there is no ISP header, so program the chips on the [ISP jig](hardware/isp-jig/README.md) with an AVRISP mkII, then move them to the board. The LED sits between MOSI and MISO and C3 loads SCK, so in-circuit programming would be unreliable anyway.
- Fuses: 128 kHz or 1.2 MHz clock, BOD at 1.8 V, with BOD disabled during sleep in firmware.
- The internal 1.1 V reference is only accurate to ±10%, so calibrate ADC thresholds per board or leave margin.
- Outdoors: use a PETG or ASA case (not PLA), conformal-coat the board, and leave a drain hole under the panel.

## Roadmap

- [x] KiCad schematic and PCB (50 × 36 mm)
- [x] ISP programming and power jig for the AVRISP mkII
- [ ] Firefly sync firmware (MicroCore / avr-gcc)
- [ ] Enclosure STL
- [ ] Bench test of the energy budget
