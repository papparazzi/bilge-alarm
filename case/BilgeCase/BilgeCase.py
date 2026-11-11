# Bilge alarm Rev C.5 - 3D-printable case, built in Autodesk Fusion with the Python API.
# Run from Fusion (Shift+S > Scripts) or through the ClaudeBridge add-in.
#
# Coordinates are PCB coordinates in mm (KiCad/JLCPCB CPL): origin = board bottom-left corner,
# +x right, +y up the board (terminal blocks sit on the y=0 edge), z up from the case floor.
# Fusion's API works in cm, so every length goes through CM.
import adsk.core, adsk.fusion, traceback, os, sys, math, importlib

CM = 0.1
HERE = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else r'C:\Users\sascha\Documents\Claude CODE\lada-panas\bilge-alarm\case\BilgeCase'
OUT = os.path.join(HERE, 'out')
PCB_FILE = os.path.join(HERE, '..', '..', 'kicad', 'bilge_n2k_revC_routed.kicad_pcb')
KICAD_MODELS = r'C:/Users/sascha/AppData/Local/Programs/KiCad/9.0/share/kicad/3dmodels'
WITH_REF = True     # also build the populated PCB + ESP32 reference and run interference checks against it

# ---------------------------------------------------------------- parameters (multiples of the 0.4 mm nozzle)
WALL = 2.0          # outer walls, floor and ceiling: 5 perimeters / 5 layers at 0.4
CLR = 0.4           # gap between PCB edge and inner wall
PCB_T = 1.6
STANDOFF_TOP = 6.0  # PCB underside; room for 3.5 mm of THT pins below
PCB_TOP = STANDOFF_TOP + PCB_T          # 7.6
SPLIT = 15.5        # base wall top / cover rim and the axis height of the cable inlets
DEV_SPACER = 2.5    # "Soldered" ESP32 variant: header plastic (~2.5 mm) stops on the socket. 0.0 if you slide it off
DEV_Z = PCB_TOP + 8.5 + DEV_SPACER      # ESP32 board underside: sits on 8.5 mm sockets
ESP_TOP = DEV_Z + 1.6 + 3.3             # tallest ESP32 parts (USB-C shells; the module shield is 0.2 mm lower)
ESP_CLEAR = 5.0                         # air above the ESP32. The cover never touches it; see case/README.md (thermal)
CEIL_IN = ESP_TOP + ESP_CLEAR           # underside of the cover top (28.5 mm)
TOP = CEIL_IN + WALL                    # 30.5 mm

# interior (PCB occupies x 0..100, y 0..100; cable zone extends below y=0)
IX0, IX1 = -CLR, 100 + CLR
IY0, IY1 = -28.4, 100 + CLR
R_I, R_O = 1.2, 3.2                     # inner / outer vertical corner radius (PCB corners just clear 1.2)
OX0, OX1, OY0, OY1 = IX0 - WALL, IX1 + WALL, IY0 - WALL, IY1 + WALL

# Voron-style M3 heat-set insert: Ruthex M3x5x4 -> OD 5.0, L 4.0, pilot hole 4.4 (4.25 nominal)
INS_D, INS_DEPTH = 4.4, 4.8
BOSS_D = 7.6        # 1.6 mm wall around the insert hole = 4 perimeters
SCREW_CLR_D = 3.4
CBORE_D, CBORE_H = 6.0, 3.2   # M3 SHCS head 5.5 x 3.0, sits in a counterbore in the cover top
TUBE_D = 8.0                  # cover screw tube, 1.0 mm wall around the 6.0 counterbore

