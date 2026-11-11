"""Stage A: build the Rev C KiCad board (footprints, nets, outline, zones) from design_revC.py.
Run with KiCad's python:
  ~/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 build_pcb.py
Writes ../kicad/bilge_n2k_revC.kicad_pcb and .kicad_pro
"""
import os, sys, json
import pcbnew
from pcbnew import FromMM as mm, VECTOR2I as V

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design_revC as d

FPDIR = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")
OUT = os.path.join(HERE, "..", "kicad")
os.makedirs(OUT, exist_ok=True)
W, H = 100.0, 100.0
# isolation barrier (CA-IS3050G pad gap 8.86 mm): bus copper x < XB0 for y > YB, logic copper x > XB1
XB0, XB1, YB = 25.5, 34.5, 75.0
YL = 67.0          # logic copper must stay above y = YL on the bus side (x < XB1)

# ---------------------------------------------------------------- placement table (mm, rot deg CCW)
# Dev board (Espressif ESP32-S3-DevKitC-1 v1.1 pattern, from Espressif's DXF), mounted HORIZONTALLY: USB end flush with the right edge, antenna end left.
# Seen from above the J3 row is the TOP row and the J1 row is the BOTTOM row (rotating the official picture 90 deg CCW), pin 1 of both rows at the left (antenna) end.
# CLONE geometry (seller's dimension drawing, 2026-10-29): board 27.94 x 57.15 mm, rows 25.40 mm apart, 53.34 mm pin span, 2.54 mm pitch.
# (Espressif's official board is 25.4 x 62.865 with rows 22.86 mm apart - NOT what is used.) Pin margins at the ends assumed equal: (57.15-53.34)/2 = 1.905 mm.
DEV_W, DEV_L, ROWSEP = 27.94, 57.15, 25.40
XP1 = W - 1.905 - 53.34           # x of pin 1 (44.755), USB end flush with the right edge
YT = 13.27                       # top row (J3 pins)
YBt = YT + ROWSEP                # bottom row (J1 pins)
YBR2 = 9.5                       # breakout pads for the J3 pins (top edge, opposite the connectors)
YBR1 = YBt + 4.07                # breakout pads for the J1 pins (below the dev board)
YC = 78.0                        # connector pin row (all connectors along the bottom edge, wire entry facing down)
P = {
    # mechanical: M3 holes in all four corners
    "H1": (2.6, 2.6, 0), "H2": (97.4, 2.6, 0), "H3": (2.6, 80.5, 0), "H4": (97.4, 80.5, 0),
    # dev-board sockets and breakout pad rows (rot 90: pin 1 at the left, pins run along +x)
    "HDR2": (XP1, YT, 90), "HDR1": (XP1, YBt, 90), "BR2": (XP1, YBR2, 90), "BR1": (XP1, YBR1, 90),
    "PW1": (XP1 - 3 * 2.54 - 1.5, YBR2, 90),
    # connectors along the bottom edge, rotated 180 so the wire entry faces the edge (pin 1 at the right)
    "J4": (23.0, YC, 180), "J1": (46.0, YC, 180), "J3": (69.0, YC, 180), "J2": (91.0, YC, 180),
    # power entry (above J1)
    "D1": (46.0, 67.0, 0), "F1": (46.0, 61.5, 0), "Q1": (51.6, 67.0, 0), "R1": (51.6, 63.0, 90), "R2": (51.6, 59.5, 90),
    "C1": (40.6, 56.5, 0), "C2": (44.0, 56.5, 0), "C3": (47.4, 56.5, 0), "C4": (50.8, 56.5, 0),
    "R16": (42.0, 52.0, 0), "R17": (45.5, 52.0, 0), "C14": (49.0, 52.0, 0),
    # 5 V buck (left middle), D8 towards the dev board 5V pin
    "U1": (24.0, 27.0, 0), "R3": (28.0, 24.5, 90), "R4": (28.0, 29.0, 90), "C5": (24.0, 31.5, 0), "L1": (24.0, 38.5, 0),
    "C6": (31.5, 36.0, 0), "C7": (31.5, 39.0, 0), "C8": (31.5, 42.0, 0), "D8": (37.0, 52.5, 0),
    # BME280 (top-left corner, far from the buck and the module) with its bypass and pull-ups
    "U5": (9.0, 10.0, 0), "C19": (13.0, 6.5, 0), "R25": (16.5, 7.0, 90), "R26": (19.0, 7.0, 90),
    # LEDs, MUTE button (right edge strip, spaced apart)
    "R5": (97.8, 40.8, 90), "D2": (97.8, 44.3, 90), "SW3": (97.0, 52.5, 0), "R7": (97.8, 58.5, 90), "R6": (97.8, 63.0, 90), "D3": (97.8, 66.5, 90),
    # input dividers, one column per input above its terminal (raw at the bottom, divided node at the top)
    "R8": (91.0, 64.0, 90), "R9": (91.0, 59.2, 90), "C10": (91.0, 54.4, 90),
    "R10": (85.92, 64.0, 90), "R11": (85.92, 59.2, 90), "C11": (85.92, 54.4, 90),
    "R12": (80.84, 64.0, 90), "R13": (80.84, 59.2, 90), "C12": (80.84, 54.4, 90),
    "R14": (75.76, 64.0, 90), "R15": (75.76, 59.2, 90), "C13": (75.76, 54.4, 90),
    # outputs (above J3)
    "R18": (56.0, 50.5, 0), "R19": (59.5, 50.5, 0), "R20": (63.0, 50.5, 0), "R21": (66.5, 50.5, 0), "R22": (70.0, 50.5, 0), "R23": (73.5, 50.5, 0),
    "Q2": (56.5, 56.0, 0), "Q3": (61.5, 56.0, 0), "Q4": (66.5, 56.0, 0), "Q5": (71.5, 56.0, 0),
    "R27": (56.5, 61.0, 0), "R28": (60.0, 61.0, 0), "R29": (63.5, 61.0, 0), "R30": (67.0, 61.0, 0),
    "Q6": (68.5, 65.5, 0), "Q7": (60.5, 65.5, 0), "D4": (66.0, 70.5, 0), "D5": (58.0, 70.5, 0),
    # isolated NMEA 2000 side (bottom left)
    "D6": (10.0, 67.0, 0), "D7": (10.0, 61.5, 0), "C15": (17.5, 61.5, 0), "U3": (17.0, 67.5, 0), "C16": (22.5, 66.0, 90), "C17": (22.5, 62.0, 90),
    "U4": (30.0, 66.0, 180), "C18": (38.0, 66.0, 90), "R24": (38.0, 61.0, 90), "C9": (38.0, 56.0, 90),
}
for _k, (_x, _y, _r) in list(P.items()):
    # (shift applied below)
    if _y >= 47 and _k not in ("D8", "H3", "H4"):
        P[_k] = (_x, _y + 16.0, _r)
