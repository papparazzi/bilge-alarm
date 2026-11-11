"""Turn the STL files exported by BilgeCase.py (model orientation) into print-ready files.

  base          printed floor down      -> unchanged (z = 0 is the floor)
  cover         printed top face down   -> rotated 180 degrees about X, so the flat top sits on the bed
  mute plunger  printed standing up     -> shifted so its lowest point is z = 0

Usage (any Python 3, no packages needed):
    python orient_for_print.py <folder with the exported *.stl> <output folder>
Binary STL in, binary STL out, millimetres. Normals are recomputed from the triangles.
"""
import math
import os
import struct
import sys


def read_stl(path):
    d = open(path, 'rb').read()
    n = struct.unpack('<I', d[80:84])[0]
    tris = []
    for i in range(n):
        v = struct.unpack('<12f', d[84 + i * 50:84 + i * 50 + 48])
        tris.append([v[3:6], v[6:9], v[9:12]])
    return tris


def write_stl(path, tris, header):
    with open(path, 'wb') as f:
        f.write(header[:80].ljust(80, b' '))
        f.write(struct.pack('<I', len(tris)))
        for a, b, c in tris:
            ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
            vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
            nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
            ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            f.write(struct.pack('<12fH', nx / ln, ny / ln, nz / ln, *a, *b, *c, 0))


def transform(tris, flip_x_180):
    out = []
    for t in tris:
        # rotation by 180 degrees about X: (x, y, z) -> (x, -y, -z); a proper rotation, so winding is kept
        out.append([(p[0], -p[1], -p[2]) if flip_x_180 else tuple(p) for p in t])
    zmin = min(p[2] for t in out for p in t)
    return [[(p[0], p[1], p[2] - zmin) for p in t] for t in out]


def main(src, dst):
    os.makedirs(dst, exist_ok=True)
    jobs = (('case_base.stl', False), ('case_cover.stl', True), ('mute_plunger.stl', False))
    for name, flip in jobs:
        tris = transform(read_stl(os.path.join(src, name)), flip)
        write_stl(os.path.join(dst, name), tris, b'Bilge alarm case, print orientation')
        zs = [p[2] for t in tris for p in t]
        xs = [p[0] for t in tris for p in t]
        ys = [p[1] for t in tris for p in t]
        print('%-18s %5d triangles, %.1f x %.1f x %.1f mm%s' % (
            name, len(tris), max(xs) - min(xs), max(ys) - min(ys), max(zs), '  (flipped, top face on the bed)' if flip else ''))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
