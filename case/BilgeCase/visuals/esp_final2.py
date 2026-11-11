import sys, importlib, adsk.core, adsk.fusion
sys.path.insert(0, r'C:\Users\sascha\Documents\Claude CODE\lada-panas\bilge-alarm\case\BilgeCase')
import ref_pcb; importlib.reload(ref_pcb)
app = adsk.core.Application.get()
design = adsk.fusion.Design.cast(app.activeProduct)
root = design.rootComponent
occ = [o for o in root.occurrences if o.name.startswith('ESP32 clone')][0]
comp = occ.component
while comp.decals.count > 0:
    comp.decals.item(0).deleteMe()
for n in [b.name for b in comp.bRepBodies]:
    if n.startswith('ESP32') or n.startswith('USB-C'):
        comp.bRepBodies.itemByName(n).deleteMe()
DEV_Z = 7.6 + 8.5 + 2.5
top = DEV_Z + 1.6
nb = comp.bRepBodies.add(ref_pcb.devkit(DEV_Z, 2.5, detail=False)); nb.name = 'ESP32-S3-DevKitC-1 clone'
lib = app.materialLibraries.itemByName('Fusion Appearance Library')
def ap(name):
    return design.appearances.itemByName(name) or design.appearances.addByCopy(lib.appearances.itemByName(name), name)
nb.appearance = ap('Plastic - Glossy (Black)')
grey = ap('Plastic - Glossy (Grey)')
board_top = None
n_grey = 0
for f in nb.faces:
    bb = f.boundingBox
    zmin, zmax = bb.minPoint.z * 10, bb.maxPoint.z * 10
    flat_top = f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and abs(zmin - top) < 0.01 and abs(zmax - top) < 0.01
    if flat_top and (board_top is None or f.area > board_top.area):
        board_top = f
    elif zmin >= top - 0.01 and not flat_top:
        f.appearance = grey
        n_grey += 1
    elif zmin >= top - 0.01 and flat_top:
        pass
print('faces painted grey:', n_grey)
# the flat faces above the board (module, USB tops) are also at z > top; paint them too
for f in nb.faces:
    bb = f.boundingBox
    if f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType and bb.minPoint.z * 10 > top + 0.05 and abs(bb.minPoint.z - bb.maxPoint.z) * 10 < 0.01:
        f.appearance = grey
inp = comp.decals.createInput(r'C:\Users\sascha\Documents\Claude CODE\lada-panas\bilge-alarm\case\ref\esp32_top_flat.png', [board_top], adsk.core.Point3D.create(7.1425, 7.403, top * 0.1))
m = adsk.core.Matrix3D.create()
W = 5.715; H = W * 559.0 / 1143.0
m.setCell(0, 0, W); m.setCell(1, 1, H); m.setCell(2, 2, 1.0)
m.setCell(0, 3, 7.1425); m.setCell(1, 3, 7.403); m.setCell(2, 3, top * 0.1)
inp.transform = m
inp.isChainFaces = False
comp.decals.add(inp)
print('decal on board face; bodies:', [b.name for b in comp.bRepBodies])

OUT = 'C:/Users/sascha/Documents/Claude CODE/lada-panas/bilge-alarm/case/renders/'
occs = {o.name.split(':')[0]: o for o in root.occurrences}
vp = app.activeViewport
def save(name, w, h):
    o = adsk.core.SaveImageFileOptions.create(OUT + name)
    o.width = w; o.height = h; o.isAntiAliased = True; o.isBackgroundTransparent = False
    vp.saveAsImageFileWithOptions(o)
    print('saved', name)
def set_cam(kind, orient):
    adsk.doEvents()
    cam = vp.camera; cam.isSmoothTransition = False
    cam.cameraType = kind
    vp.camera = cam
    adsk.doEvents()
    cam = vp.camera; cam.isSmoothTransition = False
    cam.viewOrientation = orient
    vp.camera = cam
    vp.fit()
    adsk.doEvents()
V = adsk.core.ViewOrientations
CT = adsk.core.CameraTypes
occs['Case base'].isLightBulbOn = True
occs['Case cover'].isLightBulbOn = False
occs['Mute plunger'].isLightBulbOn = False
set_cam(CT.PerspectiveCameraType, V.IsoTopRightViewOrientation); save('final_base_pcb_iso.png', 3600, 2600)
set_cam(CT.OrthographicCameraType, V.TopViewOrientation); save('final_base_pcb_top.png', 3200, 3200)
occs['Case cover'].isLightBulbOn = True
occs['Mute plunger'].isLightBulbOn = True
set_cam(CT.PerspectiveCameraType, V.IsoTopRightViewOrientation); save('final_closed_iso.png', 3000, 2200)
print('done')
