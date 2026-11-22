# kicadgen

These scripts build a KiCad 7 schematic and PCB from one Python file per board (`design.py`). The file lists every part, its footprint, its schematic and board position, and which net each pin connects to. Because the schematic and board come from the same list, they can't drift apart.

Boards that use it:

- `hardware/kicad/design.py`: the firefly board
- `hardware/isp-jig/kicad/design.py`: the programming jig

## Pipeline

Run these with the Python that ships with KiCad 7 (on Linux, `/usr/bin/python3` after installing `kicad`):

```sh
D=hardware/isp-jig/kicad/design.py
python3 tools/kicadgen/make_sch.py  $D   # schematic from stock KiCad symbols
python3 tools/kicadgen/make_pcb.py  $D   # footprints, nets, outline, rules; checks for overlaps
FREEROUTING_JAR=freerouting-1.9.0.jar \
python3 tools/kicadgen/route.py     $D   # Freerouting autoroute + import
python3 tools/kicadgen/finish.py    $D   # ground pours, silkscreen, zone fill, DRC
python3 tools/kicadgen/check_net.py $D   # schematic and PCB nets vs design.py
```

Then export the fab files with `kicad-cli pcb export gerbers` and `kicad-cli pcb export drill`.

## Notes

- `make_pcb.py` starts a fresh board, so hand edits made in KiCad are lost if you rerun it. Once you start editing a board by hand, treat the `.kicad_pcb` file as the source and stop regenerating it.
- A pin on net `"NC"` gets a no-connect marker in the schematic and no net on the board.
- KiCad 7's command line has no ERC, so `check_net.py` exports the netlist and checks every pin against `design.py` instead. It also flags single-pin nets.
- `route.py` exists because pcbnew 7 can't import a Freerouting session file when run as a standalone script; `ses_import.py` does that import.
