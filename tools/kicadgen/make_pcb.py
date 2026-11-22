"""Place footprints, assign nets and draw the outline for a design.py.

Usage: python3 make_pcb.py path/to/design.py   (KiCad's own python3)
Run make_sch.py first: it records the symbol UUIDs that link the board to the schematic.
"""
import json
import os
import sys

import pcbnew
from designload import load

FPDIR = "/usr/share/kicad/footprints/"
mm = pcbnew.FromMM


def build(D):
    OX, OY = D.ORIGIN

    def P(x, y):
        return pcbnew.VECTOR2I(mm(OX + x), mm(OY + y))

    def load_fp(fpid):
        lib, name = fpid.split(":")
        if lib in D.CUSTOM_LIBS:
            path = os.path.join(D.DIR, D.CUSTOM_LIBS[lib])
        else:
            path = FPDIR + lib + ".pretty"
        fp = pcbnew.FootprintLoad(path, name)
        if fp is None:
            raise SystemExit("footprint not found: " + fpid)
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        return fp

    uu = json.load(open(D.UUIDS))
    b = pcbnew.NewBoard(D.PCB)
    ds = b.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    ds.m_TrackMinWidth = mm(0.25)
    ds.m_MinClearance = mm(0.2)
    ds.m_HoleToHoleMin = mm(0.25)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(mm(D.TRACK_WIDTH))
    nc.SetClearance(mm(D.CLEARANCE))
    nc.SetViaDiameter(mm(0.8))
    nc.SetViaDrill(mm(0.4))

    nets = {}
    for p in D.PARTS:
        for n in p["pins"].values():
            if n != "NC" and n not in nets:
                ni = pcbnew.NETINFO_ITEM(b, n)
                b.Add(ni)
                nets[n] = ni

    for p in D.PARTS:
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
        ref_at = p.get("ref_at")
        if ref_at is None and r == 0:
            ref_at = {"R": (3.81, 0.0), "U": (3.81, 3.81)}.get(p["ref"][0])
        if ref_at:
            t = fp.Reference()
            t.SetPosition(P(x + ref_at[0], y + ref_at[1]))
            t.SetTextAngleDegrees(0)
            t.SetTextSize(pcbnew.VECTOR2I(mm(0.9), mm(0.9)))
            t.SetTextThickness(mm(0.14))
        for pad in fp.Pads():
            num = pad.GetNumber()
            if num not in p["pins"]:
                raise SystemExit("unmapped pad %s %s" % (p["ref"], num))
            if p["pins"][num] != "NC":
                pad.SetNet(nets[p["pins"][num]])
            if p.get("zone_full"):  # solid zone connection where thermals get starved
                pad.SetZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    for i, (x, y) in enumerate(D.HOLES):
        h = load_fp("MountingHole:MountingHole_3.2mm_M3")
        h.SetReference("H%d" % (i + 1))
        b.Add(h)
        h.SetPosition(P(x, y))
        h.Reference().SetVisible(False)

    R, W, H = 2.0, D.BOARD_W, D.BOARD_H
    for a, c in (((R, 0), (W - R, 0)), ((W, R), (W, H - R)), ((W - R, H), (R, H)), ((0, H - R), (0, R))):
        s = pcbnew.PCB_SHAPE(b)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a)); s.SetEnd(P(*c))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); b.Add(s)
    for cx, cy, sx, sy in ((R, R, 0, R), (W - R, R, W - R, 0), (W - R, H - R, W, H - R), (R, H - R, R, H)):
        s = pcbnew.PCB_SHAPE(b)
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetCenter(P(cx, cy)); s.SetStart(P(sx, sy))
        s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); b.Add(s)
    pcbnew.SaveBoard(D.PCB, b)


def overlaps(D):
    OX, OY = D.ORIGIN
    b = pcbnew.LoadBoard(D.PCB)
    boxes = []
    for fp in b.GetFootprints():
        cy = fp.GetCourtyard(pcbnew.F_CrtYd)
        bb = cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False, False)
        boxes.append((fp.GetReference(), pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                      pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY))
    bad = 0
    allowed = set(getattr(D, "OVERHANG", ()))
    for i, a in enumerate(boxes):
        if (a[1] < 0 or a[2] < 0 or a[3] > D.BOARD_W or a[4] > D.BOARD_H) and a[0] not in allowed:
            print("OFF-BOARD %s x %.1f..%.1f y %.1f..%.1f" % (a[0], a[1], a[3], a[2], a[4])); bad += 1
        for c in boxes[i + 1:]:
            if a[1] < c[3] and c[1] < a[3] and a[2] < c[4] and c[2] < a[4]:
                print("OVERLAP", a[0], c[0]); bad += 1
    if "-v" in sys.argv:
        for r in sorted(boxes):
            print("%-5s x %5.1f..%5.1f  y %5.1f..%5.1f" % (r[0], r[1], r[3], r[2], r[4]))
    return bad


if __name__ == "__main__":
    D = load(sys.argv[1])
    build(D)
    print("placement problems:", overlaps(D))
