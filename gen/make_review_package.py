"""Build the external-review package: bilge_n2k_revC_review_package.zip
Run with the system python3 after the board is routed and exported (export_fab.sh).
Uses KiCad's python for the board statistics and kicad-cli for DRC / renders.
"""
import os, sys, csv, json, shutil, subprocess, zipfile, collections, textwrap, datetime
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import design_revC as d

K = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents")
KCLI = K + "/MacOS/kicad-cli"
KPY = K + "/Frameworks/Python.framework/Versions/Current/bin/python3"
SCR = os.environ.get("BILGE_DS", os.path.expanduser("~/bilge-tools/ds"))   # folder with the downloaded datasheets
PCB = os.path.join(ROOT, "kicad", "bilge_n2k_revC_routed.kicad_pcb")
PKG = os.path.join(ROOT, "review_package", "bilge_n2k_revC_review")
ZIP = os.path.join(ROOT, "bilge_n2k_revC_review_package.zip")
shutil.rmtree(os.path.join(ROOT, "review_package"), ignore_errors=True)
for sub in ("01_read_first", "02_design_documents", "03_circuit_netlist_bom", "04_kicad", "05_fabrication", "06_renders", "07_datasheets", "08_verification", "09_generator_source"):
    os.makedirs(os.path.join(PKG, sub), exist_ok=True)


def cp(src, dst_sub, name=None):
    if os.path.exists(src):
        shutil.copy(src, os.path.join(PKG, dst_sub, name or os.path.basename(src)))
    else:
        print("MISSING", src)


# ------------------------------------------------------------------ circuit description + netlist from the design source
parts = sorted(d.parts, key=lambda p: d.nat(p["ref"]))
nets = d.nets()
blocks = collections.OrderedDict()
for p in parts:
    blocks.setdefault(p["block"] or "Other", []).append(p)
md = ["# Circuit description (generated from gen/design_revC.py, the single source of the netlist)\n",
      "Every part below with its footprint, its pins and the net each pin sits on. Nets named `J1_Pn`/`J3_Pn` are unused dev-board pins that only go to the breakout pad rows.\n"]
for blk, ps in blocks.items():
    md.append(f"\n## Block: {blk}\n")
    md.append("| Ref | Part (MPN) | Footprint | Pin -> net | Note |\n|---|---|---|---|---|")
    for p in ps:
        if p["ref"] in ("HDR1", "HDR2", "BR1", "BR2"):
            pins = "22 pins, see netlist file"
        else:
            pins = ", ".join(f"{k}:{v}" for k, v in sorted(p["pins"].items(), key=lambda kv: int(kv[0])))
        md.append(f"| {p['ref']} | {p['mpn'] or '(no part: pads/hole only)'} | {p['fp'].split(':')[-1]} | {pins} | {p['note'].replace('|', '/')} |")
open(os.path.join(PKG, "03_circuit_netlist_bom", "CIRCUIT_DESCRIPTION.md"), "w").write("\n".join(md) + "\n")

with open(os.path.join(PKG, "03_circuit_netlist_bom", "netlist_by_net.csv"), "w", newline="") as f:
    w = csv.writer(f); w.writerow(["net", "pin_count", "pins (ref.pin)"])
    for n in sorted(nets):
        w.writerow([n, len(nets[n]), " ".join(f"{r}.{pn}" for r, pn in sorted(nets[n], key=lambda t: (d.nat(t[0]), int(t[1]))))])

with open(os.path.join(PKG, "03_circuit_netlist_bom", "parts_list_by_reference.csv"), "w", newline="") as f:
    w = csv.writer(f); w.writerow(["ref", "mpn", "manufacturer", "description", "package", "lcsc", "unit_usd_qty1", "lcsc_stock_seen", "footprint_used", "block", "note"])
    for p in parts:
        m = d.MPN.get(p["mpn"]) if p["mpn"] else None
        w.writerow([p["ref"], p["mpn"] or "", *(m[:3] if m else ["", "", ""]), *((m[3], m[4], m[5]) if m else ["", "", ""]), p["fp"], p["block"], p["note"]])

for src, sub in ((os.path.join(ROOT, "quote_jlcpcb", "BOM_revC_JLCPCB.csv"), "03_circuit_netlist_bom"),
                 (os.path.join(ROOT, "quote_jlcpcb", "CPL_revC.csv"), "03_circuit_netlist_bom"),
                 (os.path.join(ROOT, "mfg", "BOM_revC.csv"), "03_circuit_netlist_bom"),
                 (os.path.join(ROOT, "mfg", "CPL_revC_with_values.csv"), "03_circuit_netlist_bom")):
    cp(src, sub)

