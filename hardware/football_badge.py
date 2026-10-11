#!/usr/bin/env python3
"""Football badge for the Clawd housing (like the flat Clawd + football print).

A 22 mm disc with a real football pattern: a truncated icosahedron (12
pentagons, 20 hexagons) seen face-on over a pentagon, projected to a disc.
Black base with the pentagons and seams, white hexagons raised 0.8 mm on top,
so it prints in two colours either with an AMS or with one filament swap.

Outputs (run from this folder with python3 football_badge.py):
  out/football_badge.3mf              both parts, black and white assigned (Bambu Studio)
  out/football_badge_black.stl        base, print first
  out/football_badge_white.stl        hexagons, from 2.4 mm up
  out/football_badge_on_clawd.stl     merged, placed on the Clawd (for previews and checks)

Two 2 mm holes in the back take short pieces of 1.75 mm filament as pins, matching
the pin holes in clawd_v1_housing.scad, so it lines up when glued.
"""
import zipfile
from itertools import combinations
from pathlib import Path

import numpy as np
import trimesh
from shapely.geometry import Point, Polygon
from shapely.ops import unary_union

R = 11.0            # disc radius
BASE = 2.4          # black base thickness
PANEL = 0.8         # white panels on top
GAP = 0.4           # black seam between panels (about the narrowest a 0.4 nozzle keeps)
RIM = 0.35          # black rim around the edge
ZOOM = 1.15         # pattern size: >1 makes the panels bigger, cut at the rim
CENTRE = (50.5, 4.0)                       # on the Clawd face (x, z), see clawd_v1_housing.scad
PINS = [(48, 7), (46.6, -3.5)]             # same as badge_pins there
PIN_D, PIN_DEPTH = 2.0, 2.0
FACE_Y = -1.5                              # Clawd face plane

OUT = Path(__file__).parent / "out"


def truncated_icosahedron():
    """Vertices on the unit sphere, and faces as (kind, vertex index list)."""
    phi = (1 + 5 ** 0.5) / 2
    ico = []
    for a in (-1, 1):
        for b in (-phi, phi):
            ico += [(0, a, b), (a, b, 0), (b, 0, a)]
    ico = np.array(ico, float)
    edge = min(np.linalg.norm(p - q) for p, q in combinations(ico, 2))
    edges = [(i, j) for i, j in combinations(range(12), 2)
             if abs(np.linalg.norm(ico[i] - ico[j]) - edge) < 1e-6]
    verts, index = [], {}
    for i, j in edges:                       # cut every edge in thirds
        for a, b in ((i, j), (j, i)):
            index[(a, b)] = len(verts)
            verts.append(ico[a] + (ico[b] - ico[a]) / 3)
    verts = np.array(verts)
    faces = []
    for v in range(12):                      # a pentagon around each icosahedron vertex
        ring = [index[(v, n)] for n in range(12) if (v, n) in index]
        faces.append(("pent", order(verts, ring)))
    for a, b, c in combinations(range(12), 3):   # a hexagon on each icosahedron face
        if (a, b) in index and (b, c) in index and (a, c) in index:
            ring = [index[k] for k in ((a, b), (b, a), (b, c), (c, b), (c, a), (a, c))]
            faces.append(("hex", order(verts, ring)))
    verts /= np.linalg.norm(verts, axis=1)[:, None]
    return verts, faces, ico / np.linalg.norm(ico, axis=1)[:, None]


def order(verts, ring):
    """Sort a face's vertices around its centre."""
    c = verts[ring].mean(axis=0)
    n = c / np.linalg.norm(c)
    u = np.cross(n, [0.3, 0.5, 0.8]); u /= np.linalg.norm(u)
    v = np.cross(n, u)
    ang = [np.arctan2(np.dot(verts[k] - c, v), np.dot(verts[k] - c, u)) for k in ring]
    return [k for _, k in sorted(zip(ang, ring))]


def rotation_to_z(d):
    """Rotation matrix that turns direction d to +z."""
    d = d / np.linalg.norm(d)
    axis = np.cross(d, [0, 0, 1])
    if np.linalg.norm(axis) < 1e-9:
        return np.eye(3)
    axis /= np.linalg.norm(axis)
    ang = np.arccos(np.clip(d[2], -1, 1))
    return trimesh.transformations.rotation_matrix(ang, axis)[:3, :3]


def slerp(p, q, n=8):
    om = np.arccos(np.clip(np.dot(p, q), -1, 1))
    if om < 1e-9:
        return [p]
    return [(np.sin((1 - t) * om) * p + np.sin(t * om) * q) / np.sin(om) for t in np.linspace(0, 1, n)[:-1]]