P["H3"] = (2.6, 97.4, 0); P["H4"] = (97.4, 97.4, 0)
# LEDs and MUTE button on the right edge strip, well spaced (status LED top, alarm LED bottom)
P.update({
    "R18": (56.0, 62.0, 0), "R19": (59.5, 62.0, 0), "R20": (63.0, 62.0, 0), "R21": (66.5, 62.0, 0), "R22": (70.0, 62.0, 0), "R23": (73.5, 62.0, 0),
    "Q2": (56.5, 68.0, 0), "Q3": (61.5, 68.0, 0), "Q4": (66.5, 68.0, 0), "Q5": (71.5, 68.0, 0),
    "R27": (55.5, 73.8, 0), "R28": (59.5, 73.8, 0), "R29": (63.5, 73.8, 0), "R30": (67.5, 73.8, 0),
    "Q6": (69.0, 79.0, 0), "Q7": (60.0, 79.0, 0), "D4": (66.5, 83.2, 0), "D5": (59.0, 83.2, 0),
})
P.update({"R5": (97.8, 54.0, 90), "D2": (97.8, 57.5, 90), "SW3": (96.0, 68.0, 0), "R7": (97.8, 74.0, 90), "R6": (97.8, 80.0, 90), "D3": (97.8, 83.5, 90)})


board = pcbnew.BOARD()
board.SetCopperLayerCount(2)

netitems = {}
for n in sorted(d.nets()):
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    netitems[n] = ni


def load(fpstr):
    lib, name = fpstr.split(":")
    base = os.path.join(HERE, "fp") if lib == "CUSTOM" else FPDIR
    f = pcbnew.FootprintLoad(os.path.join(base, lib + ".pretty"), name)
    assert f is not None, fpstr
    return f


