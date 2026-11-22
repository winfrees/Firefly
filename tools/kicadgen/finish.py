"""Add ground pours and silkscreen text, fill zones and write a DRC report.

Usage: python3 finish.py path/to/design.py
"""
import os, sys
import pcbnew
from designload import load

D = load(sys.argv[1])
mm = pcbnew.FromMM
OX, OY = D.ORIGIN
b = pcbnew.LoadBoard(D.PCB)
for z in list(b.Zones()):
    b.Remove(z)
gnd = b.GetNetsByName()["GND"]
for ly in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(b)
    z.SetLayer(ly); z.SetNet(gnd)
    z.SetLocalClearance(mm(0.4)); z.SetMinThickness(mm(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(mm(0.4)); z.SetThermalReliefSpokeWidth(mm(0.5))
    o = z.Outline(); o.NewOutline()
    for x, y in ((0.3, 0.3), (D.BOARD_W - 0.3, 0.3), (D.BOARD_W - 0.3, D.BOARD_H - 0.3), (0.3, D.BOARD_H - 0.3)):
        o.Append(mm(OX + x), mm(OY + y))
    b.Add(z)
for t in [d for d in b.GetDrawings() if d.GetClass() in ("PCB_TEXT", "PTEXT")]:
    b.Remove(t)
for s, x, y, size, layer in D.SILK:
    t = pcbnew.PCB_TEXT(b); t.SetText(s); t.SetPosition(pcbnew.VECTOR2I(mm(OX + x), mm(OY + y)))
    t.SetLayer(pcbnew.F_SilkS if layer == "F.SilkS" else pcbnew.B_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); t.SetTextThickness(mm(size * 0.15))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    if layer == "B.SilkS":
        t.SetMirrored(True); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    b.Add(t)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(D.PCB, b)
rpt = os.path.join(D.DIR, "drc.rpt")
pcbnew.WriteDRCReport(b, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
lines = open(rpt).read().splitlines()
real = [l for l in lines if l.startswith("[") and "lib_footprint_issues" not in l]
print("DRC:", [l for l in lines if l.startswith("** Found")], "issues (excl. library-table warnings):", len(real))
for l in sorted(set(real)):
    print("  ", l)
os.remove(rpt)
