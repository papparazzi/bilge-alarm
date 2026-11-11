"""Stage C: import the Freerouting session, widen sliver tracks, fill pours, stitch pours with vias."""
import os, math, pcbnew
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kicad")
b = pcbnew.LoadBoard(os.path.join(D, "bilge_n2k_revC.kicad_pcb"))
ok = pcbnew.ImportSpecctraSES(b, os.path.join(D, "bilge_n2k_revC.ses"))
print("ses import", ok)
V = pcbnew.VECTOR2I
mm = pcbnew.FromMM
for t in b.GetTracks():
    if t.GetClass() == "PCB_TRACK" and t.GetWidth() < mm(0.2):
        t.SetWidth(mm(0.2))
for z in b.Zones():
    if not z.GetIsRuleArea():
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
fill = lambda: pcbnew.ZONE_FILLER(b).Fill(b.Zones())
fill()
pth = [q for f in b.GetFootprints() for q in f.Pads() if q.GetDrillSize().x > 0]


def zones(net):
    zs = [z for z in b.Zones() if z.GetNetname() == net and not z.GetIsRuleArea()]
    return ([z for z in zs if z.GetLayer() == pcbnew.F_Cu][0], [z for z in zs if z.GetLayer() == pcbnew.B_Cu][0])


def free(zf, zb, x, y, m):
    return all(z.HitTestFilledArea(l, V(mm(x + dx), mm(y + dy)), 0) for z, l in ((zf, pcbnew.F_Cu), (zb, pcbnew.B_Cu))
               for dx, dy in ((0, 0), (m, 0), (-m, 0), (0, m), (0, -m)))


def add_via(net, x, y):
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(V(mm(x), mm(y)))
    v.SetWidth(mm(0.6)); v.SetDrill(mm(0.3))
    v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(net)
    b.Add(v)


total = 0
for netname, (x0, y0, x1, y1) in (("GND", (1.5, 1.5, 99, 69)), ("NET_C", (1.5, 45.5, 25, 69))):
    zf, zb = zones(netname)
    net = zf.GetNet()
    y = y0
    while y < y1:
        x = x0
        while x < x1:
            p = V(mm(x), mm(y))
            near_pth = any(math.hypot(q.GetPosition().x - p.x, q.GetPosition().y - p.y) < mm(2.0) for q in pth)
            if not near_pth and free(zf, zb, x, y, 0.7):
                add_via(net, x, y); total += 1
            x += 4.0
        y += 4.0
print("stitching vias", total)
fill()
# islands on the top pour that hold pads but no via: put a via at a free spot inside the island
fixed = 0
for netname in ("GND", "NET_C"):
    zf, zb = zones(netname)
    net = zf.GetNet()
    for rnd in range(3):
        vias = [t for t in b.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == netname]
        ps = zf.GetFilledPolysList(pcbnew.F_Cu)
        n = 0
        for i in range(ps.OutlineCount()):
            if any(ps.Contains(v.GetPosition(), i) for v in vias):
                continue
            bb = ps.Outline(i).BBox()
            done = False
            yy = pcbnew.ToMM(bb.GetTop())
            while yy <= pcbnew.ToMM(bb.GetBottom()) and not done:
                xx = pcbnew.ToMM(bb.GetLeft())
                while xx <= pcbnew.ToMM(bb.GetRight()) and not done:
                    if ps.Contains(V(mm(xx), mm(yy)), i) and free(zf, zb, xx, yy, 0.3):
                        add_via(net, xx, yy); n += 1; done = True
                    xx += 0.1
                yy += 0.1
        fixed += n
        if not n:
            break
        fill()
print("island vias added", fixed)
# drop duplicate vias (same position) left by the pre-placed + stitching passes
seen = []
for t in list(b.GetTracks()):
    if t.GetClass() == "PCB_VIA":
        p = t.GetPosition()
        if any(math.hypot(p.x - q.x, p.y - q.y) < mm(0.4) for q in seen):
            b.Remove(t)
        else:
            seen.append(p)
# remove signal vias that connect on only one layer (router leftovers)
def layers_at(pos, net):
    ls = set()
    for t in b.GetTracks():
        if t.GetClass() == "PCB_TRACK" and t.GetNetname() == net:
            for e in (t.GetStart(), t.GetEnd()):
                if math.hypot(e.x - pos.x, e.y - pos.y) < mm(0.02):
                    ls.add(t.GetLayer())
    for f in b.GetFootprints():
        for q in f.Pads():
            if q.GetNetname() == net and math.hypot(q.GetPosition().x - pos.x, q.GetPosition().y - pos.y) < mm(0.4):
                ls.add(q.GetLayer())
    return ls


for t in list(b.GetTracks()):
    if t.GetClass() == "PCB_VIA" and t.GetNetname() not in ("GND", "NET_C"):
        if len(layers_at(t.GetPosition(), t.GetNetname())) < 2:
            print("removing dangling via", t.GetNetname(), round(pcbnew.ToMM(t.GetPosition().x), 1), round(pcbnew.ToMM(t.GetPosition().y), 1))
            b.Remove(t)
fill()
pcbnew.SaveBoard(os.path.join(D, "bilge_n2k_revC_routed.kicad_pcb"), b)
print("tracks", len(b.GetTracks()))