placed = {}
for p in d.parts:
    ref = p["ref"]
    f = load(p["fp"])
    if ref in ("J1", "J2", "J3", "J4"):   # DG127S-5.08 drawing: recommended hole 1.40 mm
        for q in f.Pads():
            q.SetDrillSize(V(mm(1.4), mm(1.4)))
    f.SetReference(ref)
    f.SetValue(p["mpn"] or "M3")
    x, y, rot = P[ref]
    f.SetPosition(V(mm(x), mm(y)))
    f.SetOrientationDegrees(rot)
    if ref in ("BR1", "BR2"):             # breakout rows carry only the free pins: drop the pads of used pins
        for q in list(f.Pads()):
            if q.GetNumber() not in p["pins"]:
                f.Remove(q)
    for pin, net in p["pins"].items():
        pads = [q for q in f.Pads() if q.GetNumber() == pin]
        assert pads, (ref, pin)
        for q in pads:
            q.SetNet(netitems[net])
    # smaller reference text
    t = f.Reference()
    t.SetTextSize(V(mm(0.8), mm(0.8)))
    t.SetTextThickness(mm(0.12))
    f.Value().SetVisible(False)
    if ref in ("J1", "J2", "J3", "J4", "SW3", "HDR1", "HDR2", "H1", "H2", "H3", "H4", "BR1", "BR2", "PW1"):
        f.Reference().SetVisible(False)
    if ref.startswith("H") and ref[1:].isdigit():      # mounting holes: drop the 3.5 mm courtyard (screw heads sit on the other side of the board)
        for g in list(f.GraphicalItems()):
            if g.GetLayer() == pcbnew.F_CrtYd or g.GetLayer() == pcbnew.B_CrtYd:
                f.Remove(g)
    if ref in ("HDR1", "HDR2"):                        # socket body outline duplicates the dev-board outline legend and collides with it
        for g in list(f.GraphicalItems()):
            if g.GetLayer() == pcbnew.F_SilkS:
                f.Remove(g)
    board.Add(f)
    placed[ref] = f
missing = [p["ref"] for p in d.parts if p["ref"] not in P]
assert not missing, missing

# pre-placed GND vias beside the divider bottom resistors, so their GND pads never end up on a pour island
for ref in ("R9", "R11", "R13", "R15", "R17"):
    q = [q for q in placed[ref].Pads() if q.GetNumber() == "2"][0]
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(V(q.GetPosition().x + mm(1.5), q.GetPosition().y))
    v.SetWidth(mm(0.6)); v.SetDrill(mm(0.3))
    v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(netitems["GND"])
    board.Add(v)

# same for the 3V3 decoupling caps beside the module: the via goes on the far side of the GND pad
for ref in ("C6", "C7", "C8", "C10", "C11", "C12", "C13", "C14", "C18", "C19"):
    q1 = [q for q in placed[ref].Pads() if q.GetNumber() == "1"][0]
    q2 = [q for q in placed[ref].Pads() if q.GetNumber() == "2"][0]
    dx, dy = q2.GetPosition().x - q1.GetPosition().x, q2.GetPosition().y - q1.GetPosition().y
    n_ = (dx * dx + dy * dy) ** 0.5
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(V(int(q2.GetPosition().x + dx / n_ * mm(1.5)), int(q2.GetPosition().y + dy / n_ * mm(1.5))))
    v.SetWidth(mm(0.6)); v.SetDrill(mm(0.3))
    v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(netitems["GND"])
    board.Add(v)

for (cx_, cy_) in ((1.0, 1.0), (W - 1.0, 1.0), (1.0, H - 1.0), (W - 1.0, H - 1.0)):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(V(mm(cx_), mm(cy_)))
    v.SetWidth(mm(0.6)); v.SetDrill(mm(0.3))
    v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(netitems["NET_C" if (cx_ < XB0 and cy_ > YB) else "GND"])   # bottom-left corner is on the bus-side (isolated) ground
    board.Add(v)

