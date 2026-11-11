#!/bin/bash
# Stage B: export Specctra DSN from KiCad, autoroute with Freerouting, import the session back into the board.
set -e
cd "$(dirname "$0")/.."
KPY=~/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
DL=${DL:-$HOME/bilge-tools}   # folder holding jre25/ and freerouting.jar; set DL to use another one
JAVA=$(find $DL/jre25 -name java -type f | head -1)
$KPY gen/export_dsn.py
"$JAVA" -Djava.awt.headless=true -jar $DL/freerouting.jar -de kicad/bilge_n2k_revC.dsn -do kicad/bilge_n2k_revC.ses -mp ${1:-40} 2>&1 | grep -v -i analytics | tail -25
$KPY gen/import_ses.py
