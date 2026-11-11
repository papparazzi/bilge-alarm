# Reference geometry for the bilge-alarm case: the real populated PCB (placed from the KiCad file with
# KiCad's own STEP models) plus a model of the ESP32-S3-DevKitC-1 clone. None of this is printed; it is
# only there to look at and to run interference checks against.
#
# Everything is in PCB coordinates (mm, +y up the board, board origin bottom-left) like BilgeCase.py;
# KiCad's y axis points down, so y_case = 100 - y_kicad.
import adsk.core, adsk.fusion, math, os, re

CM = 0.1
tbm = adsk.fusion.TemporaryBRepManager.get()
BOARD_T = 1.6
LOG = r'C:\Users\sascha\Documents\Claude CODE\lada-panas\bilge-alarm\case\ref_progress.log'


def log(msg):
    with open(LOG, 'a') as f:
        f.write(msg + '\n')


def P(x, y, z):
    return adsk.core.Point3D.create(x * CM, y * CM, z * CM)


def box(x0, x1, y0, y1, z0, z1):
    obb = adsk.core.OrientedBoundingBox3D.create(
        P((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
        adsk.core.Vector3D.create(1, 0, 0), adsk.core.Vector3D.create(0, 1, 0),
        (x1 - x0) * CM, (y1 - y0) * CM, (z1 - z0) * CM)
    return tbm.createBox(obb)


def cyl(x, y, z0, z1, d):
    return tbm.createCylinderOrCone(P(x, y, z0), d / 2 * CM, P(x, y, z1), d / 2 * CM)


def add(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.UnionBooleanType)


def sub(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.DifferenceBooleanType)


# ---------------------------------------------------------------- tiny s-expression reader
_TOK = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+')


def parse_sexpr(text):
    stack, cur = [], []
    for t in _TOK.findall(text):
        if t == '(':
            stack.append(cur)
            cur = []
        elif t == ')':
            done, cur = cur, stack.pop()
            cur.append(done)
        else:
            cur.append(t[1:-1] if t.startswith('"') else t)
    return cur[0]


def child(node, name):
    for c in node:
        if isinstance(c, list) and c and c[0] == name:
            return c
    return None


def children(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def footprints(pcb_path):
    root = parse_sexpr(open(pcb_path, encoding='utf-8').read())
    out = []
    for fp in children(root, 'footprint'):
        at = child(fp, 'at')
        ref = ''
        for p in children(fp, 'property'):
            if p[1] == 'Reference':
                ref = p[2]
        model = child(fp, 'model')
        layer = child(fp, 'layer')
        out.append({
            'name': fp[1], 'ref': ref, 'x': float(at[1]), 'y': 100.0 - float(at[2]),
            'rot': float(at[3]) if len(at) > 3 else 0.0,
            'layer': layer[1] if layer else 'F.Cu',
            'model': model[1] if model else None})
    return out


# ---------------------------------------------------------------- placement of imported STEP models
def place_matrix(x, y, z, rot_deg):
    m = adsk.core.Matrix3D.create()
    m.setToRotation(math.radians(rot_deg), adsk.core.Vector3D.create(0, 0, 1), adsk.core.Point3D.create(0, 0, 0))
    m.translation = adsk.core.Vector3D.create(x * CM, y * CM, z * CM)
    return m


def import_step(app, comp, path):
    before = comp.occurrences.count
    opts = app.importManager.createSTEPImportOptions(path)
    opts.isViewFit = False
    app.importManager.importToTarget(opts, comp)
    return comp.occurrences.item(before) if comp.occurrences.count > before else None


# ---------------------------------------------------------------- real terminal blocks (Degson DG127S-5.08 drawing)
def terminal_block(cx, npos, z0):
    w = 5.08 * (npos - 1) + 6.08
    b = box(cx - w / 2, cx + w / 2, 0.0, 12.0, z0, z0 + 10.8)
    # sloped wire-entry face: 45 degree chamfer on the board-edge side, top
    a = 5.1
    s = math.sqrt(0.5)
    cutter = tbm.createBox(adsk.core.OrientedBoundingBox3D.create(
        P(cx, 0.0, z0 + 10.8), adsk.core.Vector3D.create(1, 0, 0), adsk.core.Vector3D.create(0, s, s),
        (w + 2) * CM, a * CM, a * CM))
    sub(b, cutter)
    return b


# ---------------------------------------------------------------- ESP32-S3-DevKitC-1 clone (27.94 x 57.15, vendor drawing)
def devkit(dev_z, spacer, detail=True):
    """Board at z = dev_z (underside), USB end flush with the PCB right edge x = 100.
    detail=False leaves out the small placeholder boxes (a photo decal shows those parts instead)."""
    x1 = 100.0
    x0 = x1 - 57.15
    yc = 74.03
    y0, y1 = yc - 13.97, yc + 13.97
    b = box(x0, x1, y0, y1, dev_z, dev_z + BOARD_T)
    top = dev_z + BOARD_T
    # WROOM-1 module (18 x 25.5) at the antenna end, the black antenna area overhangs 6.5 mm of it
    add(b, box(x0, x0 + 25.5, yc - 9.0, yc + 9.0, top, top + 0.8))                 # module PCB
    add(b, box(x0 + 7.0, x0 + 25.5, yc - 9.0, yc + 9.0, top + 0.8, top + 3.1))     # RF shield
    # two USB-C receptacles on the short end, shells overhang the edge slightly
    for dy in (-6.6, 6.6):
        add(b, box(x1 - 7.0, x1 + 0.7, yc + dy - 4.47, yc + dy + 4.47, top, top + 3.3))
    # buttons, LEDs, regulator, bridge chip
    if detail:
        add(b, box(x0 + 29.0, x0 + 32.0, yc + 8.0, yc + 11.0, top, top + 1.6))
        add(b, box(x0 + 29.0, x0 + 32.0, yc - 11.0, yc - 8.0, top, top + 1.6))
        add(b, box(x0 + 36.0, x0 + 40.0, yc - 3.0, yc + 3.0, top, top + 1.8))
        add(b, box(x0 + 42.0, x0 + 45.0, yc + 4.0, yc + 7.0, top, top + 1.8))
    # header strips: black plastic under the board (spacer), pins down into the PCB sockets
    if spacer > 0:
        for y in (yc - 12.7, yc + 12.7):
            add(b, box(x0 + 1.905 - 1.27, x0 + 1.905 + 53.34 + 1.27, y - 1.27, y + 1.27, dev_z - spacer, dev_z))
    return b


def build(app, design, parent_comp, pcb_path, models_dir, dev_z, spacer, pcb_top, report):
    """Create the populated-PCB reference inside parent_comp and return the occurrence group."""
    occ = parent_comp.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    comp = occ.component
    comp.name = 'REAL PCB + ESP32 (reference, not printed)'
    root = design.rootComponent          # STEP files are imported into the root only; instances go into comp

    # bare board: 100 x 100 x 1.6, four 3.2 mm holes
    z_board = pcb_top - BOARD_T
    pcb = box(0, 100, 0, 100, z_board, pcb_top)
    for hx, hy in ((2.6, 2.6), (97.4, 2.6), (2.6, 97.4), (97.4, 97.4)):
        sub(pcb, cyl(hx, hy, z_board - 1, pcb_top + 1, 3.2))
    body = comp.bRepBodies.add(pcb)
    body.name = 'PCB FR4'

    cache = {}
    missing = []
    placed = 0
    holders = []
    for fp in footprints(pcb_path):
        if not fp['model']:
            continue
        if fp['ref'].startswith('BR'):              # unpopulated breakout rows (not in the BOM)
            continue
        if fp['name'].startswith('TerminalBlock'):  # drawn from the real DB127S drawing instead
            continue
        f = fp['model'].replace('${KICAD10_3DMODEL_DIR}', models_dir)
        if f not in cache:
            if not os.path.exists(f):
                missing.append(f)
                cache[f] = None
            else:
                log('import ' + os.path.basename(f))
                holder = import_step(app, root, f)
                log('  imported ok')
                cache[f] = holder.component if holder else None
                if holder:
                    holders.append(holder)         # NOT hidden: hiding the original hides every instance's body too
        src = cache[f]
        if src is None:
            continue
        log('place %s %s at %.2f %.2f rot %.0f' % (fp['ref'], fp['name'], fp['x'], fp['y'], fp['rot']))
        # add at the origin, then set the transform: passing the matrix to addExistingComponent stacks offsets
        o = comp.occurrences.addExistingComponent(src, adsk.core.Matrix3D.create())
        o.transform = place_matrix(fp['x'], fp['y'], pcb_top, fp['rot'])
        placed += 1
    report.append('placed %d KiCad models, %d distinct, missing %s' % (placed, len([k for k in cache if cache[k]]), missing))
    # the imported originals sit at the origin; the placed instances keep the components alive
    for h in holders:
        try:
            h.deleteMe()
        except Exception as e:
            report.append('could not delete original %s: %s' % (h.name, e))

    # parts with no model in the KiCad 9 library (U5's footprint is custom): simple boxes
    for fp in footprints(pcb_path):
        if fp['ref'] == 'U5':                       # MS5637, 3 x 3 x 1 QFN
            m, nm = box(fp['x'] - 1.5, fp['x'] + 1.5, fp['y'] - 1.5, fp['y'] + 1.5, pcb_top, pcb_top + 1.0), 'U5 MS5637 (box)'
        elif fp['ref'] == 'SW3':                    # XKB TS-1187A, 5.1 x 5.1 x 1.5
            m, nm = box(fp['x'] - 2.55, fp['x'] + 2.55, fp['y'] - 2.55, fp['y'] + 2.55, pcb_top, pcb_top + 1.5), 'SW3 MUTE button (box)'
        elif fp['ref'] == 'F1':                     # 1812 PTC, 4.5 x 3.2 x 1.6
            m, nm = box(fp['x'] - 2.25, fp['x'] + 2.25, fp['y'] - 1.6, fp['y'] + 1.6, pcb_top, pcb_top + 1.6), 'F1 PTC (box)'
        else:
            continue
        bb = comp.bRepBodies.add(m)
        bb.name = nm

    # terminal blocks from the Degson drawing (pin row at y = 6, body y 0..12, 10.8 tall)
    for name, cx, n in (('J4', 15.38, 4), ('J1', 43.46, 2), ('J3', 61.38, 4), ('J2', 83.38, 4)):
        t = comp.bRepBodies.add(terminal_block(cx, n, pcb_top))
        t.name = name + ' terminal block'

    d = comp.bRepBodies.add(devkit(dev_z, spacer))
    d.name = 'ESP32-S3-DevKitC-1 clone'

    try:
        lib = app.materialLibraries.itemByName('Fusion Appearance Library')

        def look(body, name):
            ap = design.appearances.itemByName(name) or design.appearances.addByCopy(lib.appearances.itemByName(name), name)
            body.appearance = ap
        for b in comp.bRepBodies:
            if b.name == 'PCB FR4':
                look(b, 'Plastic - Matte (Green)')
            elif b.name.endswith('terminal block'):
                look(b, 'Plastic - Glossy (Green)')
            elif b.name.startswith('ESP32'):
                look(b, 'Plastic - Glossy (Black)')
            else:
                look(b, 'Plastic - Matte (Gray)')
    except Exception as e:
        report.append('appearance step skipped: %s' % e)
    return occ
