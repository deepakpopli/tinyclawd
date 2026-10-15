#!/usr/bin/env python3
"""Checks for stands.scad: the housing sits on the stand without overlap, the
screen tilt is right, it will not tip over, and the USB-C cable path is clear.
Run from this folder:
  python3 check_stands.py            check every variant
  python3 check_stands.py --render   render the STLs first (OpenSCAD)
"""
import sys

import numpy as np
import trimesh

MIN_T, CLEAR = 3, 0.3

# variants to make: (stand, tilt angle, riser mm)
VARIANTS = [("mac_ramp", 30, 0), ("mac_floppy", 30, 0), ("clawd", 30, 0)]
USB_PLUG = 5      # how far a USB-C plug sticks out of the back (measured, max)


def stl_name(name, angle, riser):
    return f"out/stand_{name}_{angle}deg" + (f"_r{riser}" if riser else "") + ".stl"

# stand name: (stand stl, housing stl, back plate stl, plate transform, platform y1, platform zb)
PLATE_MAC = "out/mac_back_plate.stl"
PLATE_ORIG = "original/Back Plate-Body.stl"
# stand: (housing stl, back plate stl, platform rear y1, platform zb), as in stands.scad
STANDS = {
    "mac_ramp": ("out/mac_housing.stl", PLATE_MAC, 48 + CLEAR + 3, -11.5),
    "mac_floppy": ("out/mac_housing.stl", PLATE_MAC, 48 + CLEAR + 3, -11.5),
    "clawd": ("out/clawd_v1_housing.stl", PLATE_ORIG, 41 + CLEAR + 4, -10.5 + 3),
}
# extra parts that ride on a housing: football badge on the Clawd
EXTRAS = {"clawd": ["out/football_badge_on_clawd.stl"]}
PLATE_T = np.array([[1, 0, 0, 44.75], [0, 0, 1, 42.5], [0, 1, 0, 0], [0, 0, 0, 1]], float)


def B(a, b, op):
    return getattr(trimesh.boolean, op)([a, b], engine="manifold")


def on_stand(y1, zb, riser=0, angle=30):
    """Same transform as on_stand() in stands.scad."""
    a = np.radians(-angle)
    rot = trimesh.transformations.rotation_matrix(a, [1, 0, 0])
    desk = y1 * np.sin(a) + zb * np.cos(a) - MIN_T - riser
    T = trimesh.transformations.translation_matrix([0, 0, -desk])
    return T @ rot


def render():
    import subprocess
    for name, angle, riser in VARIANTS:
        subprocess.run(["openscad", "--backend=manifold", "-D", f'stand="{name}"', "-D", f"angle={angle}",
                        "-D", f"riser={riser}", "-o", stl_name(name, angle, riser), "stands.scad"],
                       check=True, capture_output=True)
    for part in ("body", "label"):      # the Clawd stand with the "tiny clawd" lockup
        subprocess.run(["openscad", "--backend=manifold", "-D", 'stand="clawd"', "-D", 'label="tinyclawd"',
                        "-D", f'part="{part}"', "-o", f"out/stand_clawd_30deg_tiny_{part}.stl", "stands.scad"],
                       check=True, capture_output=True)


def main():
    ok = True
    for name, angle, riser in VARIANTS:
        housing_f, plate_f, y1, zb = STANDS[name]
        stand = trimesh.load(stl_name(name, angle, riser))
        M = on_stand(y1, zb, riser, angle)
        housing = trimesh.load(housing_f); housing.apply_transform(M)
        extras = []
        for extra in EXTRAS.get(name, []):
            e = trimesh.load(extra); e.apply_transform(M)
            extras.append(e)
        plate = trimesh.load(plate_f); plate.apply_transform(M @ PLATE_T)
        body = trimesh.util.concatenate([housing, plate])

        overlap = sum(B(stand, m, "intersection").volume for m in [housing, plate] + extras)
        gap = trimesh.proximity.signed_distance(stand, housing.vertices)  # >0 inside the stand
        contact = np.sum(np.abs(gap) < 0.05)
        # screen normal: -y of the housing, tilted
        n = M[:3, :3] @ np.array([0, -1, 0])
        tilt = np.degrees(np.arcsin(n[2]))
        # tipping: centre of mass of housing (solid plastic, ~uniform) over the stand's desk footprint
        com = (housing.center_mass * housing.volume + stand.center_mass * stand.volume) / (housing.volume + stand.volume)
        foot = stand.section(plane_origin=[0, 0, 0.05], plane_normal=[0, 0, 1])
        poly = foot.to_planar()[0].polygons_full[0] if foot is not None else None
        if poly is not None:
            T2 = foot.to_planar()[1]
            com2 = trimesh.transformations.transform_points([com], np.linalg.inv(T2))[0][:2]
            from shapely.geometry import Point
            inside = poly.contains(Point(com2))
            margin = poly.exterior.distance(Point(com2))
        else:
            inside, margin = False, 0
        # USB-C: rays from the port straight out the back
        xs, zs = np.meshgrid(np.linspace(14.5, 31.5, 8), np.linspace(5.5, 11.5, 4))
        o = np.column_stack([xs.ravel(), np.full(xs.size, 49.0), zs.ravel()])
        o = trimesh.transformations.transform_points(o, M)
        d = np.tile(M[:3, :3] @ np.array([0, 1, 0]), (len(o), 1))
        usb_hits = stand.ray.intersects_any(o, d).sum()
        # USB-C plug room: the plug leaves the back pointing down at the tilt angle;
        # longest straight plug (from the back face) before its lower edge meets the desk
        port_low = trimesh.transformations.transform_points([[23, 48, 4.5]], M)[0]
        plug_room = port_low[2] / np.sin(np.radians(angle))

        good = overlap < 0.05 and contact > 0 and abs(tilt - angle) < 0.5 and inside and usb_hits == 0
        ok &= good
        print(f"[{'PASS' if good else 'FAIL'}] {name} {angle} deg +{riser} mm: overlap {overlap:.3f} mm3, resting contact points {contact}, "
              f"screen tilted up {tilt:.1f} deg, centre of mass inside footprint {inside} ({margin:.1f} mm from edge), "
              f"USB rays blocked {usb_hits}/{len(o)}, stand {stand.extents.round(1).tolist()} mm")
        print(f"       USB-C port {port_low[2]:.1f} mm above the desk, room for a straight plug up to {plug_room:.0f} mm long "
              f"(your plug: {USB_PLUG} mm, {plug_room - USB_PLUG:.0f} mm left for the cable to bend)")
    print("\nALL STANDS PASSED" if ok else "\nSOME STANDS FAILED")


if __name__ == "__main__":
    if "--render" in sys.argv:
        render()
    main()
