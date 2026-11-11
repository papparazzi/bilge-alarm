#!/bin/bash
# Fab exports from the routed board: Gerbers + drill (zip), CPL, BOM files for PCBWay and JLCPCB.
set -e
cd "$(dirname "$0")/.."
K=~/Applications/KiCad/KiCad.app/Contents/MacOS
B=kicad/bilge_n2k_revC_routed.kicad_pcb
rm -rf mfg/gerbers && mkdir -p mfg/gerbers out
$K/kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts --use-drill-file-origin --output mfg/gerbers/ $B >/dev/null
$K/kicad-cli pcb export drill --format excellon --drill-origin plot --excellon-units mm --excellon-separate-th --generate-map --map-format pdf --output mfg/gerbers/ $B >/dev/null
$K/kicad-cli pcb export pos --format csv --units mm --side front --use-drill-file-origin --exclude-dnp --output out/pos_kicad.csv $B >/dev/null
$K/kicad-cli pcb export pdf --layers F.Cu,F.Silkscreen,F.Fab,Edge.Cuts --output mfg/bilge_n2k_revC_assembly_top.pdf $B >/dev/null
$K/kicad-cli pcb export pdf --layers B.Cu,Edge.Cuts --output mfg/bilge_n2k_revC_copper_bottom.pdf $B >/dev/null
~/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 gen/centroids.py >/dev/null 2>&1 || true
python3 gen/post_export.py
