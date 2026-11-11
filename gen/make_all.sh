#!/bin/bash
# Full Rev C build: netlist/BOM -> board -> autoroute -> DRC -> fab exports.  Needs KiCad 10, Java 25 and freerouting.jar (see route.sh).
set -e
cd "$(dirname "$0")/.."
K=~/Applications/KiCad/KiCad.app/Contents
KPY=$K/Frameworks/Python.framework/Versions/Current/bin/python3
python3 gen/design_revC.py
$KPY gen/build_pcb.py 2>&1 | grep -v assert
gen/route.sh 40 2>&1 | grep -E "ses import|stitching|island|tracks"
$KPY gen/verify_board.py 2>&1 | grep -v assert
$K/MacOS/kicad-cli pcb drc --severity-all --format json -o out/drc_final.json kicad/bilge_n2k_revC_routed.kicad_pcb | tail -2
python3 gen/drc_summary.py out/drc_final.json | head -6