# buck GND pad (U1 pin 4): explicit stub to a via so its pour island is tied to the bottom plane
_p4 = [q for q in placed["U1"].Pads() if q.GetNumber() == "4"][0]
_x4, _y4 = pcbnew.ToMM(_p4.GetPosition().x), pcbnew.ToMM(_p4.GetPosition().y)
_v = pcbnew.PCB_VIA(board)
_v.SetPosition(V(mm(_x4), mm(_y4 + 1.4)))
_v.SetWidth(mm(0.6)); _v.SetDrill(mm(0.3))
_v.SetViaType(pcbnew.VIATYPE_THROUGH); _v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
_v.SetNet(netitems["GND"])
board.Add(_v)
_t = pcbnew.PCB_TRACK(board)
_t.SetStart(V(mm(_x4), mm(_y4))); _t.SetEnd(V(mm(_x4), mm(_y4 + 1.4)))
_t.SetWidth(mm(0.25)); _t.SetLayer(pcbnew.F_Cu); _t.SetNet(netitems["GND"])
board.Add(_t)
# second via 1.5 mm to the right on the same stub (the router can otherwise wall in a single via on the bottom layer)
_v2 = pcbnew.PCB_VIA(board)
_v2.SetPosition(V(mm(_x4 + 1.5), mm(_y4 + 1.4)))
_v2.SetWidth(mm(0.6)); _v2.SetDrill(mm(0.3))
_v2.SetViaType(pcbnew.VIATYPE_THROUGH); _v2.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
_v2.SetNet(netitems["GND"])
board.Add(_v2)
_t2 = pcbnew.PCB_TRACK(board)
_t2.SetStart(V(mm(_x4), mm(_y4 + 1.4))); _t2.SetEnd(V(mm(_x4 + 1.5), mm(_y4 + 1.4)))
_t2.SetWidth(mm(0.25)); _t2.SetLayer(pcbnew.F_Cu); _t2.SetNet(netitems["GND"])
board.Add(_t2)

# ---------------------------------------------------------------- outline
def line(layer, a, b, w=0.1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(V(mm(a[0]), mm(a[1])))
    s.SetEnd(V(mm(b[0]), mm(b[1])))
    s.SetLayer(layer)
    s.SetWidth(mm(w))
    board.Add(s)


pts = [(0, 0), (W, 0), (W, H), (0, H)]
for i in range(4):
    line(pcbnew.Edge_Cuts, pts[i], pts[(i + 1) % 4], 0.1)

# ---------------------------------------------------------------- silkscreen text
def text(s, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(V(mm(x), mm(y)))
    t.SetLayer(layer)
    t.SetTextSize(V(mm(max(size, 0.8)), mm(max(size, 0.8))))
    t.SetTextThickness(mm(0.15))
    t.SetTextAngleDegrees(rot)
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    board.Add(t)


def ktext(s, x, y, size=1.2, rot=0):
    """Bold white box with the letters knocked out (transparent), for the connector and LED labels."""
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(V(mm(x), mm(y)))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(V(mm(size), mm(size)))
    t.SetTextThickness(mm(0.22))
    t.SetBold(True)
    t.SetIsKnockout(True)
    t.SetTextAngleDegrees(rot)
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    board.Add(t)


text("BILGE MONITOR  REV C.5", 4.0, 14.5, 1.0)
text("EXTERNAL 3A FUSE REQ.", 4.0, 17.0, 0.8)
text("ISOLATED NMEA 2000 SIDE", 3.0, 74.6, 0.8)

# M/Y Lada Panas logo (silkscreen): 32 mm circle in the free area at the left of the board; polygons made by gen/make_logo.py
import json as _json
_lg = _json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "logo_polys.json")))
LOGO_X, LOGO_Y = 17.5, 57.3
for _pl in _lg["polys"]:
    _ps = pcbnew.SHAPE_POLY_SET()
    _ps.NewOutline()
    for _x, _y in _pl["outer"]:
        _ps.Append(mm(LOGO_X + _x), mm(LOGO_Y + _y))
    for _h in _pl["holes"]:
        _hi = _ps.NewHole()
        for _x, _y in _h:
            _ps.Append(mm(LOGO_X + _x), mm(LOGO_Y + _y), 0, _hi)
    _sh = pcbnew.PCB_SHAPE(board)
    _sh.SetShape(pcbnew.SHAPE_T_POLY)
    _sh.SetFilled(True)
    _sh.SetWidth(0)
    _sh.SetLayer(pcbnew.F_SilkS)
    _sh.SetPolyShape(_ps)
    board.Add(_sh)

# connector pin labels (bold, knocked out), one box per terminal above the connector body
def pin_labels(ref, labels):
    pads = sorted([q for q in placed[ref].Pads()], key=lambda q: int(q.GetNumber()))
    for q, s_ in zip(pads, labels):
        ktext(s_, pcbnew.ToMM(q.GetPosition().x), YC + 16.0 - 6.6, 1.2)


