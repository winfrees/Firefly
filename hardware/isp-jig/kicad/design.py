"""Firefly ISP jig rev 0.1: socket + power board for an Atmel AVRISP mkII.

The mkII only senses target voltage, so this board powers the chip (USB 5 V
through a polyfuse, or an external 1.8-5.5 V supply picked with JP1). The
chip sits in an 8-pin socket wired to the standard 6-pin AVR ISP header.
D2 and R3 copy the firefly board's LED wiring (PB0 -> 150 R -> LED -> PB1)
so firmware can be tried on the bench; pull JP2 while programming.

Build with the scripts in tools/kicadgen (see tools/kicadgen/README.md).
"""

PROJECT = "isp-jig"
ROOT_UUID = "7c2e9a41-3b5d-4f60-8a71-2d3e4f5a6b7c"
TITLE = "Firefly ISP jig"
DATE = "2026-11-22"
REV = "0.1"
COMMENT = "Programming + power board for AVRISP mkII and 8-pin AVRs"

FP_R = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"
FP_C = "Capacitor_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm"
H = "Connector_PinHeader_2.54mm:PinHeader_"

PARTS = [
    # ---- power -------------------------------------------------------
    dict(ref="J1", lib="Connector:USB_B", val="USB-B (5 V in)",
         fp="Connector_USB:USB_B_OST_USB-B1HSxx_Horizontal", sch=(35.56, 66.04, 0),
         skip={"5"}, pins={"1": "VBUS", "2": "NC", "3": "NC", "4": "GND", "5": "GND"},
         pcb=(14.0, 13.0, 180), ref_at=(-7.0, 0.0)),
    dict(ref="F1", lib="Device:Polyfuse", val="PTC 0.5 A (MF-RHT050)",
         fp="Fuse:Fuse_Bourns_MF-RHT050", sch=(71.12, 63.5, 0),
         pins={"1": "VBUS", "2": "V_USB"}, pcb=(14.0, 25.5, 0)),
    dict(ref="J2", lib="Connector_Generic:Conn_01x02", val="EXT 1.8-5.5V",
         fp=H + "1x02_P2.54mm_Vertical", sch=(114.3, 60.96, 0),
         pins={"1": "V_EXT", "2": "GND"}, pcb=(4.0, 36.0, 90)),
    dict(ref="JP1", lib="Jumper:Jumper_3_Open", val="Power: USB | EXT",
         fp=H + "1x03_P2.54mm_Vertical", sch=(142.24, 63.5, 0),
         pins={"1": "V_USB", "2": "VCC", "3": "V_EXT"}, pcb=(12.0, 31.0, 90)),
    dict(ref="C1", lib="Device:C", val="10u",
         fp="Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm", sch=(165.1, 63.5, 0),
         pins={"1": "VCC", "2": "GND"}, pcb=(22.0, 36.0, 0)),
    dict(ref="C2", lib="Device:C", val="100n",
         fp=FP_C, sch=(177.8, 63.5, 0), pins={"1": "VCC", "2": "GND"},
         pcb=(37.5, 10.0, 0)),
    dict(ref="R1", lib="Device:R", val="1k",
         fp=FP_R, sch=(195.58, 58.42, 0), skip={"2"}, pins={"1": "VCC", "2": "PWR_LED"},
         pcb=(32.0, 36.0, 0)),
    dict(ref="D1", lib="Device:LED", val="Green 3mm (power)",
         fp="LED_THT:LED_D3.0mm", sch=(195.58, 71.12, 90), skip={"2"},
         pins={"1": "GND", "2": "PWR_LED"}, pcb=(43.0, 36.0, 0), zone_full=True),
    # ---- target and ISP ------------------------------------------------
    dict(ref="U1", lib="MCU_Microchip_ATtiny:ATtiny13A-P",
         val="Socket: ATtiny13A/25/45/85", fp="Package_DIP:DIP-8_W7.62mm_Socket",
         sch=(116.84, 129.54, 0),
         pins={"1": "RST", "2": "PB3", "3": "PB4", "4": "GND",
               "5": "MOSI_PB0", "6": "MISO_PB1", "7": "SCK_PB2", "8": "VCC"},
         pcb=(27.0, 14.0, 0)),
    dict(ref="J3", lib="Connector:AVR-ISP-6", val="AVRISP mkII",
         fp=H + "2x03_P2.54mm_Vertical", sch=(63.5, 129.54, 0),
         pins={"1": "MISO_PB1", "2": "VCC", "3": "SCK_PB2", "4": "MOSI_PB0",
               "5": "RST", "6": "GND"}, pcb=(28.0, 6.0, 90), ref_at=(-3.0, -1.27)),
    dict(ref="R2", lib="Device:R", val="10k",
         fp=FP_R, sch=(88.9, 116.84, 0), pins={"1": "VCC", "2": "RST"},
         pcb=(22.0, 27.0, 0)),
    dict(ref="R3", lib="Device:R", val="150",
         fp=FP_R, sch=(152.4, 132.08, 0), pins={"1": "MOSI_PB0", "2": "TLED_A"},
         pcb=(37.0, 26.0, 0)),
    dict(ref="D2", lib="Device:LED", val="Red 5mm (test, like firefly)",
         fp="LED_THT:LED_D5.0mm", sch=(172.72, 139.7, 180),
         pins={"1": "TLED_K", "2": "TLED_A"}, pcb=(43.5, 17.0, 180)),
    dict(ref="JP2", lib="Jumper:Jumper_2_Open", val="LED test (remove to program)",
         fp=H + "1x02_P2.54mm_Vertical", sch=(210.82, 139.7, 0),
         pins={"1": "TLED_K", "2": "MISO_PB1"}, pcb=(37.0, 30.5, 90)),
    dict(ref="J4", lib="Connector_Generic:Conn_01x08", val="Chip pins 1-8",
         fp=H + "1x08_P2.54mm_Vertical", sch=(233.68, 111.76, 0), skip={"4", "8"},
         pins={"1": "RST", "2": "PB3", "3": "PB4", "4": "GND",
               "5": "MOSI_PB0", "6": "MISO_PB1", "7": "SCK_PB2", "8": "VCC"},
         pcb=(51.0, 13.0, 0)),
    # ---- ERC helpers ---------------------------------------------------
    dict(ref="#FLG01", lib="power:PWR_FLAG", val="PWR_FLAG", fp="",
         sch=(38.1, 165.1, 0), pins={"1": "VCC"}),
    dict(ref="#FLG02", lib="power:PWR_FLAG", val="PWR_FLAG", fp="",
         sch=(55.88, 165.1, 0), pins={"1": "GND"}),
]

