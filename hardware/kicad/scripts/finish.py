import sys, pcbnew
from design import BOARD_W, BOARD_H
pcb = sys.argv[1]
mm = pcbnew.FromMM
OX, OY = 100.0, 80.0
b = pcbnew.LoadBoard(pcb)
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
    for x, y in ((0.3, 0.3), (BOARD_W - 0.3, 0.3), (BOARD_W - 0.3, BOARD_H - 0.3), (0.3, BOARD_H - 0.3)):
        o.Append(mm(OX + x), mm(OY + y))
    b.Add(z)
for t in [d for d in b.GetDrawings() if d.GetClass() in ("PCB_TEXT", "PTEXT")]:
    b.Remove(t)
def txt(s, x, y, size=1.0, layer=pcbnew.F_SilkS):
    t = pcbnew.PCB_TEXT(b); t.SetText(s); t.SetPosition(pcbnew.VECTOR2I(mm(OX + x), mm(OY + y)))
    t.SetLayer(layer); t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); t.SetTextThickness(mm(size * 0.15))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    if layer == pcbnew.B_SilkS: t.SetMirrored(True); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    b.Add(t)
txt("FIREFLY rev 0.1", 23.5, 31.0, 1.2)
txt("PV1: square pad = panel +", 23.5, 33.3, 0.8)
txt("github.com/winfrees/firefly", 22.5, 33.5, 0.9, pcbnew.B_SilkS)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(pcb, b)
pcbnew.WriteDRCReport(b, "/tmp/drc.rpt", pcbnew.EDA_UNITS_MILLIMETRES, True)
print(open("/tmp/drc.rpt").read()[-3000:])
