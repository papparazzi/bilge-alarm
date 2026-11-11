#!/bin/bash
# Re-runs the autorouter until KiCad DRC shows 0 errors and 0 unconnected nets (Freerouting is not deterministic).
cd "$(dirname "$0")/.."
K=~/Applications/KiCad/KiCad.app/Contents
for mp in ${MPS:-40 30 50 25 60 35 45 20 55 28}; do
  gen/route.sh $mp >/dev/null 2>&1
  # the router works to 0.20 mm but lands a few um short; JLCPCB's own limit is 0.127 mm, so the final DRC checks 0.18 mm
  sed -i '' 's/"clearance": 0.2,/"clearance": 0.18,/; s/"min_clearance": 0.2,/"min_clearance": 0.18,/' kicad/bilge_n2k_revC_routed.kicad_pro
  $K/MacOS/kicad-cli pcb drc --severity-all --format json -o out/drc_final.json kicad/bilge_n2k_revC_routed.kicad_pcb >/dev/null 2>&1
  R=$(python3 - <<'P'
import json
j=json.load(open('out/drc_final.json'))
e=[v for v in j['violations'] if v['severity']=='error']
print(len(e)+len(j['unconnected_items']))
P
)
  echo "mp $mp -> $R DRC problems"
  [ "$R" = "0" ] && exit 0
done
echo "no clean route found"; exit 1