SCH_WIRES = [(33.02, 76.2, 35.56, 76.2), (195.58, 62.23, 195.58, 64.77), (195.58, 64.77, 195.58, 67.31)]
SCH_WIRES += [(228.6, 111.76, 213.36, 111.76), (228.6, 121.92, 208.28, 121.92)]
SCH_POWER = [("GND", 213.36, 111.76, 0), ("VCC", 208.28, 121.92, 0)]
SCH_LABELS = [("PWR_LED", 195.58, 64.77, 0)]

NOTES = [
    ("Firefly ISP jig rev 0.1: socket + power for an AVRISP mkII", 25.4, 25.4, 2.5),
    ("The mkII does not power the target. JP1 picks USB 5 V (via F1) or an external 1.8-5.5 V supply on J2.", 25.4, 33.02, 1.5),
    ("D1 shows target power. Plug the mkII ribbon into J3 with pin 1 (red stripe) at the marked corner.", 25.4, 38.1, 1.5),
    ("Target: any 8-pin AVR with the ATtiny13A/25/45/85 pinout. J4 brings all 8 pins out for a scope or breadboard.", 25.4, 93.98, 1.5),
    ("D2 + R3 copy the firefly LED (PB0 -> 150R -> LED -> PB1). Pull JP2 while programming: the LED loads MOSI/MISO.", 25.4, 99.06, 1.5),
]

BOARD_W, BOARD_H = 55.0, 44.0
HOLES = [(51.5, 3.5), (51.5, 40.5)]
OVERHANG = {"J1"}  # USB socket mouth hangs over the left edge on purpose
SILK = [
    ("FIREFLY ISP JIG r0.1", 14.0, 41.0, 1.1, "F.SilkS"),
    ("JP2 off while programming", 14.0, 42.9, 0.8, "F.SilkS"),
    ("JP1: 1-2 USB, 2-3 EXT", 2.0, 28.7, 0.8, "F.SilkS"),
    ("github.com/winfrees/firefly", 14.0, 41.5, 0.9, "B.SilkS"),
]