# PCB retention
#  - bottom (cable side): BR and BL are screwed down to their posts. BR takes an M3x6. The J4 terminal block is only
#    2.1-2.5 mm from the BL hole, an M3 head (5.5 mm) would hit it, so BL takes an M2x5 (3.8 mm head, 1.9 mm radius).
#  - top: TL and TR carry the closing screws (M3x25 from the cover through the PCB hole into the post's insert).
#  - the cover also has hold-down pads over the component-free strip along the top edge, and a matching support rib
#    under the board there, so the PCB is clamped (HOLD_GAP of play) without the cover touching the ESP32.
PCB_SCREWS = ('BR', 'BL')
# insert pilot hole (d, depth), insert (OD, length), screw (length, shank d, head d, head height)
M3_SPEC = dict(name='M3', hole=INS_D, depth=INS_DEPTH, ins_od=5.0, ins_len=4.0, L=6.0, shank=2.9, head_d=5.5, head_h=3.0)
M2_SPEC = dict(name='M2', hole=3.2, depth=4.0, ins_od=3.5, ins_len=3.0, L=5.0, shank=1.9, head_d=3.8, head_h=2.0)
PCB_SPEC = {'BR': M3_SPEC, 'BL': M2_SPEC}
CLOSE_SCREWS = ('TL', 'TR')                 # closing screws through the top PCB holes
# and two more at the very bottom corners of the cable area (below the PCB), same post height and screw
CABLE_END_POSTS = ((IX0 + 4.0, IY0 + 4.0), (IX1 - 4.0, IY0 + 4.0))
TUBE_D_END = 7.6                            # slimmer tube there: clears the cable saddles by 0.4 mm
HOLD_GAP = 0.1                      # air between the cover pads and the PCB (0 = touching; printed parts vary +-0.2)
HOLD_Y0, HOLD_Y1 = 96.4, 99.2       # pad strip along the top edge (no components, only silkscreen and vias)
HOLD_X = ((10.0, 32.0), (39.0, 61.0), (68.0, 90.0))

# Closing screw length: head seat z = TOP - CBORE_H, tip 3.7 mm inside the 4 mm insert (insert z 2.0..6.0, tip z 2.3)
#   L = (TOP - CBORE_H) - (STANDOFF_TOP - 3.7) = 30.5 - 3.2 - 2.3 = 25 mm (M3x25)
CLOSE_TIP_IN = 3.7
CLOSE_SCREW_L = round((TOP - CBORE_H) - (STANDOFF_TOP - CLOSE_TIP_IN))

# tongue-and-recess joint between base and cover
RECESS_W, RECESS_H = 1.2, 2.0
TONGUE_W = 0.8

# PCB mounting holes (KiCad: 2.6 mm from each corner)
HOLES = {'BL': (2.6, 2.6), 'BR': (97.4, 2.6), 'TL': (2.6, 97.4), 'TR': (97.4, 97.4)}

# cable inlets, one per terminal block (x of block centre, inlet radius). The hole is a full circle on the
# split line: half in the base, half in the cover. The cable lies in a half-round saddle behind it.
INLETS = [
    ('J4 NMEA 2000', 15.38, 5.0, 15.2),
    ('J1 12V in', 43.46, 4.5, 15.0),
    ('J3 outputs', 61.38, 5.0, 16.0),
    ('J2 inputs', 83.38, 5.0, 16.0),
]                                       # name, centre x, inlet radius, saddle width
GROOVE_DR = 0.4                         # groove radius = inlet radius - this
TIE_Y = -20.0                           # tie tunnel ~2 cm in front of the terminal blocks (front face y = 0)
PLAT_Y1 = -14.0
# cable tie through the cradle: two vertical slots, one each side of the cable, opening into the half-round groove,
# joined by a tunnel under the groove. The tie goes down one slot, under, up the other and over the cable,
# so it pulls the cable down into the groove. Sized for ties up to 5 mm wide and 1.5 mm thick.
TIE_W = 5.2                             # slot / tunnel width along the cable (tie width + clearance)
TIE_T = 2.2                             # slot thickness across the cable (tie thickness + clearance)
TIE_H = 3.5                             # corridor height: 1.5 mm tie + room to pull the printed support out
SLOT_IN = 0.6                           # the slot starts this far inside the groove edge, so it opens into the half-round
TIE_FLOOR_Z = 3.0                       # floor of the tie corridor
ROOF_CHAMFER = False                    # False: flat corridor roof (print a support under it and break it out through the back).
                                        # True: 45 degree chamfered roof with a FLAT_ROOF wide top, prints without support
FLAT_ROOF = 4.0

