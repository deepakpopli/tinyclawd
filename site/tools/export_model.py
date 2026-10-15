#!/usr/bin/env python3
"""Web model of the printed Clawd for the site.
  .venv/bin/python site/tools/export_model.py   -> site/assets/3d/clawd.bin

Reads the STLs from hardware/out and hardware/original (run hardware/build.sh first).
Turns them to y up, front facing +z, mm units, floor at y = 0.

clawd.bin is deflate (zlib) of:
  uint32 header length, header json (padded to 4 bytes), then for each part:
  int16 positions (x3, 1/100 mm), int8 normals (x3), padding to 4, uint16 or uint32 indices"""
import json
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
import trimesh

here = Path(__file__).resolve().parent
repo = here / "../.."
hw = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / "hardware"
out = here / "../assets/3d/clawd.bin"

# back plate STL coords -> housing coords, same matrix as hardware/build_live_viewer.py
PLATE = np.array([[1, 0, 0, 44.75], [0, 0, 1, 42.5], [0, 1, 0, 0], [0, 0, 0, 1]], float)
# z up, front -y  ->  y up, front +z
TO_WEB = np.array([[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, 0], [0, 0, 0, 1]], float)
CREASE = np.radians(35)   # sharper edges than this stay hard, the blocky look

PARTS = [
    ("body", "out/clawd_ball_body_red.stl", "#c8372d", None),
    ("black", "out/clawd_ball_body_black.stl", "#17181b", None),
    ("white", "out/clawd_ball_body_white.stl", "#eeeeea", None),
    ("plate", "original/Back Plate-Body.stl", "#c8372d", PLATE),
]
# OLED window: 32 x 16 mm at (23, 2.05, 22.75) in housing coords, facing -y
SCREEN = (23.0, 2.05, 22.75)


def main():
    meshes = []
    for name, path, color, matrix in PARTS:
        m = trimesh.load(hw / path)
        if matrix is not None:
            m.apply_transform(matrix)
        m.apply_transform(TO_WEB)
        meshes.append((name, color, m))

    lo = np.min([m.bounds[0] for _, _, m in meshes], axis=0)
    hi = np.max([m.bounds[1] for _, _, m in meshes], axis=0)
    shift = np.array([-(lo[0] + hi[0]) / 2, -lo[1], -(lo[2] + hi[2]) / 2])

    header = {"parts": [], "size": (hi - lo).round(2).tolist()}
    screen = TO_WEB[:3, :3] @ np.array(SCREEN) + shift
    header["screen"] = {"pos": screen.round(3).tolist(), "w": 32, "h": 16}

    chunks = []
    for name, color, m in meshes:
        m.apply_translation(shift)
        s = trimesh.graph.smooth_shade(m, angle=CREASE)
        pos = np.round(s.vertices * 100).astype("<i2")
        nrm = np.round(s.vertex_normals * 127).astype("i1")
        idx = s.faces.astype("<u2" if len(s.vertices) < 65536 else "<u4").ravel()
        blob = pos.tobytes() + nrm.tobytes()
        blob += b"\0" * (-len(blob) % 4) + idx.tobytes()
        blob += b"\0" * (-len(blob) % 4)
        chunks.append(blob)
        header["parts"].append({"name": name, "color": color, "verts": len(s.vertices),
                                "index": len(idx), "wide": len(s.vertices) >= 65536})
        print(f"{name:6} {len(s.vertices):6} verts {len(s.faces):6} tris")

    head = json.dumps(header, separators=(",", ":")).encode()
    head += b" " * (-len(head) % 4)
    raw = struct.pack("<I", len(head)) + head + b"".join(chunks)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(zlib.compress(raw, 9))
    print(f"wrote {out.name} {out.stat().st_size / 1024:.0f} KB (raw {len(raw) / 1024:.0f} KB)")
    print(json.dumps(header))


if __name__ == "__main__":
    main()
