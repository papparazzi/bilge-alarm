import adsk.core, adsk.fusion
app = adsk.core.Application.get()
design = adsk.fusion.Design.cast(app.activeProduct)
root = design.rootComponent
OUT = 'C:/Users/sascha/Documents/Claude CODE/lada-panas/bilge-alarm/case/renders/'
before = root.occurrences.count
opts = app.importManager.createSTEPImportOptions(r'C:\Users\sascha\Documents\Claude CODE\lada-panas\bilge-alarm\case\ref\bilge_pcb_kicad_silk.step')
opts.isViewFit = False
app.importManager.importToTarget(opts, root)
new = root.occurrences.item(root.occurrences.count - 1)
print('imported', new.name, 'children', new.component.occurrences.count)
m = adsk.core.Matrix3D.create()
m.translation = adsk.core.Vector3D.create(0, 10.0, 0.6)
new.transform = m
bb = new.boundingBox
print('placed bbox mm', [round(v * 10, 1) for v in (bb.minPoint.x, bb.minPoint.y, bb.minPoint.z, bb.maxPoint.x, bb.maxPoint.y, bb.maxPoint.z)])
for o in root.occurrences:
    if o.name.startswith('bilge_pcb_kicad 1'):
        o.isLightBulbOn = False
        print('hid plain import', o.name)
occs = {o.name.split(':')[0]: o for o in root.occurrences}
for n, o in occs.items():
    if n.startswith('REAL PCB'):
        o.isLightBulbOn = False
occs['Case base'].isLightBulbOn = True
occs['Case cover'].isLightBulbOn = False
occs['Mute plunger'].isLightBulbOn = False
vp = app.activeViewport
def save(name, w, h):
    o = adsk.core.SaveImageFileOptions.create(OUT + name)
    o.width = w; o.height = h; o.isAntiAliased = True; o.isBackgroundTransparent = False
    vp.saveAsImageFileWithOptions(o)
    print('saved', name)
adsk.doEvents()
cam = vp.camera
cam.isSmoothTransition = False
cam.viewOrientation = adsk.core.ViewOrientations.IsoTopRightViewOrientation
vp.camera = cam
vp.fit()
adsk.doEvents()
save('silk_base_pcb_iso.png', 3600, 2600)
cam = vp.camera
cam.cameraType = adsk.core.CameraTypes.OrthographicCameraType
cam.target = adsk.core.Point3D.create(5.0, 5.0, 1.0)
cam.eye = adsk.core.Point3D.create(5.0, 5.0, 80.0)
cam.upVector = adsk.core.Vector3D.create(0, 1, 0)
cam.viewExtents = 11.0
vp.camera = cam
adsk.doEvents()
save('silk_base_pcb_top.png', 3200, 3200)
print('done')
