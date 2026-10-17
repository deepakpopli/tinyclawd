#!/usr/bin/env python3
"""Web model of the 30 deg Clawd stand, for the desk in the hero.
  .venv/bin/python site/tools/export_desk.py   -> site/assets/3d/desk.bin

Same file format as clawd.bin (see export_model.py). The stand is level: y up, front +z,
floor at y = 0, in mm. The header adds:
  perch  4x4 row major, Clawd model coords (clawd.bin) -> stand coords, Clawd sitting on it
  port   the USB-C port on Clawd's back, in Clawd model coords
  back   the direction the cable leaves the port, Clawd model coords"""
import math

import numpy as np
import trimesh

from export_model import TO_WEB, hw, here, printed, write

out = here / "../assets/3d/desk.bin"

# stands.scad: housing coords -> stand coords is translate(0, 0, LIFT) rotate([-30, 0, 0])
ANGLE = 30
ZB = -10.5 + 3                       # platform top, legs go 3 mm into the pockets
Y1 = 41 + 0.3 + 4                    # back of the platform
LIFT = -(Y1 * math.sin(math.radians(-ANGLE)) + ZB * math.cos(math.radians(ANGLE)) - 3)

PARTS = [
    ("stand", "out/stand_clawd_30deg_tiny_body.stl", "#141414"),
    ("label", "out/stand_clawd_30deg_tiny_label.stl", "#c8372d"),
]
PORT = (23, 55.5, 9)                 # housing coords, where the plug sits on the back plate


def main():
    _, _, _, shift = printed()
    a = math.radians(-ANGLE)
    seat = np.array([[1, 0, 0, 0],
                     [0, math.cos(a), -math.sin(a), 0],
                     [0, math.sin(a), math.cos(a), LIFT],
                     [0, 0, 0, 1]])
    unshift = np.eye(4)
    unshift[:3, 3] = -shift
    # Clawd model -> housing -> stand -> web
    perch = TO_WEB @ seat @ np.linalg.inv(TO_WEB) @ unshift

    meshes = []
    for name, path, color in PARTS:
        m = trimesh.load(hw / path)
        m.apply_transform(TO_WEB)
        meshes.append((name, color, m, {}))
    lo = np.min([m.bounds[0] for _, _, m, _ in meshes], axis=0)
    hi = np.max([m.bounds[1] for _, _, m, _ in meshes], axis=0)
    # stand centred under Clawd's x, floor at 0, Clawd's own x/z centre as the origin
    centre = perch @ np.array([0, 0, 0, 1])
    move = np.array([-centre[0], -lo[1], -centre[2]])
    perch[:3, 3] += move

    port = TO_WEB[:3, :3] @ np.array(PORT) + shift
    back = TO_WEB[:3, :3] @ np.array([0, 1, 0])
    header = {"parts": [], "size": (hi - lo).round(2).tolist(),
              "perch": perch.round(5).ravel().tolist(),
              "port": port.round(3).tolist(), "back": back.round(3).tolist()}
    write(meshes, header, move, out)


if __name__ == "__main__":
    main()
