"""Generate firefly.kicad_pcb from design.py (placement, nets, outline, rules)."""
import json
import os
import sys

import pcbnew
from design import PARTS, HOLES, BOARD_W, BOARD_H

FPDIR = "/usr/share/kicad/footprints/"
PRJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + "/"
OX, OY = 100.0, 80.0  # board origin on the page
mm = pcbnew.FromMM


def P(x, y):
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + y))


def load_fp(fpid):
    lib, name = fpid.split(":")
    path = PRJ + lib + ".pretty" if lib == "firefly" else FPDIR + lib + ".pretty"
    fp = pcbnew.FootprintLoad(path, name)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def build(out):
    uu = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "sym_uuids.json")))
    b = pcbnew.NewBoard(out)
    ds = b.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    ds.m_TrackMinWidth = mm(0.25)
    ds.m_MinClearance = mm(0.2)
    ds.m_HoleToHoleMin = mm(0.25)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(mm(0.4))
    nc.SetClearance(mm(0.25))
    nc.SetViaDiameter(mm(0.8))
    nc.SetViaDrill(mm(0.4))

    nets = {}
    for p in PARTS:
        for n in p["pins"].values():
            if n not in nets:
                ni = pcbnew.NETINFO_ITEM(b, n)
                b.Add(ni)
                nets[n] = ni

    for p in PARTS:
        if not p["fp"]:
            continue
        fp = load_fp(p["fp"])
        fp.SetReference(p["ref"])
        fp.SetValue(p["val"])
        x, y, r = p["pcb"]
        b.Add(fp)
        fp.SetPosition(P(x, y))
        fp.SetOrientationDegrees(r)
        fp.SetPath(pcbnew.KIID_PATH("/" + uu["symbols"][p["ref"]]))
        ref_at = {"R": (3.81, 0.0), "U": (3.81, 3.81)}.get(p["ref"][0])
        if ref_at and r == 0:
            t = fp.Reference()
            t.SetPosition(P(x + ref_at[0], y + ref_at[1]))
            t.SetTextSize(pcbnew.VECTOR2I(mm(0.9), mm(0.9)))
            t.SetTextThickness(mm(0.14))
        for pad in fp.Pads():
            num = pad.GetNumber()
            if num in p["pins"]:
                pad.SetNet(nets[p["pins"][num]])
            else:
                raise SystemExit("unmapped pad %s %s" % (p["ref"], num))

    for i, (x, y) in enumerate(HOLES):
        h = load_fp("MountingHole:MountingHole_3.2mm_M3")
        h.SetReference("H%d" % (i + 1))
        b.Add(h)
        h.SetPosition(P(x, y))
        h.Reference().SetVisible(False)

    # outline with rounded corners
    R = 2.0
    W, H = BOARD_W, BOARD_H
    segs = [((R, 0), (W - R, 0)), ((W, R), (W, H - R)), ((W - R, H), (R, H)), ((0, H - R), (0, R))]
    for a, c in segs:
        s = pcbnew.PCB_SHAPE(b)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a)); s.SetEnd(P(*c))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); b.Add(s)
    for cx, cy, sx, sy in ((R, R, 0, R), (W - R, R, W - R, 0), (W - R, H - R, W, H - R), (R, H - R, R, H)):
        s = pcbnew.PCB_SHAPE(b)
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetCenter(P(cx, cy)); s.SetStart(P(sx, sy)); s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); b.Add(s)
    pcbnew.SaveBoard(out, b)
    return b


def overlaps(b):
    boxes = []
    for fp in b.GetFootprints():
        cy = fp.GetCourtyard(pcbnew.F_CrtYd)
        bb = cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False, False)
        boxes.append((fp.GetReference(), pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                      pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY))
    bad = 0
    for i, a in enumerate(boxes):
        if a[1] < 0 or a[2] < 0 or a[3] > BOARD_W or a[4] > BOARD_H:
            print("OFF-BOARD", a); bad += 1
        for c in boxes[i + 1:]:
            if a[1] < c[3] and c[1] < a[3] and a[2] < c[4] and c[2] < a[4]:
                print("OVERLAP", a[0], c[0]); bad += 1
    for r in sorted(boxes):
        print("%-5s x %5.1f-%5.1f  y %5.1f-%5.1f" % r)
    return bad


if __name__ == "__main__":
    out = sys.argv[1]
    b = build(out)
    b = pcbnew.LoadBoard(out)
    print("problems:", overlaps(b))