# dev board (clone): 27.94 x 57.15, USB end flush with the PCB right edge, centred on the socket rows
USB_Y0, USB_Y1 = 74.03 - 14.5, 74.03 + 14.5
USB_Z1 = 23.0 + DEV_SPACER

LEDS = [(97.8, 42.5), (97.8, 16.5)]     # D2 status, D3 alarm
SW3 = (96.0, 32.0)

# ---------------------------------------------------------------- geometry helpers
app = adsk.core.Application.get()
tbm = adsk.fusion.TemporaryBRepManager.get()


def P(x, y, z):
    return adsk.core.Point3D.create(x * CM, y * CM, z * CM)


def V(x, y, z):
    return adsk.core.Vector3D.create(x, y, z)


def box(x0, x1, y0, y1, z0, z1):
    obb = adsk.core.OrientedBoundingBox3D.create(
        P((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), V(1, 0, 0), V(0, 1, 0),
        (x1 - x0) * CM, (y1 - y0) * CM, (z1 - z0) * CM)
    return tbm.createBox(obb)


def cyl(x, y, z0, z1, d):
    return tbm.createCylinderOrCone(P(x, y, z0), d / 2 * CM, P(x, y, z1), d / 2 * CM)


def cylY(x, z, y0, y1, d):
    """cylinder lying along y (cable axis)"""
    return tbm.createCylinderOrCone(P(x, y0, z), d / 2 * CM, P(x, y1, z), d / 2 * CM)


def add(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.UnionBooleanType)


def sub(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.DifferenceBooleanType)


def rr(x0, x1, y0, y1, z0, z1, r):
    """rounded-rectangle prism"""
    b = box(x0 + r, x1 - r, y0, y1, z0, z1)
    add(b, box(x0, x1, y0 + r, y1 - r, z0, z1))
    for cx in (x0 + r, x1 - r):
        for cy in (y0 + r, y1 - r):
            add(b, cyl(cx, cy, z0, z1, 2 * r))
    return b


def cbore(c, x, y):
    """Counterbore for an M3 head with a 45 degree cone seat instead of a flat ledge (prints support-free, top side down).
    The head's edge (radius 2.75) touches the cone at the designed seat height TOP - CBORE_H."""
    seat = TOP - CBORE_H
    r_in, r_head, r_out = SCREW_CLR_D / 2, 2.75, CBORE_D / 2
    z_cone0 = seat - (r_head - r_in)
    z_cone1 = z_cone0 + (r_out - r_in)
    sub(c, tbm.createCylinderOrCone(P(x, y, z_cone0), r_in * CM, P(x, y, z_cone1), r_out * CM))
    sub(c, cyl(x, y, z_cone1, TOP + 1, CBORE_D))


def tie_corridor(cx, y0, y1, half_w, z_bot, h_vert, legs):
    """Void for the tie's bottom run: a box with its two top corners cut at 45 degrees, so the roof needs no support."""
    c = box(cx - half_w, cx + half_w, y0, y1, z_bot, z_bot + h_vert + legs)
    if legs > 0:
        s = math.sqrt(0.5)
        a = legs * math.sqrt(2.0)                      # diamond with half-diagonal = legs, centred on the top corner
        for sgn in (-1, 1):
            cut = tbm.createBox(adsk.core.OrientedBoundingBox3D.create(
                P(cx + sgn * half_w, (y0 + y1) / 2, z_bot + h_vert + legs), V(s, 0, s), V(0, 1, 0),
                a * CM, (y1 - y0 + 2) * CM, a * CM))
            sub(c, cut)
    return c


def gusset(side, x_wall, yc, thick, z0, z1, run):
    """right-triangle rib standing on a lug: tall edge against the wall, sloping down to z0 at `run` mm out"""
    xo = x_wall + side * run
    xa, xb = sorted((x_wall - side * 1.0, xo))
    g = box(xa, xb, yc - thick / 2, yc + thick / 2, z0, z1)
    # hypotenuse A(x_wall, z1) -> B(xo, z0); cut everything on the far side of it
    dx, dz = xo - x_wall, z0 - z1
    ln = math.hypot(dx, dz)
    ux, uz = dx / ln, dz / ln
    nx, nz = -uz, ux
    cx_, cz_ = x_wall, z0                      # the corner that stays
    mx, mz = (x_wall + xo) / 2, (z1 + z0) / 2
    if (cx_ - mx) * nx + (cz_ - mz) * nz > 0:  # make n point away from the kept corner
        nx, nz = -nx, -nz
    H = 40.0
    cutter = tbm.createBox(adsk.core.OrientedBoundingBox3D.create(
        P(mx + nx * H / 2, yc, mz + nz * H / 2), V(ux, 0, uz), V(nx, 0, nz),
        (ln + 10) * CM, H * CM, (thick + 2) * CM))
    sub(g, cutter)
    return g


# ---------------------------------------------------------------- base
def build_base():
    b = rr(OX0, OX1, OY0, OY1, 0, SPLIT, R_O)
    sub(b, rr(IX0, IX1, IY0, IY1, WALL, SPLIT + 1, R_I))
    # recess on the inner half of the wall top, receives the cover tongue
    sub(b, rr(IX0 - RECESS_W, IX1 + RECESS_W, IY0 - RECESS_W, IY1 + RECESS_W, SPLIT - RECESS_H, SPLIT + 1, R_I + RECESS_W))

    # half-round cable saddles and the lower half of each inlet circle
    for name, cx, r, sw in INLETS:
        sad = box(cx - sw / 2, cx + sw / 2, IY0, PLAT_Y1, WALL - 0.1, SPLIT)      # starts at the wall's inner face
        sub(sad, cylY(cx, SPLIT, IY0 - 1, PLAT_Y1 + 1, 2 * (r - GROOVE_DR)))                  # groove
        rg = r - GROOVE_DR                                                                      # groove radius
        s0, s1 = rg - SLOT_IN, rg - SLOT_IN + TIE_T                                            # slot x range from the cable axis
        t_bot = TIE_FLOOR_Z                                                                    # corridor floor (fixed, low)
        legs = max(s1 - FLAT_ROOF / 2.0, 0.0) if ROOF_CHAMFER else 0.0                  # roof chamfer (off by default)
        t_top = t_bot + TIE_H + legs                                                           # roof apex
        print('%s: tie corridor z %.1f..%.1f, %.1f mm of plastic under the groove' % (name, t_bot, t_top, SPLIT - rg - t_top))
        if t_top > SPLIT - rg - 0.9:
            print('WARNING %s: less than 0.9 mm of plastic between the tie corridor and the groove' % name)
        sub(b, cylY(cx, SPLIT, OY0 - 1, IY0 + 0.3, 2 * r))                                      # inlet, lower half
        add(b, sad)
        sub(b, cylY(cx, SPLIT, OY0 - 1, IY0 + 0.3, 2 * r))                                      # keep the inlet clear of the saddle root
        # Tie path, open to the BACK: the two slots and the tunnel under the cable run out through the cable-end wall,
        # like the notches on the Raymarine strain-relief plate. A tie already looped round the cable slides in from
        # outside, front-wards, and stops at the closed end of the slots. No threading a tie down a closed hole.
        ty0, ty1 = OY0 - 1, TIE_Y + TIE_W / 2
        sub(b, tie_corridor(cx, ty0, ty1, s1, t_bot, TIE_H, legs))                              # corridor under the cable
        for sgn in (-1, 1):                                                                     # slot each side, up through the rim
            xa, xb = sorted((cx + sgn * s0, cx + sgn * s1))
            sub(b, box(xa, xb, ty0, ty1, t_bot, SPLIT + 1))

    # PCB corner posts (6.0 mm). Inserts in BR (PCB screw) and in TL/TR (closing screws); BL stays solid
    # (terminal block too close for a screw head) and is located by a peg on the cover
    for k, (x, y) in HOLES.items():
        add(b, cyl(x, y, WALL - 0.1, STANDOFF_TOP, BOSS_D))
    for k in PCB_SCREWS:
        x, y = HOLES[k]
        sp = PCB_SPEC[k]
        sub(b, cyl(x, y, STANDOFF_TOP - sp['depth'], STANDOFF_TOP + 1, sp['hole']))
    for k in CLOSE_SCREWS:
        x, y = HOLES[k]
        sub(b, cyl(x, y, STANDOFF_TOP - INS_DEPTH, STANDOFF_TOP + 1, INS_D))
    # posts at the cable-end corners (outside the PCB): same height and insert as the PCB posts
    for x, y in CABLE_END_POSTS:
        add(b, cyl(x, y, WALL - 0.1, STANDOFF_TOP, BOSS_D))
        sub(b, cyl(x, y, STANDOFF_TOP - INS_DEPTH, STANDOFF_TOP + 1, INS_D))

    # support ribs under the PCB: front edge (pin-free band), between the dev-board socket rows, and along the top
    # strip under the cover's hold-down pads (no through-hole pins there, so the rib can touch the underside)
    add(b, box(6.6, 93.4, -CLR, 3.2, WALL - 0.1, STANDOFF_TOP))
    add(b, box(45.0, 98.0, 71.0, 77.0, WALL - 0.1, STANDOFF_TOP))
    add(b, box(8.0, 92.0, HOLD_Y0 - 0.4, HOLD_Y1 + 0.2, WALL - 0.1, STANDOFF_TOP))

    # heavy-duty tie-down lugs: 4 mm plate, round end, 6.4 x 3.2 slot for a cable tie, two ribs up the wall
    for side in (-1, 1):
        x_wall = OX0 if side < 0 else OX1
        for yc in (12.0, 88.0):
            u_c = 8.0                                   # lug end radius 10, centred 8 mm out
            xa, xb = sorted((x_wall - side * 1.5, x_wall + side * u_c))
            lug = box(xa, xb, yc - 10, yc + 10, 0, 4.0)
            add(lug, cyl(x_wall + side * u_c, yc, 0, 4.0, 20.0))
            xs = x_wall + side * u_c
            sub(lug, box(xs - 1.6, xs + 1.6, yc - 3.2, yc + 3.2, -1, 5))
            add(b, lug)
            for dy in (-5.2, 5.2):
                add(b, gusset(side, x_wall, yc + dy, 2.4, 3.9, 14.0, 6.0))
    return b


# ---------------------------------------------------------------- cover
def build_cover():
    c = rr(OX0, OX1, OY0, OY1, SPLIT, TOP, R_O)
    sub(c, rr(IX0, IX1, IY0, IY1, SPLIT - 1, CEIL_IN, R_I))
    # tongue ring that drops into the base recess
    ring = rr(IX0 - TONGUE_W, IX1 + TONGUE_W, IY0 - TONGUE_W, IY1 + TONGUE_W, SPLIT - RECESS_H + 0.2, SPLIT + 0.1, R_I + TONGUE_W)
    sub(ring, rr(IX0, IX1, IY0, IY1, SPLIT - 3, SPLIT + 1, R_I))
    for name, cx, r, sw in INLETS:                  # tongue and wall opened for the upper half of each inlet and the tie slots
        wide = max(r, (r - GROOVE_DR) - SLOT_IN + TIE_T)
        sub(ring, box(cx - wide - 0.5, cx + wide + 0.5, IY0 - 2, IY0 + 1, SPLIT - 3, SPLIT + 1))
    add(c, ring)
    for name, cx, r, sw in INLETS:
        sub(c, cylY(cx, SPLIT, OY0 - 1, IY0 + 0.3, 2 * r))

    # USB-C slot in the right wall: open at the rim, the plug overmold clears the wall
    sub(c, box(IX1 - 1, OX1 + 1, USB_Y0, USB_Y1, SPLIT - 3, USB_Z1))      # also removes the rim tongue under the slot

    # closing screw tubes over the TL/TR PCB holes: down to the board (HOLD_GAP above it), counterbore for the M3 head
    for k in CLOSE_SCREWS:
        x, y = HOLES[k]
        add(c, cyl(x, y, PCB_TOP + HOLD_GAP, CEIL_IN + 0.1, TUBE_D))
        sub(c, cyl(x, y, 0, TOP + 1, SCREW_CLR_D))
        cbore(c, x, y)
    # closing screw tubes at the two cable-end corners, down to the base posts
    for x, y in CABLE_END_POSTS:
        add(c, cyl(x, y, STANDOFF_TOP + 0.1, CEIL_IN + 0.1, TUBE_D_END))
        sub(c, cyl(x, y, STANDOFF_TOP, TOP + 1, SCREW_CLR_D))
        cbore(c, x, y)
    # hold-down pads over the component-free strip along the top edge (they only ever press on bare board)
    for xa, xb in HOLD_X:
        add(c, box(xa, xb, HOLD_Y0, HOLD_Y1, PCB_TOP + HOLD_GAP, CEIL_IN + 0.1))
    # trim the tubes where they run into the base wall (below the tongue, outside the cavity)
    trim = box(OX0 - 1, OX1 + 1, OY0 - 1, OY1 + 1, STANDOFF_TOP - 1, SPLIT - RECESS_H + 0.1)
    sub(trim, rr(IX0, IX1, IY0, IY1, STANDOFF_TOP - 2, SPLIT + 1, R_I))
    sub(c, trim)

    # (the PCB is screwed to the base at BL and BR, see build_base; TL/TR carry the closing screws)

    # LED light-pipe chimneys (2.2 mm bore takes 1.75 mm clear filament) and MUTE plunger guide
    for x, y in LEDS:
        add(c, cyl(x, y, 11.0, CEIL_IN + 0.1, 5.2))
    add(c, cyl(SW3[0], SW3[1], 12.0, CEIL_IN + 0.1, 7.2))
    for x, y in LEDS:
        sub(c, cyl(x, y, 10.0, TOP + 1, 2.2))
    sub(c, cyl(SW3[0], SW3[1], 10.0, TOP + 1, 4.0))
    return c


# ---------------------------------------------------------------- MUTE plunger (separate small print)
def build_plunger():
    p = cyl(SW3[0], SW3[1], 10.0, TOP + 1.0, 3.6)
    # retaining flange as a 45 degree cone (r 1.8 -> 3.0 over 1.2 mm) so it prints standing up without support;
    # its flat top at z = 12.0 stops against the bottom of the guide tube
    add(p, tbm.createCylinderOrCone(P(SW3[0], SW3[1], 10.8), 1.8 * CM, P(SW3[0], SW3[1], 12.0), 3.0 * CM))
    return p


# ---------------------------------------------------------------- features that need Fusion's feature tools
def fillet_top(comp, body, r):
    edges = adsk.core.ObjectCollection.create()
    for f in body.faces:
        bb = f.boundingBox
        if f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType \
                and abs(bb.minPoint.z * 10 - TOP) < 0.01 and abs(bb.maxPoint.z * 10 - TOP) < 0.01:
            for lp in f.loops:
                if lp.isOuter:
                    for e in lp.edges:
                        edges.add(e)
    if edges.count == 0:
        return 'fillet: no top edges found'
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(edges, adsk.core.ValueInput.createByReal(r * CM), True)
    comp.features.filletFeatures.add(fi)
    return 'fillet r=%.1f on %d edges' % (r, edges.count)


LABELS = [   # text, centre x, centre y, box width (wide enough that nothing wraps), text height
    ('NMEA 2000', 19.5, -22.0, 25, 3.2),
    ('12V IN', 43.5, -22.0, 14, 3.2),
    ('OUTPUTS', 62.5, -22.0, 17, 3.2),
    ('INPUTS', 85.0, -22.0, 14, 3.2),
    ('M/Y LADA PANAS', 38.0, 84.0, 78, 6.4),
    ('BILGE MONITOR  REV C.5', 38.0, 74.0, 66, 3.6),
    ('STATUS', 84.0, 42.5, 18, 3.4),
    ('MUTE', 83.0, 32.0, 14, 3.4),
    ('ALARM', 84.0, 16.5, 18, 3.4),
]


def paint(design, body, name):
    try:
        lib = app.materialLibraries.itemByName('Fusion Appearance Library')
        ap = design.appearances.itemByName(name) or design.appearances.addByCopy(lib.appearances.itemByName(name), name)
        body.appearance = ap
    except Exception as e:
        print('paint failed', name, e)


def engrave(comp, body, depth):
    planes = comp.constructionPlanes
    pin = planes.createInput()
    pin.setByOffset(comp.xYConstructionPlane, adsk.core.ValueInput.createByReal(TOP * CM))
    plane = planes.add(pin)
    sk = comp.sketches.add(plane)
    sk.name = 'engraving'
    for text, cx, cy, w, h in LABELS:
        ti = sk.sketchTexts.createInput2(text, h * CM)
        ti.setAsMultiLine(P(cx - w / 2, cy - h, 0), P(cx + w / 2, cy + h, 0),
                          adsk.core.HorizontalAlignments.CenterHorizontalAlignment,
                          adsk.core.VerticalAlignments.MiddleVerticalAlignment, 0)
        try:
            ti.fontName = 'Arial'
            ti.textStyle = adsk.fusion.TextStyles.TextStyleBold
        except Exception:
            pass
        sk.sketchTexts.add(ti)
    for i in range(sk.sketchTexts.count - 1, -1, -1):      # text -> outline curves so Fusion can make profiles
        sk.sketchTexts.item(i).explode()
    profs = adsk.core.ObjectCollection.create()
    for p in sk.profiles:
        profs.add(p)
    if profs.count == 0:
        return 'engrave: no text profiles'
    ex = comp.features.extrudeFeatures
    inp = ex.createInput(profs, adsk.fusion.FeatureOperations.CutFeatureOperation)
    inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(adsk.core.ValueInput.createByReal(depth * CM)),
                         adsk.fusion.ExtentDirections.NegativeExtentDirection)
    inp.participantBodies = [body]
    ex.add(inp)
    return 'engraved %d text profiles, %.1f mm deep' % (profs.count, depth)


