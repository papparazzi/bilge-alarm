import os, json, pcbnew
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
b = pcbnew.LoadBoard(os.path.join(root, "kicad", "bilge_n2k_revC_routed.kicad_pcb"))
out = {}
for f in b.GetFootprints():
    if f.GetReference() in ("J1", "J2", "J3", "J4", "HDR1", "HDR2"):
        ps = [p for p in f.Pads() if p.GetNumber()]
        x = sum(pcbnew.ToMM(p.GetX()) for p in ps) / len(ps)
        y = sum(pcbnew.ToMM(p.GetY()) for p in ps) / len(ps)
        out[f.GetReference()] = [round(x, 3), round(y, 3)]
json.dump(out, open(os.path.join(root, "out", "centroids.json"), "w"))
print(out)
