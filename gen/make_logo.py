"""Turns logo/lada_panas_logo.webp into filled silkscreen polygons (gen/logo_polys.json, mm, origin = logo centre, y down).
Strokes are thickened to JLCPCB's minimum silk line width (0.15 mm) and a 1-bit preview is written to logo/logo_silk_preview.png."""
import json, os, sys
import cv2, numpy as np
from PIL import Image
D = float(sys.argv[1]) if len(sys.argv) > 1 else 32.0       # finished diameter, mm
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
im = np.array(Image.open(os.path.join(root, "logo", "lada_panas_logo.webp")).convert("L"))
S = 4                                                       # work at 4x so thickening is sub-pixel accurate
im = cv2.resize(im, None, fx=S, fy=S, interpolation=cv2.INTER_CUBIC)
ink = (im < 150).astype(np.uint8)
ys, xs = np.nonzero(ink)
x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
cx, cy, W = (x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0)
mm_per_px = D / W
# minimum stroke 0.15 mm: thicken thin strokes (opening finds the strokes that are already thick enough)
minpx = 0.15 / mm_per_px
k = int(round(minpx / 2)) | 1
thick = cv2.morphologyEx(ink, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k)))
thin = cv2.bitwise_and(ink, cv2.bitwise_not(thick))
grow = cv2.dilate(thin, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k)))
out = cv2.bitwise_or(ink, grow)
sg = 0.035 / mm_per_px                                      # round off the beads left by the thickening
out = (cv2.GaussianBlur(out.astype(np.float32), (0, 0), sg) > 0.5).astype(np.uint8)
cv2.imwrite(os.path.join(root, "logo", "logo_silk_preview.png"), 255 - out * 255)
cnts, hier = cv2.findContours(out, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
eps = 0.04 / mm_per_px                                      # 0.04 mm simplification
polys = []
for i, c in enumerate(cnts):
    if hier[0][i][3] != -1:
        continue
    def conv(cc):
        a = cv2.approxPolyDP(cc, eps, True)[:, 0, :].astype(float)
        return [[round((p[0] - cx) * mm_per_px, 3), round((p[1] - cy) * mm_per_px, 3)] for p in a]
    outer = conv(c)
    if len(outer) < 3 or cv2.contourArea(c) * mm_per_px ** 2 < 0.004:
        continue
    holes = []
    j = hier[0][i][2]
    while j != -1:
        if cv2.contourArea(cnts[j]) * mm_per_px ** 2 >= 0.004:
            h = conv(cnts[j])
            if len(h) >= 3:
                holes.append(h)
        j = hier[0][j][0]
    polys.append({"outer": outer, "holes": holes})
json.dump({"diameter": D, "polys": polys}, open(os.path.join(root, "gen", "logo_polys.json"), "w"))
print("polygons", len(polys), "holes", sum(len(p["holes"]) for p in polys), "points", sum(len(p["outer"]) + sum(len(h) for h in p["holes"]) for p in polys), "mm/px", round(mm_per_px, 4), "min stroke px", round(minpx, 1))