def panels():
    verts, faces, ico = truncated_icosahedron()
    rot = rotation_to_z(ico[0])               # a pentagon faces the viewer
    verts = verts @ rot.T
    hexes = []
    for kind, ring in faces:
        if kind != "hex":
            continue
        normal = verts[ring].mean(axis=0)
        if normal[2] < 0.25:                  # skip faces turned away (slivers at the rim)
            continue
        pts = []
        for a, b in zip(ring, ring[1:] + ring[:1]):   # curved edges, then flatten to the disc
            pts += slerp(verts[a], verts[b])
        poly = Polygon([(p[0] * R * ZOOM, p[1] * R * ZOOM) for p in pts]).buffer(-GAP / 2)
        hexes.append(poly)
    inside = Point(0, 0).buffer(R - RIM, 128)
    return unary_union([h.intersection(inside) for h in hexes if not h.is_empty])


def extrude(poly, z0, h):
    parts = [trimesh.creation.extrude_polygon(g, h) for g in getattr(poly, "geoms", [poly]) if g.area > 0.05]
    m = trimesh.util.concatenate(parts)
    m.apply_translation([0, 0, z0])
    return m


def build():
    disc = Point(0, 0).buffer(R, 128)
    holes = unary_union([Point(px - CENTRE[0], pz - CENTRE[1]).buffer(PIN_D / 2, 32) for px, pz in PINS])
    black = extrude(disc, 0, BASE)
    hole_m = extrude(holes, -1, PIN_DEPTH + 1)
    black = trimesh.boolean.difference([black, hole_m], engine="manifold")
    white = extrude(panels(), BASE, PANEL)
    return black, white


def write_3mf(path, parts):
    """3MF with one object made of coloured parts. Bambu Studio reads the
    per-part filament from Metadata/model_settings.config; other slicers get
    the colours from the core 3MF base materials."""
    objs, comps, settings = [], [], []
    for i, (name, mesh, colour, extruder) in enumerate(parts, start=1):
        v = "".join(f'<vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x, y, z in mesh.vertices)
        t = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in mesh.faces)
        objs.append(f'<object id="{i}" type="model" name="{name}" pid="100" pindex="{i - 1}">'
                    f'<mesh><vertices>{v}</vertices><triangles>{t}</triangles></mesh></object>')
        comps.append(f'<component objectid="{i}"/>')
        settings.append(f'<part id="{i}" subtype="normal_part"><metadata key="name" value="{name}"/>'
                        f'<metadata key="extruder" value="{extruder}"/></part>')
    top = len(parts) + 1
    mats = "".join(f'<base name="{n}" displaycolor="{c}"/>' for n, _, c, _ in parts)
    model = ('<?xml version="1.0" encoding="UTF-8"?>'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             f'<resources><basematerials id="100">{mats}</basematerials>{"".join(objs)}'
             f'<object id="{top}" type="model" name="football badge"><components>{"".join(comps)}</components></object>'
             f'</resources><build><item objectid="{top}"/></build></model>')
    config = ('<?xml version="1.0" encoding="UTF-8"?><config>'
              f'<object id="{top}"><metadata key="name" value="football badge"/>'
              f'<metadata key="extruder" value="1"/>{"".join(settings)}</object></config>')
    types = ('<?xml version="1.0" encoding="UTF-8"?>'
             '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
             '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
             '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
             '<Default Extension="config" ContentType="text/xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
        z.writestr("Metadata/model_settings.config", config)


def main():
    OUT.mkdir(exist_ok=True)
    black, white = build()
    for name, m in [("black", black), ("white", white)]:
        assert m.is_watertight, name
        m.export(OUT / f"football_badge_{name}.stl")
    write_3mf(OUT / "football_badge.3mf", [("black base", black, "#1A1A1A", 1), ("white panels", white, "#F2F2F2", 2)])
    # placed on the Clawd face: print +z -> housing -y, print x -> x, print y -> z
    placed = trimesh.util.concatenate([black, white])
    T = np.array([[1, 0, 0, CENTRE[0]], [0, 0, -1, FACE_Y], [0, 1, 0, CENTRE[1]], [0, 0, 0, 1]], float)
    placed.apply_transform(T)
    placed.export(OUT / "football_badge_on_clawd.stl")
    print(f"black {black.volume:.0f} mm3, white {white.volume:.0f} mm3, "
          f"{len([g for g in getattr(panels(), 'geoms', [panels()])])} white panels")


if __name__ == "__main__":
    main()
