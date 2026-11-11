"""Moves the reference designators that DRC flags (silk over pads / over other silk) to the first clean spot around the part."""
import os, json, subprocess, pcbnew
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
p = os.path.join(root, "kicad", "bilge_n2k_revC_routed.kicad_pcb")
kcli = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
FLAGGED = "R26 D5 C18 Q1 D7".split()
CAND = [(0, -3.0, 0), (0, 3.0, 0), (3.0, 0, 0), (-3.0, 0, 0), (0, -3.6, 90), (0, 3.6, 90), (3.4, -2.2, 0), (-3.4, -2.2, 0), (3.4, 2.2, 0), (-3.4, 2.2, 0), (0, -4.4, 0), (0, 4.4, 0), (4.4, 0, 0), (-4.4, 0, 0)]


def warn_refs():
    out = os.path.join(root, "out", "drc_refs.json")
    subprocess.run([kcli, "pcb", "drc", "--severity-all", "--format", "json", "-o", out, p], capture_output=True)
    j = json.load(open(out))
    bad = set()
    for v in j["violations"]:
        for it in v["items"]:
            for r in FLAGGED:
                if it["description"].startswith(f"Reference field of {r}"):
                    bad.add(r)
    return bad


done = {}
base = {r: pcbnew.LoadBoard(p).FindFootprintByReference(r).GetPosition() for r in FLAGGED}
for k, (dx, dy, rot) in enumerate(CAND):
    b = pcbnew.LoadBoard(p)
    for r in FLAGGED:
        if r in done:
            x, y, rr = done[r]
        else:
            x, y, rr = pcbnew.ToMM(base[r].x) + dx, pcbnew.ToMM(base[r].y) + dy, rot
        t = b.FindFootprintByReference(r).Reference()
        t.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        t.SetTextAngleDegrees(rr)
    pcbnew.SaveBoard(p, b)
    bad = warn_refs()
    for r in FLAGGED:
        if r not in done and r not in bad:
            done[r] = (pcbnew.ToMM(base[r].x) + dx, pcbnew.ToMM(base[r].y) + dy, rot)
            print("ref", r, "-> candidate", k, done[r])
    if len(done) == len(FLAGGED):
        break
print("unresolved:", [r for r in FLAGGED if r not in done])
