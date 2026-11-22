import os, subprocess, sys, pcbnew
from ses_import import import_ses
pcb = sys.argv[1]
b = pcbnew.LoadBoard(pcb)
for t in list(b.GetTracks()):
    b.Remove(t)
assert pcbnew.ExportSpecctraDSN(b, "/tmp/fly.dsn")
subprocess.run(["java", "-jar", os.environ.get("FREEROUTING_JAR", "freerouting-1.9.0.jar"), "-de", "/tmp/fly.dsn", "-do", "/tmp/fly.ses", "-mp", "30", "-mt", "1"],
               capture_output=True, text=True, timeout=900)
print("segments, vias:", import_ses(b, "/tmp/fly.ses"))
pcbnew.SaveBoard(pcb, b)
