"""Writes the BOM files for PCBWay and JLCPCB quote requests from design_revC.py (no layout needed for a parts quote)."""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design_revC as d

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
by = {}
for p in d.parts:
    if p["mpn"]:
        by.setdefault(p["mpn"], []).append(p)
PKG = {"R_0603_1608Metric": "R0603", "C_0603_1608Metric": "C0603", "C_0805_2012Metric": "C0805", "LED_0603_1608Metric": "LED0603"}
items = sorted(by.items(), key=lambda kv: d.nat(kv[1][0]["ref"]))
rows_pcbway, rows_jlc = [], []
for i, (mpn, ps) in enumerate(items, 1):
    man, desc, pkg, lcsc, price, stock = d.MPN[mpn]
    refs = ",".join(sorted((p["ref"] for p in ps), key=d.nat))
    fp = ps[0]["fp"].split(":")[1]
    passive = mpn.startswith(("0603WAF", "CL10", "CL21"))
    note = ("Any brand OK: same value, package, voltage/tolerance, commercial grade" if passive
            else "Exact MPN preferred. Equivalent with same pinout and ratings OK - tell us the MPN for approval")
    rows_pcbway.append([i, refs, len(ps), man, mpn, desc, pkg, "THT" if "DB127" in mpn else "SMD", note, "Place"])
    rows_jlc.append([desc, refs, PKG.get(fp, pkg), lcsc])
hdr = ["Item #", "*Designator", "*Qty", "Manufacturer", "*Mfg Part #", "Description / Value", "*Package/Footprint",
       "Mounting Type", "Your Instructions / Notes", "Assembly requirement"]
with open(os.path.join(root, "quote_pcbway", "BOM_revC_PCBWay.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(hdr); w.writerows(rows_pcbway)
with open(os.path.join(root, "quote_jlcpcb", "BOM_revC_JLCPCB.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"]); w.writerows(rows_jlc)
print(len(rows_pcbway), "lines")