def hardware_parts():
    """Heat-set inserts and screws at every screw point (reference only, not printed)."""
    close_spec = dict(M3_SPEC, L=CLOSE_SCREW_L)
    # name, x, y, insert top z, screw head seat z, spec
    pts = [('PCB %s' % k, HOLES[k][0], HOLES[k][1], STANDOFF_TOP, PCB_TOP, PCB_SPEC[k]) for k in PCB_SCREWS]
    for k in CLOSE_SCREWS:
        pts.append(('closing %s' % k, HOLES[k][0], HOLES[k][1], STANDOFF_TOP, TOP - CBORE_H, close_spec))
    for i, (x, y) in enumerate(CABLE_END_POSTS):
        pts.append(('closing cable end %d' % (i + 1), x, y, STANDOFF_TOP, TOP - CBORE_H, close_spec))
    parts = []
    for name, x, y, z_ins, z_seat, sp in pts:
        ins = cyl(x, y, z_ins - sp['ins_len'], z_ins, sp['ins_od'])        # heat-set insert
        sub(ins, cyl(x, y, z_ins - sp['ins_len'] - 1.0, z_ins + 1.0, sp['shank'] + 0.1))
        parts.append((name + ' insert', 'insert', ins))
        scr = cyl(x, y, z_seat - sp['L'], z_seat, sp['shank'])             # SHCS: shank + head
        add(scr, cyl(x, y, z_seat, z_seat + sp['head_h'], sp['head_d']))
        parts.append(('%s %sx%d' % (name, sp['name'], sp['L']), 'screw', scr))
        print('%s: %sx%d, head seat z=%.1f, tip z=%.1f, insert z %.1f..%.1f -> %.1f mm engaged' % (
            name, sp['name'], sp['L'], z_seat, z_seat - sp['L'], z_ins - sp['ins_len'], z_ins, z_ins - (z_seat - sp['L'])))
    return parts