# ------------------------------------------------------------------ documents
for name in ("DESIGN_DECISIONS_revC.md", "DEVBOARD_PINMAP_revC.md", "HOLISTIC_REVIEW_revC3.md"):
    cp(os.path.join(ROOT, "docs", name), "02_design_documents")
cp(os.path.join(ROOT, "QUOTES_revC.md"), "02_design_documents")

# ------------------------------------------------------------------ KiCad files
cp(PCB, "04_kicad")
cp(os.path.join(ROOT, "kicad", "bilge_n2k_revC_routed.kicad_pro"), "04_kicad")
cp(os.path.join(ROOT, "kicad", "bilge_n2k_revC.kicad_pcb"), "04_kicad", "bilge_n2k_revC_UNROUTED_placement.kicad_pcb")
shutil.copytree(os.path.join(HERE, "fp"), os.path.join(PKG, "04_kicad", "fp"))

# ------------------------------------------------------------------ fabrication files
cp(os.path.join(ROOT, "quote_jlcpcb", "bilge_n2k_revC_gerbers.zip"), "05_fabrication")
for pdf in ("bilge_n2k_revC_assembly_top.pdf", "bilge_n2k_revC_copper_bottom.pdf"):
    cp(os.path.join(ROOT, "mfg", pdf), "05_fabrication")
shutil.copytree(os.path.join(ROOT, "mfg", "gerbers"), os.path.join(PKG, "05_fabrication", "gerbers_unzipped"))

# ------------------------------------------------------------------ renders
R = os.path.join(PKG, "06_renders")
for side in ("top", "bottom"):
    subprocess.run([KCLI, "pcb", "render", "--side", side, "--width", "2400", "--height", "2400", "--quality", "basic", "--background", "opaque", "-o", os.path.join(R, f"board_{side}_3d.png"), PCB], capture_output=True)
subprocess.run([KCLI, "pcb", "render", "--side", "top", "--width", "2400", "--height", "1800", "--quality", "basic", "--perspective", "--rotate", "'-55,0,25'", "--zoom", "0.9", "--background", "opaque", "-o", os.path.join(R, "board_top_angled_3d.png"), PCB], capture_output=True)

# ------------------------------------------------------------------ verification
V = os.path.join(PKG, "08_verification")
subprocess.run([KCLI, "pcb", "drc", "--severity-all", "--format", "json", "-o", os.path.join(V, "drc_report.json"), PCB], capture_output=True)
subprocess.run([KCLI, "pcb", "drc", "--severity-all", "--format", "report", "-o", os.path.join(V, "drc_report.txt"), PCB], capture_output=True)
open(os.path.join(V, "drc_summary.txt"), "w").write(subprocess.run(["python3", os.path.join(HERE, "drc_summary.py"), os.path.join(V, "drc_report.json")], capture_output=True, text=True).stdout)
open(os.path.join(V, "verify_board_output.txt"), "w").write("\n".join(l for l in subprocess.run([KPY, os.path.join(HERE, "verify_board.py")], capture_output=True, text=True).stdout.splitlines()) + "\n")
stats_code = r'''
import pcbnew, collections
b=pcbnew.LoadBoard(r"%s"); mm=pcbnew.ToMM
w=collections.defaultdict(list); L=collections.defaultdict(float)
for t in b.GetTracks():
    if t.GetClass()=="PCB_TRACK": w[t.GetNetname()].append(mm(t.GetWidth())); L[t.GetNetname()]+=mm(t.GetLength())
print("Track width / length per net (mm)")
for n in sorted(w): print(f"{n:14s} min {min(w[n]):.2f} max {max(w[n]):.2f} total length {L[n]:.0f} segments {len(w[n])}")
vias=[t for t in b.GetTracks() if t.GetClass()=="PCB_VIA"]
print("vias",len(vias),"(pad,drill):",sorted({(round(mm(v.GetWidth()),2),round(mm(v.GetDrillValue()),2)) for v in vias}))
dr=collections.Counter()
for f in b.GetFootprints():
    for p in f.Pads():
        dd=p.GetDrillSize().x
        if dd>0: dr[(round(mm(dd),2),"NPTH" if p.GetAttribute()==pcbnew.PAD_ATTRIB_NPTH else "PTH")]+=1
print("pad drills:",dict(dr))
bb=b.GetBoardEdgesBoundingBox(); print("board size mm:",round(mm(bb.GetWidth()),1),"x",round(mm(bb.GetHeight()),1))
print("footprints:",len(list(b.GetFootprints())),"tracks:",len([t for t in b.GetTracks() if t.GetClass()=="PCB_TRACK"]))
''' % PCB
open(os.path.join(V, "_stats.py"), "w").write(stats_code)
out = subprocess.run([KPY, os.path.join(V, "_stats.py")], capture_output=True, text=True).stdout
open(os.path.join(V, "board_stats.txt"), "w").write("\n".join(l for l in out.splitlines() if "assert" not in l and "Debug" not in l) + "\n")
os.remove(os.path.join(V, "_stats.py"))
open(os.path.join(V, "JLCPCB_findings.md"), "w").write(textwrap.dedent("""\
    # JLCPCB tooling findings (2026-10-28)
    - Economic PCBA accepted all 23 BOM lines; no "Standard Only" flag. Extended-part fee $18.54; "Large Size" fee $0 at 100 x 100 mm.
    - U5 (MS5637, C3033010) is flagged "The processing of this component is difficult. After placing an order, it will be reviewed. If PCBA advanced option is required, additional fees will be charged."; it is unticked by default and has to be ticked by hand.
    - BR1, BR2 and PW1 (bare pad rows) are not in the BOM on purpose; JLCPCB asks "Project has unselected parts" -> "Do not place".
    - Placement viewer showed the SOT-23 (x7), TSOT-23-6 (U1) and SOIC-8W (U4) models rotated 180/270/90 degrees from the pads with raw KiCad rotations; the placement file now carries JLCPCB offsets (+180 SOT-23, +270 TSOT-23-6 and SOIC-8) and the viewer then matches pad 1 / silkscreen.
    - Final draft price (5 boards, Economic, top side, Singapore, MS5637 ticked): PCB $4.00 + assembly $85.78 = $89.78, excluding shipping and the dev boards. Not added to the cart; no order placed.
    - JLCPCB's own note: "File preview is for reference only. Check final part placement at DFM Analysis in Order Library in 4-6 hours after placing the order."
    - PCB options left at JLCPCB defaults: FR-4 TG135, 1.6 mm, green, white silk, HASL with lead, 1 oz outer copper, tented vias, 0.3 mm min via drill.
    """))

