"""Check that the schematic netlist and the PCB both match design.py.

Usage: python3 check_net.py path/to/design.py   (needs kicad-cli and KiCad's python3)
"""
import os, subprocess, sys, tempfile
from designload import load
from sexp import parse, find, first

D = load(sys.argv[1])
net = os.path.join(tempfile.mkdtemp(), "n.net")
subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadsexpr", "-o", net, D.SCH],
               check=True, capture_output=True)
t = parse(open(net).read())
got = {}
for n in find(first(t, "nets"), "net"):
    name = first(n, "name")[1].lstrip("/")
    for nd in find(n, "node"):
        got[(first(nd, "ref")[1], first(nd, "pin")[1])] = name
bad = 0
for p in D.PARTS:
    if p["ref"].startswith("#"):
        continue
    for pin, want in p["pins"].items():
        g = got.get((p["ref"], pin))
        ok = (g is None or g.startswith("unconnected-")) if want == "NC" else g == want
        if not ok:
            bad += 1; print("SCH MISMATCH", p["ref"], pin, want, g)
names = {}
for k, v in got.items():
    names.setdefault(v, []).append(k)
singles = [v for v, ks in names.items() if len(ks) == 1 and not v.startswith("unconnected-")]
if singles:
    print("single-pin nets:", singles); bad += len(singles)
try:
    import pcbnew
    if os.path.exists(D.PCB):
        b = pcbnew.LoadBoard(D.PCB)
        parts = {p["ref"]: p for p in D.PARTS}
        for fp in b.GetFootprints():
            r = fp.GetReference()
            if r not in parts:
                continue
            for pad in fp.Pads():
                want = parts[r]["pins"][pad.GetNumber()]
                want = "" if want == "NC" else want
                if pad.GetNetname() != want:
                    bad += 1; print("PCB MISMATCH", r, pad.GetNumber(), pad.GetNetname(), want)
except ImportError:
    print("(pcbnew not available: skipped PCB check)")
print("nets:", len(names), " mismatches:", bad)
sys.exit(1 if bad else 0)
