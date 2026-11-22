"""Generate <project>.kicad_sch from a design.py using KiCad's stock symbols.

Usage: python3 make_sch.py path/to/design.py
"""
import math
import sys
import uuid
from copy import deepcopy

from sexp import Sym, parse, dump, find, first
from designload import load

SYMDIR = "/usr/share/kicad/symbols/"
PROJECT = None
ROOT = None
_ns = uuid.UUID("5a0c7f2e-1111-4222-8333-944455556666")
_count = [0]


def uid(tag=None):
    _count[0] += 1
    return str(uuid.uuid5(_ns, "%s-%d" % (tag, _count[0])))


_libcache = {}
SYM_UUIDS = {}


def load_lib(nick):
    if nick not in _libcache:
        tree = parse(open(SYMDIR + nick + ".kicad_sym").read())
        _libcache[nick] = {s[1]: s for s in find(tree, "symbol")}
    return _libcache[nick]


def lib_symbol(lib_id):
    """Return a flattened symbol definition named lib_id (with nickname)."""
    nick, name = lib_id.split(":")
    lib = load_lib(nick)
    sym = deepcopy(lib[name])
    ext = first(sym, "extends")
    if ext:
        parent = deepcopy(lib[ext[1]])
        props = find(sym, "property")
        body = [e for e in parent if not (isinstance(e, list) and e[0] == "property")]
        # body = ['symbol', parentname, ...flags..., units...]
        flags = [e for e in body[2:] if not (isinstance(e, list) and e[0] == "symbol")]
        units = [e for e in body[2:] if isinstance(e, list) and e[0] == "symbol"]
        for u in units:
            u[1] = u[1].replace(ext[1], name, 1)
        sym = ["symbol", name] + flags + props + units
    sym[1] = lib_id
    return sym


def pins_of(sym):
    out = {}

    def walk(n):
        for e in n:
            if isinstance(e, list):
                if e[0] == "pin":
                    at = first(e, "at")
                    num = first(e, "number")[1]
                    out[num] = (float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0)
                else:
                    walk(e)
    walk(sym)
    return out


def rot(px, py, r):
    a = math.radians(r)
    return (px * math.cos(a) - py * math.sin(a), px * math.sin(a) + py * math.cos(a))


def snap(v):
    return round(v / 0.635) * 0.635


def pin_pos(part_at, p):
    X, Y, r = part_at
    rx, ry = rot(p[0], p[1], r)
    out_dir = (p[2] + 180 + r) % 360
    return snap(X + rx), snap(Y - ry), out_dir


def eff(size=1.27, hide=False, justify=None):
    e = ["effects", ["font", ["size", size, size]]]
    if justify:
        e.append(["justify"] + [Sym(j) for j in justify.split()])
    if hide:
        e.append(Sym("hide"))
    return e


def prop(name, value, x, y, ang=0, hide=False, justify=None, pid=0):
    return ["property", name, value, ["at", x, y, ang], eff(hide=hide, justify=justify)]


def instance(lib_id, ref, val, fp, at, pins, sym_def, place_text=True):
    X, Y, r = at
    node = ["symbol", ["lib_id", lib_id], ["at", X, Y, r], ["unit", 1],
            ["in_bom", Sym("no" if ref.startswith("#") else "yes")],
            ["on_board", Sym("no" if ref.startswith("#") else "yes")],
            ["dnp", Sym("no")], ["uuid", uid(ref)]]
    SYM_UUIDS[ref] = node[-1][1]
    # property placement: beside the body
    lp = {p[1]: p for p in find(sym_def, "property")}

    def place(name, hide):
        p = lp.get(name)
        px, py = (float(p[3][1]), float(p[3][2])) if p else (0.0, 0.0)
        rx, ry = rot(px, py, r)
        just = None
        if p and first(first(p, "effects") or [], "justify"):
            just = " ".join(str(x) for x in first(first(p, "effects"), "justify")[1:])
        if r in (90, 270) and name in ("Reference", "Value"):
            just = "left"
            return (X + 3.81, Y + (-1.27 if name == "Reference" else 1.27), just)
        if lib_id in ("Device:R", "Device:Polyfuse") and name in ("Reference", "Value"):
            return (X + 2.54, Y + (-1.27 if name == "Reference" else 1.27), "left")
        if r == 180 and name in ("Reference", "Value"):
            return (X, Y + (-3.81 if name == "Reference" else 3.81), None)
        return (X + rx, Y - ry, just)

    for name, value, hide in (("Reference", ref, ref.startswith("#")),
                              ("Value", val, False),
                              ("Footprint", fp, True),
                              ("Datasheet", "", True)):
        x, y, just = place(name, hide)
        if ref.startswith("#PWR") and name == "Value":
            hide = False
        node.append(prop(name, value, round(x, 2), round(y, 2), 0 if r in (0, 180) else 90, hide, just))
    for n in pins:
        node.append(["pin", n, ["uuid", uid(ref + n)]])
    node.append(["instances", ["project", PROJECT,
                               ["path", "/" + ROOT, ["reference", ref], ["unit", 1]]]])
    return node


