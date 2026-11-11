import os, pcbnew
b = pcbnew.LoadBoard(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kicad", "bilge_n2k_revC.kicad_pcb"))
ok = pcbnew.ExportSpecctraDSN(b, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kicad", "bilge_n2k_revC.dsn"))
print("dsn export", ok)
