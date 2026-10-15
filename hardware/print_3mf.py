#!/usr/bin/env python3
"""Fill the print folder with only the final files, one folder per design.
Every part is turned to its print orientation; 3MFs carry the filament per part
(Bambu Studio reads Metadata/model_settings.config). Fits a 180 x 180 bed.

  print/clawd/  clawd_print_set.3mf (body + back plate 1 red, stand + ball base 2 black,
                ball panels 3 white), the same parts as STLs, football badge, test rings
  print/mac/    mac_print_set.3mf (body + back plate 1 white, ramp stand 2 black),
                the same parts as STLs, floppy stand

Run from this folder after rendering: python3 print_3mf.py
"""
import shutil
import zipfile
from pathlib import Path

import numpy as np
import trimesh

OUT = Path(__file__).parent / "out"
PRINT = Path(__file__).parent / "print"
# Bambu A1 + AMS lite slots:
# 1 yellow, 2 white, 3 black, 4 red. Parts go straight to the right slot.
YELLOW, WHITE, BLACK, RED = 1, 2, 3, 4
COLOURS = {YELLOW: "#F7D959", WHITE: "#FFFFFF", BLACK: "#000000", RED: "#C12E1E"}


CLAWD_README = """CLAWD - what to print (no supports for anything)

PICK ONE:
  clawd_print_set.3mf               football as a separate badge you glue on
  clawd_print_set_ball_in_body.3mf  football built into the body, flush, one piece. Only 2 objects:
                                    the body (red, ball black + white) and the back plate (red).
                                    Needs AMS / multi-colour: the colours share the first layers.
                                    Stand not included: print 3_clawd_stand_30deg.stl on its own.

AMS slots in the 3mf files (Bambu A1 + AMS lite): 4 = red (body, back plate), 3 = black (football,
stand), 2 = white (hexagons). Slot 1 (yellow) is not used.

Stand (black is best):
  3_clawd_stand_30deg_tiny_clawd.3mf  black stand + red pixel Clawd and "tiny clawd" (Chakra Petch Bold).
                                      Prints FACE DOWN (letters in the first 1.2 mm). Slots 3 + 4. Easiest.
  3_clawd_stand_tiny_clawd_A_stand_black.stl + _B_text_red.stl  the same two parts as STLs: load both,
                                      say yes to "load as one object with multiple parts", set black + red.
  3_clawd_stand_30deg.stl            plain stand, prints standing on its bottom, one colour

Single STLs, numbered (body must stay FACE DOWN exactly as it loads):
  1_clawd_body_face_down.stl, 2_clawd_back_plate.stl, 3_clawd_stand_30deg.stl
  football_badge.3mf  (badge version: 2 bits of 1.75 mm filament ~4.5 mm long into the
                       two small holes on the face, press the badge on, a drop of glue)

tests (optional)/tolerance_rings.3mf: 5 thin rings (1-5 notches). The body uses ring 3.
If another ring is the snug one, set rib_side / rib_floor in mochi_fit.scad to its
values (see tolerance_rings.scad) and run build.sh.
"""

MAC_README = """MAC - what to print (no supports)

  mac_print_set.3mf : slot 2 = white (body face down + back plate), slot 3 = black (ramp stand)
                      (AMS lite slots: 1 yellow, 2 white, 3 black, 4 red)

Single STLs, numbered (body must stay FACE DOWN exactly as it loads):
  1_mac_body_face_down.stl, 2_mac_back_plate.stl, 3_mac_stand_ramp_30deg.stl
  3_mac_stand_floppy_30deg (alternative).stl = the floppy-disk stand instead of the ramp

The body has the same gentle crush ribs as the Clawd, so the electronics sit snug.
"""


def load(path, rot_x=0):
    m = trimesh.load(OUT / path if not str(path).startswith("original") else Path(__file__).parent / path)
    if rot_x:
        m.apply_transform(trimesh.transformations.rotation_matrix(np.radians(rot_x), [1, 0, 0]))
    return m


def place(meshes, x, y):
    """Move a group of meshes together so its corner sits at x, y on the bed."""
    lo = np.min([m.bounds[0] for m in meshes], axis=0)
    for m in meshes:
        m.apply_translation([x - lo[0], y - lo[1], -lo[2]])


