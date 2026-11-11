import csv, os, re, glob, zipfile, shutil, subprocess
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
os.chdir(root)
H = 100.0
rows = list(csv.DictReader(open("out/pos_kicad.csv")))
import json
cent = json.load(open("out/centroids.json"))
out = []
for r in rows:
    if re.fullmatch(r"H\d|BR\d|PW\d", r["Ref"]):
        continue
    x_, y_ = float(r["PosX"]), float(r["PosY"])   # bottom-left origin, Y up (aux origin set on the board)
    if r["Ref"] in cent:   # multi-pin THT parts: report the pad centroid (part centre), not pin 1
        x_, y_ = cent[r["Ref"]][0], H - cent[r["Ref"]][1]
    rot_ = 0.0 if r["Ref"].startswith("HDR") else float(r["Rot"])   # JLC 1x22 header footprint is horizontal at 0 deg and the rows are horizontal on this board
    # JLCPCB's part library is oriented differently from KiCad's for these packages (measured in JLCPCB's PCBA viewer 2026-10-28:
    # the part models sat 180 / 270 / 270 degrees off the pads). Offsets are added to KiCad's rotation for the placement file only.
    pk = r["Package"]
    off = 270 if pk.startswith(("TSOT-23", "SOIC-8")) else (180 if pk.startswith("SOT-23") else 0)
    rot_ = (rot_ + off) % 360
    out.append((r["Ref"], x_, y_, rot_, r["Val"], r["Package"]))
out.sort(key=lambda t: (t[0][0], int("".join(c for c in t[0] if c.isdigit()))))
with open("mfg/CPL_revC.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for ref, x, y, rot, val, pkg in out:
        w.writerow([ref, f"{x:.3f}mm", f"{y:.3f}mm", "Top", f"{rot:.0f}"])
with open("mfg/CPL_revC_with_values.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Designator", "Value", "Package", "Mid X mm", "Mid Y mm (up from bottom-left)", "Layer", "Rotation (KiCad, CCW)"])
    for ref, x, y, rot, val, pkg in out:
        w.writerow([ref, val, pkg, f"{x:.3f}", f"{y:.3f}", "Top", f"{rot:.0f}"])
for p in glob.glob("mfg/gerbers/*routed*"):
    os.rename(p, p.replace("_routed", ""))
with zipfile.ZipFile("mfg/bilge_n2k_revC_gerbers.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for p in sorted(glob.glob("mfg/gerbers/*")):
        if p.endswith((".pdf", ".gbrjob")):
            continue
        z.write(p, os.path.basename(p))
for d in ("quote_pcbway", "quote_jlcpcb"):
    os.makedirs(d, exist_ok=True)
    for fn in ("mfg/bilge_n2k_revC_gerbers.zip", "mfg/CPL_revC.csv"):
        shutil.copy(fn, d)
print(len(out), "CPL rows; gerber zip:", zipfile.ZipFile("mfg/bilge_n2k_revC_gerbers.zip").namelist())
