"""Autoroute a board with Freerouting and import the result.

Usage: FREEROUTING_JAR=freerouting-1.9.0.jar python3 route.py path/to/design.py
"""
import os, subprocess, sys, tempfile
import pcbnew
from designload import load
from ses_import import import_ses

D = load(sys.argv[1])
b = pcbnew.LoadBoard(D.PCB)
for t in list(b.GetTracks()):
    b.Remove(t)
tmp = tempfile.mkdtemp()
dsn, ses = os.path.join(tmp, "b.dsn"), os.path.join(tmp, "b.ses")
assert pcbnew.ExportSpecctraDSN(b, dsn)
subprocess.run(["java", "-jar", os.environ.get("FREEROUTING_JAR", "freerouting-1.9.0.jar"),
                "-de", dsn, "-do", ses, "-mp", "30", "-mt", "1"], capture_output=True, text=True, timeout=900)
print("segments, vias:", import_ses(b, ses))
pcbnew.SaveBoard(D.PCB, b)