def find_look(*keys):
    lib = app.materialLibraries.itemByName('Fusion Appearance Library')
    for k in keys:
        for a in lib.appearances:
            if k in a.name:
                return a.name
    return 'Plastic - Glossy (Grey)'


def run():
    doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name = 'Bilge alarm case'
    design = adsk.fusion.Design.cast(doc.products.itemByProductType('DesignProductType'))
    design.designType = adsk.fusion.DesignTypes.DirectDesignType
    root = design.rootComponent
    made, comps = {}, {}
    for name, fn in (('Case base', build_base), ('Case cover', build_cover), ('Mute plunger', build_plunger)):
        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        comp = occ.component
        comp.name = name
        body = comp.bRepBodies.add(fn())
        body.name = name
        made[name], comps[name] = body, (occ, comp)
        print('built', name)
    for step in (lambda: fillet_top(comps['Case cover'][1], made['Case cover'], 1.6),
                 lambda: engrave(comps['Case cover'][1], comps['Case cover'][1].bRepBodies.itemByName('Case cover'), 0.8)):
        try:
            print(step())
        except Exception:
            print('feature step failed:\n' + traceback.format_exc())

    ref = None
    if WITH_REF:
        sys.path.insert(0, HERE)
        import ref_pcb
        importlib.reload(ref_pcb)
        rep = []
        ref = ref_pcb.build(app, design, root, os.path.normpath(PCB_FILE), KICAD_MODELS, DEV_Z, DEV_SPACER, PCB_TOP, rep)
        print('\n'.join(rep))

    for name, look in (('Case base', 'Plastic - Matte (Black)'), ('Case cover', 'Plastic - Matte (Gray)'), ('Mute plunger', 'Plastic - Matte (Red)')):
        paint(design, comps[name][1].bRepBodies.itemByName(name), look)

    hw_occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    hw_occ.component.name = 'Hardware (inserts + M3x6 screws, not printed)'
    screws = []
    brass, steel = find_look('Brass', 'Copper - Polished'), find_look('Steel', 'Stainless')
    for pname, kind, shape in hardware_parts():
        pb = hw_occ.component.bRepBodies.add(shape)
        pb.name = pname
        paint(design, pb, brass if kind == 'insert' else steel)
        if kind == 'screw':
            screws.append(pb)
    print('hardware: %d inserts + %d screws (%s / %s)' % (len(screws), len(screws), brass, steel))
    print('air above the ESP32: cover ceiling z=%.1f, ESP32 top z=%.1f -> %.1f mm (module shield: %.1f mm); '
          'hold-down pads %.1f mm above the PCB' % (CEIL_IN, ESP_TOP, CEIL_IN - ESP_TOP, CEIL_IN - (ESP_TOP - 0.2), HOLD_GAP))

    os.makedirs(OUT, exist_ok=True)
    em = design.exportManager
    for name in ('Case base', 'Case cover', 'Mute plunger'):
        fn = name.lower().replace(' ', '_')
        body = comps[name][1].bRepBodies.itemByName(name)
        o = em.createSTLExportOptions(body, os.path.join(OUT, fn + '.stl'))
        o.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        em.execute(o)
        em.execute(em.createSTEPExportOptions(os.path.join(OUT, fn + '.step'), comps[name][1]))
    print('exported STL + STEP to', OUT)

    ents = adsk.core.ObjectCollection.create()
    for name in ('Case base', 'Case cover', 'Mute plunger'):
        ents.add(comps[name][1].bRepBodies.itemByName(name))
    for sb in screws:                       # screws must clear everything they pass through; inserts are press-fits, left out
        ents.add(sb)
    if ref is not None:
        ents.add(ref)
    res = design.analyzeInterference(design.createInterferenceInput(ents))
    print('interferences:', res.count)
    for i in range(res.count):
        it = res.item(i)
        b = it.interferenceBody.boundingBox
        print('  %s <-> %s  vol %.2f mm3  at x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f' % (
            it.entityOne.name, it.entityTwo.name, it.interferenceBody.volume * 1000,
            b.minPoint.x * 10, b.maxPoint.x * 10, b.minPoint.y * 10, b.maxPoint.y * 10, b.minPoint.z * 10, b.maxPoint.z * 10))
    for name in ('Case base', 'Case cover', 'Mute plunger'):
        b = comps[name][1].bRepBodies.itemByName(name)
        bb = b.boundingBox
        print(name, 'bbox mm: %.1f x %.1f x %.1f' % ((bb.maxPoint.x - bb.minPoint.x) * 10, (bb.maxPoint.y - bb.minPoint.y) * 10, (bb.maxPoint.z - bb.minPoint.z) * 10),
              'vol %.1f cm3' % b.volume)


try:
    run()
except Exception:
    print('FAILED\n' + traceback.format_exc())
    raise