# ------------------------------------------------------------------ datasheets and sources
for f, n in (("ms5637.pdf", "MS5637-02BA03_TE_datasheet.pdf"), ("dg127.pdf", "DG127S-5.08_Degson_drawing_equivalent_of_DORABO_DB127S.pdf"), ("ap63205.pdf", "AP63205_Diodes_DS41326_datasheet.pdf"), ("l78l.pdf", "L78L_ST_datasheet.pdf"),
             ("is3050.txt", "CA-IS3050G_Chipanalog_datasheet_text_extract.txt"), ("clone_dim.png", "clone_dimensions_from_seller.png"), ("clone_silkscreen_photo.png", "clone_silkscreen_photo_from_listing.png"), ("dk_dim.pdf", "ESP32-S3-DevKitC-1_v1.1_Espressif_dimensions.pdf"), ("dk_dxf.dxf", "ESP32-S3-DevKitC-1_v1.1_Espressif_outline.dxf")):
    cp(os.path.join(SCR, f), "07_datasheets", n)
open(os.path.join(PKG, "07_datasheets", "SOURCES.md"), "w").write(textwrap.dedent("""\
    # Datasheet sources
    - MS5637-02BA03 (TE Connectivity / MEAS), DS 06/2017: te.com document delivery (pin table p.14, package drawing p.14, pad layout p.15).
    - AP63205 (Diodes Inc.) DS41326 Rev 3-2: diodes.com/assets/Datasheets/AP63200-AP63201-AP63203-AP63205.pdf
    - L78L (STMicroelectronics) DS0424 Rev 30: st.com/resource/en/datasheet/l78l.pdf (pin connection Fig. 2, page 3)
    - CA-IS3050G (Chipanalog) Version 1.10: text extract only (original PDF not stored).
    - ESP32-S3-DevKitC-1 v1.1 (Espressif): dimension drawing and DXF from dl.espressif.com; user guide https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html
    - DG127S-5.08 (Degson) drawing 10001534: it.degson.com product page details_552_3552465; used as the equivalent of DORABO DB127S-5.08 (same ratings and height). NOT available to the author (please check): the DORABO DB127S-5.08 drawing itself (LCSC C2997519 / C2997521), the AliExpress DZQJ clone's own schematic and dimensions, Bourns SRN6045TA-4R7M (parametric data taken from JLCPCB's listing), SMBJ15CA hongjiacheng datasheet, Littelfuse 1812L150/24MR datasheet.
    """))

# ------------------------------------------------------------------ generator source
for f in sorted(os.listdir(HERE)):
    if f.endswith((".py", ".sh")):
        cp(os.path.join(HERE, f), "09_generator_source")
cp(os.path.join(ROOT, "mfg", "netlist_revC.csv"), "03_circuit_netlist_bom")