pin_labels("J1", ["+12V", "GND"])
pin_labels("J3", ["LGT+", "GND", "BUZ+", "GND"])
pin_labels("J2", ["IN1", "IN2", "IN3", "FLT"])
pin_labels("J4", ["NETS", "NETC", "CANH", "CANL"])
# LED / button labels, vertical, left of each part
ktext("STATUS", 94.5, 57.5, 1.2, rot=90)
ktext("MUTE", 90.8, 64.6, 1.2, rot=90)
ktext("ALARM", 94.5, 83.5, 1.2, rot=90)

# dev board outline (silk): USB end flush with the right edge, antenna end left; J3 row on top, J1 row below
dx0, dx1 = W - DEV_L - 0.03, W - 0.25      # outline kept 0.15 mm inside the board edge (silk is clipped at the edge)
dy0, dy1 = YT - (DEV_W - ROWSEP) / 2 - 0.03, YBt + (DEV_W - ROWSEP) / 2 + 0.03
for a_, b_ in (((dx0, dy0), (dx1, dy0)), ((dx1, dy0), (dx1, dy1)), ((dx1, dy1), (dx0, dy1)), ((dx0, dy1), (dx0, dy0))):
    line(pcbnew.F_SilkS, a_, b_, 0.2)
text("ESP32-S3-DEVKITC-1 (N8R2 / N16R8)", dx0 + 2.0, dy0 + 5.0, 0.9)
text("ANTENNA END", dx0 + 2.0, dy0 + 8.0, 0.8)
text("USB END (FLUSH WITH EDGE) >>", dx0 + 22.0, dy0 + 12.0, 0.8)
text("TOP ROW = J3 PINS, BOTTOM ROW = J1 PINS, LEFT = PIN 1", dx0 + 2.0, YBt - 3.8, 0.7)

# breakout pad rows: only the unused dev-board pins (plus SDA/SCL) are broken out, numbered "pin:GPIO" (J3 row on the top edge, J1 row below the dev board)
J3L = {1: "GND", 2: "TX", 3: "RX", 4: "IO1", 5: "IO2", 6: "SDA", 7: "SCL", 8: "IO40", 9: "IO39", 10: "IO38", 11: "IO37*", 12: "IO36*", 13: "IO35*", 14: "IO0",
       15: "IO45", 16: "IO48", 17: "IO47", 18: "IO21", 19: "IO20U", 20: "IO19U", 21: "GND", 22: "GND"}
J1L = {1: "3V3", 2: "3V3", 3: "RST", 4: "RX-CAN", 5: "TX-CAN", 6: "VBAT", 7: "IO7", 8: "IO15", 9: "LGT", 10: "BUZ", 11: "IO18", 12: "MUTE", 13: "IO3", 14: "IO46",
       15: "IN4", 16: "IN3", 17: "IN2", 18: "IN1", 19: "STAT", 20: "ALM", 21: "5V", 22: "GND"}
d_br1 = next(p_ for p_ in d.parts if p_['ref'] == 'BR1')['pins']
d_br2 = next(p_ for p_ in d.parts if p_['ref'] == 'BR2')['pins']
for k in range(1, 23):
    x_ = XP1 + 2.54 * (k - 1)
    lab2 = {19: "19:20U", 20: "20:19U", 21: "21G", 22: "22G"}.get(k, f"{k}:{J3L[k]}")   # short near the corner hole
    if str(k) in d_br2:
        text(lab2, x_, YBR2 - 1.6, 0.9, rot=90)           # reads upward from the pad
    if str(k) in d_br1:
        text(f"{k}:{J1L[k]}", x_, YBR1 + 1.6, 0.9, rot=270)         # reads downward from the pad
text("DEV J3 ROW (TOP): FREE PINS. SDA=IO42 SCL=IO41. *IO35-37 NOT ON N16R8. U=USB D+/D-", 12.0, 1.2, 0.7)
text("DEV J1 ROW (BOTTOM): IN1-4 = IO12,11,10,9  VBAT = IO6", XP1, YBR1 + 12.0, 0.7)
for i_, s_ in enumerate(("3V3", "5V", "GND")):
    text(s_, XP1 - 3 * 2.54 + 2.54 * i_, YBR2 - 1.6, 0.9, rot=90)

