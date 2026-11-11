"""Cross-checks the routed board against design_revC.py: pad nets, isolation barrier, routed-ness."""
import os, sys
import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design_revC as d
b = pcbnew.LoadBoard(os.path.join(HERE, "..", "kicad", "bilge_n2k_revC_routed.kicad_pcb"))
mm = pcbnew.ToMM
bad = 0
fps = {f.GetReference(): f for f in b.GetFootprints()}
for p in d.parts:
    f = fps[p["ref"]]
    for pin, net in p["pins"].items():
        pads = [q for q in f.Pads() if q.GetNumber() == pin]
        for q in pads:
            if q.GetNetname() != net:
                print("NET MISMATCH", p["ref"], pin, q.GetNetname(), net); bad += 1
print("pad/net check:", "OK" if not bad else f"{bad} problems")
# isolation: bus-side nets vs everything else
BUS = {"NET_S", "NET_C", "NET_SP", "+5V_ISO", "CANH_J", "CANL_J"}
def bb(item):
    r = item.GetBoundingBox(); return mm(r.GetLeft()), mm(r.GetTop()), mm(r.GetRight()), mm(r.GetBottom())
items = []
for t in b.GetTracks(): items.append((t.GetNetname(), bb(t), "track"))
for f in b.GetFootprints():
    for q in f.Pads():
        if q.GetNetname(): items.append((q.GetNetname(), bb(q), f.GetReference() + "." + q.GetNumber()))
viol = 0
for net, (x0, y0, x1, y1), what in items:
    if net in BUS and (x1 > 26.5 or y0 < 44.0):
        # CAN lines from U4 pads 6,7 are bus nets that stay left of the gap
        print("bus item outside bus region:", net, what, round(x1, 1), round(y0, 1)); viol += 1
    if net not in BUS and not what.startswith("U4.") and x0 < 33.2 and y1 > 67.0 and y0 > 67.0 - 0.01 and not net in ("",):
        # logic copper must not sit in the barrier region (x<34.4, y>67) except GND pour and mounting hole
        print("logic item in barrier region:", net, what, round(x0, 1), round(y0, 1)); viol += 1
print("isolation check:", "OK" if not viol else f"{viol} problems")
# pour: bus zone and logic zone must not overlap the gap
for z in b.Zones():
    if z.GetIsRuleArea():
        continue
    ps = z.GetFilledPolysList(z.GetLayer())
    for i in range(ps.OutlineCount()):
        r = ps.Outline(i).BBox()
        x0, y0, x1, y1 = mm(r.GetLeft()), mm(r.GetTop()), mm(r.GetRight()), mm(r.GetBottom())
        if z.GetNetname() == "NET_C" and (x1 > 25.6 or y0 < 44.0): print("bus pour out of region", x1, y0); viol += 1
        if z.GetNetname() == "GND" and x0 < 34.3 and y1 > 67.0 and y0 > 67.0 - 0.01: print("GND island in barrier", round(x0,1), round(y0,1)); viol += 1
bus_x = max(x1 for net, (x0, y0, x1, y1), what in items if net in BUS)
log_x = min(x0 for net, (x0, y0, x1, y1), what in items if net not in BUS and net and y0 > 67.0 and x0 < 45 and x1 > 26)
print("barrier: bus copper max x = %.2f mm, logic copper min x (y>67) = %.2f mm, gap = %.2f mm (datasheet package clearance 8 mm)" % (bus_x, log_x, log_x - bus_x))
print("tracks", len(b.GetTracks()), "footprints", len(fps))