def write_3mf(path, objects):
    """objects: list of (name, [(part name, mesh, filament)])."""
    res, items, conf, mats = [], [], [], []
    filaments = sorted({f for _, parts in objects for _, _, f in parts})
    mat_index = {f: i for i, f in enumerate(filaments)}
    mats = "".join(f'<base name="filament {f}" displaycolor="{COLOURS[f]}"/>' for f in filaments)
    nid = 1
    for name, parts in objects:
        comp_ids, part_conf = [], []
        for pname, mesh, fil in parts:
            v = "".join(f'<vertex x="{x:.8f}" y="{y:.8f}" z="{z:.8f}"/>' for x, y, z in mesh.vertices)
            t = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in mesh.faces)
            res.append(f'<object id="{nid}" type="model" name="{pname}" pid="100" pindex="{mat_index[fil]}">'
                       f'<mesh><vertices>{v}</vertices><triangles>{t}</triangles></mesh></object>')
            comp_ids.append(nid)
            part_conf.append(f'<part id="{nid}" subtype="normal_part"><metadata key="name" value="{pname}"/>'
                             f'<metadata key="extruder" value="{fil}"/></part>')
            nid += 1
        comps = "".join(f'<component objectid="{i}"/>' for i in comp_ids)
        res.append(f'<object id="{nid}" type="model" name="{name}"><components>{comps}</components></object>')
        items.append(f'<item objectid="{nid}"/>')
        conf.append(f'<object id="{nid}"><metadata key="name" value="{name}"/>'
                    f'<metadata key="extruder" value="{parts[0][2]}"/>{"".join(part_conf)}</object>')
        nid += 1
    model = ('<?xml version="1.0" encoding="UTF-8"?>'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             f'<resources><basematerials id="100">{mats}</basematerials>{"".join(res)}</resources>'
             f'<build>{"".join(items)}</build></model>')
    config = f'<?xml version="1.0" encoding="UTF-8"?><config>{"".join(conf)}</config>'
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
    clawd_dir, mac_dir = PRINT / "clawd", PRINT / "mac"
    for d in (clawd_dir, mac_dir):
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)

    # --- Clawd ---
    body = load("clawd_v1_housing.stl", rot_x=90)          # face down on the bed
    plate = load("original/Back Plate-Body.stl")            # flat, as designed
    stand = load("stand_clawd_30deg.stl")                   # flat on its bottom
    ball_black, ball_white = load("football_badge_black.stl"), load("football_badge_white.stl")
    place([body], 8, 8)
    place([stand], 104, 8)
    place([plate], 8, 72)
    place([ball_black, ball_white], 70, 80)
    write_3mf(clawd_dir / "clawd_print_set.3mf", [
        ("clawd body", [("clawd body", body, RED)]),
        ("back plate", [("back plate", plate, RED)]),
        ("stand 30 deg", [("stand", stand, BLACK)]),
        ("football badge", [("ball base", ball_black, BLACK), ("ball panels", ball_white, WHITE)]),
    ])
    # alternative: the football built into the body, colours inlaid on the face (needs AMS).
    # Just the two pieces: the body (red, ball black + white) and the back plate (red).
    inlay = [load(f"clawd_ball_body_{c}.stl", rot_x=90) for c in ("red", "black", "white")]
    plate2 = load("original/Back Plate-Body.stl")
    place(inlay, 8, 8)
    place([plate2], 100, 8)
    write_3mf(clawd_dir / "clawd_print_set_ball_in_body.3mf", [
        ("clawd body with football", [("body", inlay[0], RED), ("ball black", inlay[1], BLACK), ("ball white", inlay[2], WHITE)]),
        ("back plate", [("back plate", plate2, RED)]),
    ])
    # the stand with the red "tiny clawd" lockup (pixel Clawd + serif name), printed face down
    st_body, st_label = load("stand_clawd_30deg_tiny_body.stl", rot_x=90), load("stand_clawd_30deg_tiny_label.stl", rot_x=90)
    place([st_body, st_label], 40, 40)
    write_3mf(clawd_dir / "3_clawd_stand_30deg_tiny_clawd.3mf", [
        ("tiny clawd stand", [("stand", st_body, BLACK), ("tiny clawd", st_label, RED)]),
    ])
    # the same two parts as STLs, already face down and lined up (load both, set the colours)
    st_body.export(clawd_dir / "3_clawd_stand_tiny_clawd_A_stand_black.stl")
    st_label.export(clawd_dir / "3_clawd_stand_tiny_clawd_B_text_red.stl")
    write_3mf(clawd_dir / "football_badge.3mf", [("football badge", [("ball base", ball_black, BLACK), ("ball panels", ball_white, WHITE)])])
    for name, m in [("1_clawd_body_face_down", body), ("2_clawd_back_plate", plate), ("3_clawd_stand_30deg", stand)]:
        m.export(clawd_dir / f"{name}.stl")
    rings = load("tolerance_rings.stl")
    place([rings], 10, 10)
    tests = clawd_dir / "tests (optional)"
    tests.mkdir()
    write_3mf(tests / "tolerance_rings.3mf", [("tolerance rings", [("rings", rings, RED)])])
    fit = load("clawd_housing_fit_test.stl")
    fit.export(tests / "clawd_fit_test_back_22mm.stl")

    # --- Mac ---
    mbody = load("mac_housing.stl", rot_x=90)
    mplate = load("mac_back_plate.stl")
    ramp = load("stand_mac_ramp_30deg.stl")
    floppy = load("stand_mac_floppy_30deg.stl")
    place([mbody], 8, 8)
    place([ramp], 70, 8)
    place([mplate], 8, 70)
    write_3mf(mac_dir / "mac_print_set.3mf", [
        ("mac body", [("mac body", mbody, WHITE)]),
        ("mac back plate", [("mac back plate", mplate, WHITE)]),
        ("ramp stand 30 deg", [("ramp stand", ramp, BLACK)]),
    ])
    for name, m in [("1_mac_body_face_down", mbody), ("2_mac_back_plate", mplate),
                    ("3_mac_stand_ramp_30deg", ramp), ("3_mac_stand_floppy_30deg (alternative)", floppy)]:
        m.export(mac_dir / f"{name}.stl")
    for m in (body, plate, stand, ball_black, ball_white, mbody, mplate, ramp, floppy, rings, fit, *inlay, st_body, st_label):
        assert m.is_watertight
    (clawd_dir / "README.txt").write_text(CLAWD_README)
    (mac_dir / "README.txt").write_text(MAC_README)
    print("PRINT folder ready")


if __name__ == "__main__":
    main()
