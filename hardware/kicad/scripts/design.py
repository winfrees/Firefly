"""Single source of truth for the Firefly rev 0.1 board.

Both the schematic and the PCB are generated from this file, so they
always share the same parts, footprints and nets.
"""

FP_R = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"
FP_C = "Capacitor_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm"
FP_C5 = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"
FP_HDR2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"

# ref, lib_id, value, footprint, sch (x, y, rot), pin -> net, pcb (x, y, rot)
# PCB coordinates are mm from the board's top-left corner.
PARTS = [
    # ---- power and sensing -------------------------------------------
    dict(ref="PV1", lib="Device:Solar_Cell", val="Solar panel 5V 60mA",
         fp=FP_HDR2, sch=(30.48, 63.5, 0), pins={"1": "PV+", "2": "GND"},
         pcb=(3.5, 3.5, 90), note="Panel leads solder here (+ is pin 1)"),
    dict(ref="D2", lib="Device:D_Zener", val="1N4734A 5.6V",
         fp="Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal", sch=(55.88, 63.5, 270),
         pins={"1": "PV+", "2": "GND"}, pcb=(3.5, 8.0, 0)),
    dict(ref="R2", lib="Device:R", val="100k",
         fp=FP_R, sch=(76.2, 55.88, 0), skip={"2"}, pins={"1": "PV+", "2": "PANEL_SNS"},
         pcb=(3.5, 12.0, 0)),
    dict(ref="R3", lib="Device:R", val="22k",
         fp=FP_R, sch=(76.2, 67.31, 0), skip={"1"}, pins={"1": "PANEL_SNS", "2": "GND"},
         pcb=(3.5, 15.5, 0)),
    dict(ref="C3", lib="Device:C", val="100n",
         fp=FP_C, sch=(88.9, 66.04, 0), skip={"1"}, pins={"1": "PANEL_SNS", "2": "GND"},
         pcb=(14.5, 13.5, 0)),
    dict(ref="D1", lib="Device:D_Schottky", val="BAT46",
         fp="Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal", sch=(116.84, 45.72, 0),
         pins={"1": "VCC", "2": "PV+"}, pcb=(17.5, 3.5, 180)),
    dict(ref="SC1", lib="Device:C_Polarized", val="1F 5.5V supercap",
         fp="firefly:SuperCap_Coin_Vertical_D19.2mm_W4.8mm_P5.00mm",
         sch=(137.16, 63.5, 0), pins={"1": "VCC", "2": "GND"}, pcb=(12.0, 31.0, 0)),
    dict(ref="C2", lib="Device:C", val="10u",
         fp=FP_C5, sch=(157.48, 63.5, 0), pins={"1": "VCC", "2": "GND"},
         pcb=(3.5, 22.0, 0)),
    dict(ref="R4", lib="Device:R", val="3.9M",
         fp=FP_R, sch=(180.34, 55.88, 0), skip={"2"}, pins={"1": "VCC", "2": "CAP_SNS"},
         pcb=(26.0, 22.0, 0)),
    dict(ref="R5", lib="Device:R", val="1M",
         fp=FP_R, sch=(180.34, 67.31, 0), skip={"1"}, pins={"1": "CAP_SNS", "2": "GND"},
         pcb=(26.0, 25.5, 0)),
    dict(ref="C4", lib="Device:C", val="100n",
         fp=FP_C, sch=(193.04, 66.04, 0), skip={"1"}, pins={"1": "CAP_SNS", "2": "GND"},
         pcb=(37.0, 24.0, 0)),
    # ---- microcontroller and light I/O -------------------------------
    dict(ref="U1", lib="MCU_Microchip_ATtiny:ATtiny13A-P", val="ATtiny13A-P",
         fp="Package_DIP:DIP-8_W7.62mm_Socket",
         sch=(114.3, 129.54, 0),
         pins={"1": "RST", "2": "CAP_SNS", "3": "PB4", "4": "GND",
               "5": "PB0", "6": "PB1", "7": "PANEL_SNS", "8": "VCC"},
         pcb=(25.0, 6.0, 0)),
    dict(ref="R6", lib="Device:R", val="10k",
         fp=FP_R, sch=(68.58, 116.84, 0), pins={"1": "VCC", "2": "RST"},
         pcb=(25.0, 2.0, 0)),
    dict(ref="C1", lib="Device:C", val="100n",
         fp=FP_C, sch=(152.4, 116.84, 0), pins={"1": "VCC", "2": "GND"},
         pcb=(36.5, 4.0, 0)),
    dict(ref="R1", lib="Device:R", val="150",
         fp=FP_R, sch=(165.1, 132.08, 0), skip={"2"}, pins={"1": "PB0", "2": "LED_A"},
         pcb=(36.5, 18.5, 0)),
    dict(ref="LED1", lib="Device:LED", val="Red 5mm water-clear",
         fp="LED_THT:LED_D5.0mm", sch=(173.99, 139.7, 180), skip={"2"},
         pins={"1": "PB1", "2": "LED_A"}, pcb=(43.5, 13.0, 0),
         note="Flashes and senses light"),
    dict(ref="SW1", lib="Switch:SW_SPST", val="Vibration switch (opt.)",
         fp=FP_HDR2, sch=(68.58, 142.24, 270), pins={"1": "PB4", "2": "GND"},
         pcb=(16.0, 22.0, 90), note="SW-18010P or tilt switch, optional"),
    # ---- ERC helpers (no footprint) ----------------------------------
    dict(ref="#FLG01", lib="power:PWR_FLAG", val="PWR_FLAG", fp="",
         sch=(38.1, 165.1, 0), pins={"1": "VCC"}),
    dict(ref="#FLG02", lib="power:PWR_FLAG", val="PWR_FLAG", fp="",
         sch=(55.88, 165.1, 0), pins={"1": "GND"}),
]

# Mounting holes (PCB only), mm from top-left.
HOLES = [(46.5, 3.5), (46.5, 32.5)]
BOARD_W, BOARD_H = 50.0, 36.0

POWER_NETS = {"VCC", "GND"}

# Hand-drawn schematic wiring where labels would collide: (x1, y1, x2, y2)
SCH_WIRES = [
    (76.2, 59.69, 76.2, 62.23), (76.2, 62.23, 76.2, 63.5), (76.2, 62.23, 88.9, 62.23), (88.9, 62.23, 96.52, 62.23),
    (180.34, 59.69, 180.34, 62.23), (180.34, 62.23, 180.34, 63.5), (180.34, 62.23, 193.04, 62.23), (193.04, 62.23, 200.66, 62.23),
    (165.1, 135.89, 165.1, 139.7), (165.1, 139.7, 170.18, 139.7),
]
SCH_LABELS = [("PANEL_SNS", 96.52, 62.23, 0), ("CAP_SNS", 200.66, 62.23, 0),
              ("LED_A", 165.1, 139.7, 180)]
SCH_JUNCTIONS = [(76.2, 62.23), (88.9, 62.23), (180.34, 62.23), (193.04, 62.23)]