# ------------------------------------------------------------------ README
today = datetime.date.today().isoformat()
open(os.path.join(PKG, "01_read_first", "README_REVIEW_BRIEF.md"), "w").write(textwrap.dedent(f"""\
    # Review brief - Lada Panas bilge monitor, board Rev C.5 ({today})

    You are asked to review a **designed but not yet built or ordered** PCB. Please read this file, then `02_design_documents/HOLISTIC_REVIEW_revC3.md` (the designer's own review: findings, open items, questions for you), and then look at the files you find useful. Be critical: the design and the first review were made by the same AI, so independent eyes are the point. Report anything that would stop the board working, damage it or the boat, or cost a respin, and rank your findings (High / Medium / Low).

    ## What the board is
    A 100 x 100 mm, 2-layer board for a boat's dry helm compartment (behind a Raymarine Axiom MFD) that
    - monitors three bilge-pump "running" signals and one high-water float switch (12 V signals, 8-16 V = active),
    - drives an external warning light and an external buzzer (switched +12 V, high side),
    - reports on **NMEA 2000** through an **isolated CAN transceiver** (CA-IS3050G, 5 kVrms) so a fault cannot reach the bus devices,
    - carries an on-board barometric pressure/temperature sensor (MS5637), two LEDs, a mute button,
    - plugs in an **ESP32-S3-DevKitC-1 (clone) dev board** on two 1x22 sockets and breaks out all 44 dev-board pins to numbered solder pads.
    A hard-wired path turns both outputs on when the float input is active, without the ESP32.
    Power: boat 12 V battery (10-15 V normal, up to ~16 V charging, 3 A external fuse), reverse-polarity P-FET, bidirectional TVS, resettable fuse, 5 V buck (AP63205) to the dev board's 5V pin; the dev board makes 3.3 V. NMEA 2000 side is powered from the bus (9-16 V) through SS14 + L78L05 and is galvanically isolated from the rest.
    Firmware is not written yet.

    ## Ordering plan
    JLCPCB only: PCB + Economic assembly of 5 boards (top side SMT, terminals and sockets soldered by JLCPCB), about US$90 excluding shipping and dev boards. The order has NOT been placed. A JLCPCB quote draft exists (not in the cart).

    ## Where things are
    | Folder | Contents |
    |---|---|
    | 01_read_first | this brief |
    | 02_design_documents | design decisions, dev-board pin map, **HOLISTIC_REVIEW_revC3.md**, quotes/sensor comparison |
    | 03_circuit_netlist_bom | **CIRCUIT_DESCRIPTION.md** (every part, pin and net, per block), netlist by net, parts list, JLCPCB BOM and placement file, full BOM with prices |
    | 04_kicad | routed board (`bilge_n2k_revC_routed.kicad_pcb`, KiCad 10), project file, unrouted placement, custom MS5637 footprint. There is **no schematic drawing**: the netlist is generated from Python (`09_generator_source/design_revC.py`) and the board is built from it |
    | 05_fabrication | Gerber/drill zip as sent to JLCPCB, unzipped copies, assembly and bottom-copper PDFs |
    | 06_renders | 3D renders top/bottom/angled |
    | 07_datasheets | the datasheets the author could read, and a list of those he could not |
    | 08_verification | DRC report, pad/net check, isolation check, track/via/drill statistics, JLCPCB tooling findings |
    | 09_generator_source | the Python/shell pipeline (netlist -> board -> autoroute -> DRC -> export) |

    ## Known facts that look odd but are intended
    - Placement-file rotations differ from KiCad's for SOT-23, TSOT-23-6 and SOIC-8 parts (JLCPCB library offsets, verified in their viewer).
    - BR1, BR2, PW1 are bare pad rows (no part); the BOM lists 23 lines / 77 placed parts; the dev board itself is not on the BOM.
    - IN5 was dropped; the board has four inputs (IN1-IN3 pump sense, IN4 = float).
    - Only the CAN side is isolated; the 12 V side is non-isolated by design (owner's choice for cost).
    - The final DRC runs at 0.18 mm clearance while the autorouter works to 0.20 mm (JLCPCB's limit is 0.127 mm).

    ## Please focus on
    1. The questions in section 6 of HOLISTIC_REVIEW_revC3.md.
    2. Anything that disagrees between the netlist (03), the board (04) and the datasheets (07).
    3. Any item the author marked unverified (O2-O3, and the DORABO terminal in F6) that you can close.
    """))

# ------------------------------------------------------------------ zip
if os.path.exists(ZIP):
    os.remove(ZIP)
with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(PKG):
        for f in sorted(files):
            full = os.path.join(base, f)
            z.write(full, os.path.relpath(full, os.path.dirname(PKG)))
print("zip:", ZIP, round(os.path.getsize(ZIP) / 1e6, 2), "MB")
