"""Load a board's design.py (the single source of truth for one board)."""
import importlib.util
import os


def load(path):
    path = os.path.abspath(path)
    spec = importlib.util.spec_from_file_location("design", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.DIR = os.path.dirname(path)
    d = dict(SCH_WIRES=[], SCH_LABELS=[], SCH_JUNCTIONS=[], NOTES=[], SILK=[], HOLES=[],
             CUSTOM_LIBS={}, POWER_NETS={"VCC", "GND"}, TRACK_WIDTH=0.4, CLEARANCE=0.25,
             COMMENT="", ORIGIN=(100.0, 80.0))
    for k, v in d.items():
        if not hasattr(mod, k):
            setattr(mod, k, v)
    mod.SCH = os.path.join(mod.DIR, mod.PROJECT + ".kicad_sch")
    mod.PCB = os.path.join(mod.DIR, mod.PROJECT + ".kicad_pcb")
    mod.UUIDS = os.path.join(mod.DIR, ".sym_uuids.json")
    return mod
