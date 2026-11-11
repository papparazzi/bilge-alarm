import os, pcbnew
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kicad", "bilge_n2k_revC_routed.kicad_pcb")
b = pcbnew.LoadBoard(p)
for d in b.GetDrawings():
    if d.GetClass() == "PCB_TEXT" and d.GetText().startswith("REV C.3"):
        d.SetText("REV C.5  EXTERNAL 3A FUSE REQ.")
        print("rev text updated")
    if d.GetClass() == "PCB_TEXT" and d.GetText() == "MUTE":
        d.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(90.8), pcbnew.FromMM(64.6)))
        print("MUTE label moved clear of C10")
    if d.GetClass() == "PCB_TEXT" and d.GetText().startswith("TOP ROW ="):
        d.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(45.0), pcbnew.FromMM(34.8)))
        print("moved legend text")
pcbnew.SaveBoard(p, b)

# bottom-left origin (auxiliary origin) so Gerbers, drill and placement files share a conventional positive-coordinate frame
b = pcbnew.LoadBoard(p)
b.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(0, pcbnew.FromMM(100.0)))
pcbnew.SaveBoard(p, b)
print("aux origin set to bottom-left")

# terminal blocks (Degson DG127S-5.08 drawing, DORABO DB127S equivalent): recommended PCB hole 1.40 mm (footprint library says 1.3)
b = pcbnew.LoadBoard(p)
n_ = 0
for ref in ("J1", "J2", "J3", "J4"):
    for q in b.FindFootprintByReference(ref).Pads():
        q.SetDrillSize(pcbnew.VECTOR2I(pcbnew.FromMM(1.4), pcbnew.FromMM(1.4)))
        n_ += 1
pcbnew.SaveBoard(p, b)
print("terminal pad drills set to 1.4 mm:", n_)
