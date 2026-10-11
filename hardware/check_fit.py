#!/usr/bin/env python3
"""Fit and print checks for the Mac housing against the original Mochi parts.
Run from this folder after rendering:
  python3 check_fit.py                      (Mac housing + Mac back plate)
  python3 check_fit.py out/clawd_housing.stl "original/Back Plate-Body.stl"
"""
import sys
import numpy as np
import trimesh

def B(a, b, op):
    return getattr(trimesh.boolean, op)([a, b], engine="manifold")

def box(x0, x1, y0, y1, z0, z1):
    b = trimesh.creation.box(extents=[x1 - x0, y1 - y0, z1 - z0])
    b.apply_translation([(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2])
    return b

# plate STL coords -> housing coords
PLATE_T = np.array([[1, 0, 0, 44.75], [0, 0, 1, 42.5], [0, 1, 0, 0], [0, 0, 0, 1]], float)

housing_file = sys.argv[1] if len(sys.argv) > 1 else "out/mac_housing.stl"
plate_file = sys.argv[2] if len(sys.argv) > 2 else "out/mac_back_plate.stl"
print(f"housing: {housing_file}\nplate:   {plate_file}\n")
new = trimesh.load(housing_file)
plate = trimesh.load(plate_file)
orig = trimesh.load("original/Housing-Body.stl"); orig.apply_translation([46, 0, 0])
oplate = trimesh.load("original/Back Plate-Body.stl")
ok = True

def report(name, good, detail):
    global ok
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] {name}: {detail}")

# 1. clean solids
for n, m in [("housing", new), ("back plate", plate)]:
    parts = len(m.split(only_watertight=False))
    report(f"{n} is one watertight solid", m.is_watertight and parts == 1,
           f"parts={parts} watertight={m.is_watertight} size={m.extents.round(1).tolist()} mm")

# 2. inside of the housing is identical to the original: no new material inside
# the original's outline (cavity, grooves, dimples, window, touch pocket, side
# hole pockets). Its outer edge chamfers are outside the outline and get filled.
hull = orig.convex_hull.copy(); c = np.array([23, 24, 20.5])
hull.apply_translation(-c); hull.apply_scale(0.995); hull.apply_translation(c)
added_mesh = B(B(new, hull, "intersection"), orig, "difference")
# crush ribs (mochi_fit.scad) are allowed: side walls up to 0.25 mm, floor up to 0.45 mm
rib_zones = trimesh.util.concatenate([box(2.7, 3.1, 4.9, 39.1, 0, 41), box(42.9, 43.3, 4.9, 39.1, 0, 41),
                                      box(2.7, 43.3, 4.9, 39.1, 1.7, 2.3)])
ribs = B(added_mesh, rib_zones, "intersection").volume if added_mesh.volume > 0 else 0
added = added_mesh.volume - ribs
report("inside identical to the original (cavity, grooves, dimples, window, touch pocket)",
       added < 0.5, f"new material inside the original outline {added:.3f} mm3 apart from the crush ribs "
       f"({ribs:.1f} mm3 of ribs; under 0.5 = mesh facet slivers)")
report("no original material removed", B(orig, new, "difference").volume < 0.01,
       f"{B(orig, new, 'difference').volume:.4f} mm3")

# 3. back plate: original part unchanged (USB-C cutout, logo, click arms)
top_part = B(plate, box(-50, 5, 0, 40.01, -1, 7), "intersection")
d = B(top_part, oplate, "symmetric_difference" if hasattr(trimesh.boolean, "symmetric_difference") else "difference").volume \
    if False else (B(top_part, oplate, "difference").volume + B(oplate, top_part, "difference").volume)
report("back plate keeps the original USB-C cutout, logo and click arms", d < 0.01, f"difference {d:.4f} mm3")

# 4. plate slides all the way in and out
p = plate.copy(); p.apply_transform(PLATE_T)
report("closed plate overlaps housing", B(p, new, "intersection").volume < 0.01,
       f"{B(p, new, 'intersection').volume:.3f} mm3; plate bottom z={p.bounds[0][2]:.2f}, housing bottom z={new.bounds[0][2]:.2f}")
op = oplate.copy(); op.apply_transform(PLATE_T @ np.diag([1, 1, 1, 1]))
op.apply_translation([-46 + 46, 0, 0])
travel = p.bounds[1][2] - new.bounds[0][2]
worst_new, worst_orig = 0, 0
for dz in np.arange(0, travel + 1, 0.5):
    q = p.copy(); q.apply_translation([0, 0, -dz])
    worst_new = max(worst_new, B(q, new, "intersection").volume)
for dz in np.arange(0, 41, 0.5):
    q = op.copy(); q.apply_translation([0, 0, -dz])
    worst_orig = max(worst_orig, B(q, orig, "intersection").volume)
report("plate slide path is clear (only the click bumps touch, same as original)",
       worst_new <= worst_orig + 0.01,
       f"worst contact {worst_new:.2f} mm3 over {travel:.1f} mm of travel; original design {worst_orig:.2f} mm3")

# 5. screen window: straight rays from the OLED out the front are not blocked
xs, zs = np.meshgrid(np.linspace(7.3, 38.7, 30), np.linspace(15.05, 30.45, 15))
origins = np.column_stack([xs.ravel(), np.full(xs.size, 2.5), zs.ravel()])
hits = new.ray.intersects_any(origins, np.tile([0, -1, 0], (len(origins), 1)))
report("OLED window clear to the front", not hits.any(), f"{hits.sum()} of {len(origins)} rays blocked")

# 6. USB-C: straight rays from the port area out the back are not blocked
pu = plate.copy(); pu.apply_transform(PLATE_T)
xs, zs = np.meshgrid(np.linspace(-31 + 44.75 + 0.5, -12.5 + 44.75 - 0.5, 15), np.linspace(5, 12, 8))
origins = np.column_stack([xs.ravel(), np.full(xs.size, 40.0), zs.ravel()])
both = trimesh.util.concatenate([new, pu])
hits = both.ray.intersects_any(origins, np.tile([0, 1, 0], (len(origins), 1)))
op_u = oplate.copy(); op_u.apply_transform(PLATE_T)
base = trimesh.util.concatenate([orig, op_u]).ray.intersects_any(origins, np.tile([0, 1, 0], (len(origins), 1)))
report("USB-C opening clear to the back, same as original", (hits == base).all(),
       f"{(~hits).sum()} of {len(origins)} rays pass (original design: {(~base).sum()}); rest hit the rounded slot ends")

# 7. overhangs for the suggested print orientation (front face down)
def overhang(m, up, limit=50):
    n = m.face_normals @ np.array(up, float)
    h = m.triangles_center @ np.array(up, float)
    down = (n < -np.sin(np.radians(limit))) & (h > h.min() + 0.3)
    return m.area_faces[down].sum()
for name, up in [("front face down", [0, 1, 0]), ("standing upright", [0, 0, 1])]:
    print(f"       housing overhang area steeper than 50 deg, {name}: {overhang(new, up):.0f} mm2")
print(f"       back plate overhang area, flat as designed: {overhang(plate, [0, 0, 1]):.0f} mm2")

print("\nALL CHECKS PASSED" if ok else "\nSOME CHECKS FAILED")