def wire(x1, y1, x2, y2):
    return ["wire", ["pts", ["xy", x1, y1], ["xy", x2, y2]],
            ["stroke", ["width", 0], ["type", Sym("default")]], ["uuid", uid("w")]]


def label(net, x, y, d):
    just = {0: "left bottom", 180: "right bottom", 90: "left bottom", 270: "right bottom"}[d]
    return ["label", net, ["at", x, y, d], ["fields_autoplaced"],
            eff(justify=just), ["uuid", uid("l")]]


def text(s, x, y, size=1.5):
    return ["text", s, ["at", x, y, 0], eff(size=size, justify="left bottom"), ["uuid", uid("t")]]


def main(D):
    global PROJECT, ROOT
    PROJECT, ROOT = D.PROJECT, D.ROOT_UUID
    PARTS, POWER_NETS = D.PARTS, D.POWER_NETS
    out = D.SCH
    lib_ids = sorted({p["lib"] for p in PARTS} | {"power:VCC", "power:GND"})
    defs = {lid: lib_symbol(lid) for lid in lib_ids}
    items = []
    pwr_n = [0]
    for p in PARTS:
        sd = defs[p["lib"]]
        pins = pins_of(sd)
        assert set(pins) == set(p["pins"]), (p["ref"], pins.keys())
        items.append(instance(p["lib"], p["ref"], p["val"], p["fp"], p["sch"], p["pins"], sd))
        for num, net in p["pins"].items():
            if num in p.get("skip", ()):
                continue
            x, y, d = pin_pos(p["sch"], pins[num])
            if net == "NC":
                items.append(["no_connect", ["at", x, y], ["uuid", uid("nc")]])
                continue
            dx, dy = {0: (1, 0), 90: (0, -1), 180: (-1, 0), 270: (0, 1)}[int(d) % 360]
            L = 2.54
            ex, ey = round(x + dx * L, 4), round(y + dy * L, 4)
            items.append(wire(x, y, ex, ey))
            if net in POWER_NETS:
                pwr_n[0] += 1
                pid = "power:" + net
                base = 270 if net == "GND" else 90
                # horizontal stub: keep the power symbol upright; vertical: point it away
                r = 0 if int(d) % 180 == 0 else int((d - base) % 360)
                items.append(instance(pid, "#PWR%02d" % pwr_n[0], net, "", (ex, ey, r), {"1": net}, defs[pid]))
            else:
                items.append(label(net, ex, ey, int(d) % 360))

    for (x1, y1, x2, y2) in D.SCH_WIRES:
        items.append(wire(x1, y1, x2, y2))
    for (net, x, y, r) in getattr(D, "SCH_POWER", []):
        pwr_n[0] += 1
        pid = "power:" + net
        items.append(instance(pid, "#PWR%02d" % pwr_n[0], net, "", (x, y, r), {"1": net}, defs[pid]))
    for (n, x, y, d) in D.SCH_LABELS:
        items.append(label(n, x, y, d))
    for (x, y) in D.SCH_JUNCTIONS:
        items.append(["junction", ["at", x, y], ["diameter", 0], ["color", 0, 0, 0, 0], ["uuid", uid("j")]])
    notes = [text(s, x, y, size) for (s, x, y, size) in D.NOTES]
    items += notes

    lib_block = ["lib_symbols"] + [defs[k] for k in lib_ids]
    doc = ["kicad_sch", ["version", 20230121], ["generator", Sym("eeschema")],
           ["uuid", ROOT], ["paper", "A4"],
           ["title_block", ["title", D.TITLE], ["date", D.DATE], ["rev", D.REV], ["company", "winfrees/firefly"], ["comment", 1, D.COMMENT]],
           lib_block] + items + [["sheet_instances", ["path", "/", ["page", "1"]]]]
    open(out, "w").write(dump(doc) + "\n")
    import json, os
    json.dump({"root": ROOT, "symbols": SYM_UUIDS}, open(D.UUIDS, "w"), indent=1)
    print("wrote", out, len(items), "items")


if __name__ == "__main__":
    main(load(sys.argv[1]))