# ---------------------------------------------------------------- zones
def zone(net, layer, poly, name):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(netitems[net])
    ol = z.Outline()
    ol.NewOutline()
    for (x, y) in poly:
        ol.Append(mm(x), mm(y))
    z.SetMinThickness(mm(0.25))
    z.SetLocalClearance(mm(0.3))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetThermalReliefGap(mm(0.3))
    z.SetThermalReliefSpokeWidth(mm(0.4))
    z.SetZoneName(name)
    board.Add(z)
    return z


E_ = 0.3
logic = [(E_, E_), (W - E_, E_), (W - E_, H - E_), (XB1, H - E_), (XB1, YL), (E_, YL)]
bus = [(E_, YB), (XB0, YB), (XB0, H - E_), (E_, H - E_)]
for layer, nm in ((pcbnew.F_Cu, "F"), (pcbnew.B_Cu, "B")):
    zone("GND", layer, logic, f"GND_{nm}")
    zone("NET_C", layer, bus, f"NETC_{nm}")

# keep-out (no copper, tracks or vias) across the isolation barrier; U4's pads sit just outside it
ko = pcbnew.ZONE(board)
ko.SetIsRuleArea(True)
ko.SetLayerSet(pcbnew.LSET.AllCuMask())
ko.SetDoNotAllowZoneFills(True)
ko.SetDoNotAllowTracks(True)
ko.SetDoNotAllowVias(True)
ko.SetDoNotAllowPads(False)
ko.SetDoNotAllowFootprints(False)
ko.SetZoneName("ISOLATION_BARRIER")
ol = ko.Outline()
ol.NewOutline()
for (x, y) in ((25.7, YL), (34.3, YL), (34.3, H), (25.7, H)):
    ol.Append(mm(x), mm(y))
board.Add(ko)

# MS5637 datasheet (06/2017 p.15): "Reserved area: please do not route tracks between pads" -> keep-out cross between the four pads, top layer
_ux, _uy = pcbnew.ToMM(placed["U5"].GetPosition().x), pcbnew.ToMM(placed["U5"].GetPosition().y)
for _i, (_x0, _y0, _x1, _y1) in enumerate(((-0.45, -1.6, 0.45, 1.6), (-1.6, -0.45, 1.6, 0.45))):
    _k = pcbnew.ZONE(board)
    _k.SetIsRuleArea(True)
    _ls = pcbnew.LSET(); _ls.AddLayer(pcbnew.F_Cu)
    _k.SetLayerSet(_ls)
    _k.SetDoNotAllowZoneFills(True); _k.SetDoNotAllowTracks(True); _k.SetDoNotAllowVias(True)
    _k.SetDoNotAllowPads(False); _k.SetDoNotAllowFootprints(False)
    _k.SetZoneName(f"U5_RESERVED_{_i}")
    _o = _k.Outline(); _o.NewOutline()
    for (_a, _b) in ((_x0, _y0), (_x1, _y0), (_x1, _y1), (_x0, _y1)):
        _o.Append(mm(_ux + _a), mm(_uy + _b))
    board.Add(_k)

board.Save(os.path.join(OUT, "bilge_n2k_revC.kicad_pcb"))

# ---------------------------------------------------------------- project file with net classes
power_nets = ["VIN", "VFUSED", "VBAT_IN", "+5V", "DEV_5V", "SW", "LIGHT_OUT", "BUZZ_OUT", "NET_SP", "+5V_ISO", "NET_S"]
pro = {
    "meta": {"filename": "bilge_n2k_revC.kicad_pro", "version": 3},
    "board": {"design_settings": {"rules": {"min_clearance": 0.2, "min_track_width": 0.15, "min_through_hole_diameter": 0.3, "min_hole_to_hole": 0.25,
                                            "min_via_diameter": 0.5, "min_copper_edge_clearance": 0.3}}},
    "net_settings": {
        "classes": [
            {"name": "Default", "clearance": 0.2, "track_width": 0.25, "via_diameter": 0.6, "via_drill": 0.3},
            {"name": "Power", "clearance": 0.2, "track_width": 0.6, "via_diameter": 0.8, "via_drill": 0.4},
        ],
        "meta": {"version": 3},
        "netclass_patterns": [{"netclass": "Power", "pattern": n} for n in power_nets],
    },
}
with open(os.path.join(OUT, "bilge_n2k_revC.kicad_pro"), "w") as f:
    json.dump(pro, f, indent=2)
print("board saved:", len(placed), "footprints,", len(netitems), "nets")
