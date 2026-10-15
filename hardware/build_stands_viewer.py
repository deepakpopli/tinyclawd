#!/usr/bin/env python3
"""Build out/stands_viewer.html: every stand with its housing sitting on it,
side by side (cs.VARIANTS in check_stands.py), with a typical 30 mm USB-C
plug in the back to show the desk clearance, and the football badge on the Clawd. three.js comes from the jsdelivr CDN.
Run from this folder: python3 build_stands_viewer.py"""
import base64
import json
from pathlib import Path

import numpy as np
import trimesh

import check_stands as cs

CDN = "https://cdn.jsdelivr.net/npm/three@0.169.0/"
LABELS = {"mac_ramp": "Mac: simple ramp", "mac_floppy": "Mac: floppy disk", "clawd": "Clawd: footprints"}
COLOURS = {"mac": 0xf4f3ee, "clawd": 0xc8372d, "stand": 0x3a3f4a, "plate": 0xe2e1dc, "screen": 0x050607,
           "plug": 0x6d8fd8, "ball_black": 0x1a1a1a, "ball_white": 0xf2f2f2}
PLUG_LEN = cs.USB_PLUG   # a USB-C plug, sticking out of the back


BADGE_T = np.array([[1, 0, 0, 50.5], [0, 0, -1, -1.5], [0, 1, 0, 4.0], [0, 0, 0, 1]], float)  # football_badge.py

parts, labels = [], []
for i, (name, angle, riser) in enumerate(cs.VARIANTS):
    housing_f, plate_f, y1, zb = cs.STANDS[name]
    M = cs.on_stand(y1, zb, riser, angle)
    shift = trimesh.transformations.translation_matrix([i * 110, 0, 0])
    items = [(cs.stl_name(name, angle, riser), np.eye(4), "stand"), (housing_f, M, "clawd" if "clawd" in name else "mac"),
             (plate_f, M @ cs.PLATE_T, "plate")]
    if name == "clawd":
        items += [("out/football_badge_black.stl", M @ BADGE_T, "ball_black"),
                  ("out/football_badge_white.stl", M @ BADGE_T, "ball_white")]
    for f, T, kind in items:
        m = trimesh.load(f)
        m.apply_transform(shift @ T)
        parts.append({"stl": base64.b64encode(m.export(file_type="stl")).decode(), "colour": COLOURS[kind]})
    # dark screen in the window, and a USB-C plug in the back
    screen = trimesh.creation.box(extents=[32, 0.4, 16])
    screen.apply_translation([23, 2.3, 22.75])
    plug = trimesh.creation.box(extents=[12, PLUG_LEN, 6.5])
    plug.apply_translation([23, 48 + PLUG_LEN / 2, 8.5])
    for extra, kind in [(screen, "screen"), (plug, "plug")]:
        extra.apply_transform(shift @ M)
        parts.append({"stl": base64.b64encode(extra.export(file_type="stl")).decode(), "colour": COLOURS[kind]})
    labels.append({"text": f"{LABELS[name]}, {angle} deg" + (f" +{riser} mm" if riser else ""), "x": i * 110 + 23, "y": 0})

importmap = json.dumps({"imports": {
    "three": CDN + "build/three.module.js",
    "three/addons/": CDN + "examples/jsm/",
}})

html = """<!doctype html><html><head><meta charset="utf-8"><title>Mochi stands</title>
<style>body{margin:0;background:#1e2127;color:#ddd;font:14px sans-serif;overflow:hidden}
#t{position:absolute;top:10px;left:12px}#t b{color:#fff;font-size:15px}
.lab{position:absolute;color:#fff;background:#2a2e36cc;padding:3px 8px;border-radius:5px;font-size:13px;transform:translate(-50%,0)}
#bar{position:absolute;top:10px;right:12px;display:flex;gap:6px}
button{background:#2a2e36;color:#ddd;border:1px solid #3c414c;border-radius:6px;padding:5px 10px;cursor:pointer}</style>
<script type="importmap">__IMPORTMAP__</script></head><body>
<div id="t"><b>Mochi stands, 30 deg (blue = your 5 mm USB-C plug)</b><div>drag to rotate, scroll to zoom, right drag to pan</div></div>
<div id="bar"><button id="side">Side view</button><button id="eye">Desk eye level</button><button id="iso">Iso</button></div>
<script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { STLLoader } from 'three/addons/loaders/STLLoader.js';
const PARTS = __PARTS__, LABELS = __LABELS__;
const r = new THREE.WebGLRenderer({antialias:true}); r.setPixelRatio(devicePixelRatio); r.setSize(innerWidth, innerHeight);
document.body.appendChild(r.domElement);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x1e2127);
const cam = new THREE.PerspectiveCamera(30, innerWidth/innerHeight, 1, 5000); cam.up.set(0,0,1);
const ctl = new OrbitControls(cam, r.domElement);
scene.add(new THREE.HemisphereLight(0xffffff, 0x444455, 1.3));
const sun = new THREE.DirectionalLight(0xffffff, 2); sun.position.set(-150,-300,400); scene.add(sun);
const fill = new THREE.DirectionalLight(0xffffff, 0.6); fill.position.set(300,200,100); scene.add(fill);
const loader = new STLLoader();
for (const p of PARTS) {
  const g = loader.parse(Uint8Array.from(atob(p.stl), c => c.charCodeAt(0)).buffer); g.computeVertexNormals();
  scene.add(new THREE.Mesh(g, new THREE.MeshStandardMaterial({color:p.colour, roughness:0.6})));
}
const grid = new THREE.GridHelper(600, 60, 0x3c414c, 0x2c3038); grid.rotation.x = Math.PI/2; grid.position.set(220,20,-0.01); scene.add(grid);
const target = new THREE.Vector3(220, 15, 25);
const views = {iso:[-200,-520,300], side:[620,15,30], eye:[0,-640,120]};
function view(k){ cam.position.copy(target).add(new THREE.Vector3(...views[k])); ctl.target.copy(target); ctl.update(); }
view('iso'); for (const k of Object.keys(views)) document.getElementById(k).onclick = () => view(k);
const tags = LABELS.map(l => { const d = document.createElement('div'); d.className='lab'; d.textContent=l.text; document.body.appendChild(d); return [d, new THREE.Vector3(l.x, l.y - 40, 0)]; });
addEventListener('resize', () => { cam.aspect = innerWidth/innerHeight; cam.updateProjectionMatrix(); r.setSize(innerWidth, innerHeight); });
r.setAnimationLoop(() => { ctl.update(); r.render(scene, cam);
  for (const [d, p] of tags) { const v = p.clone().project(cam); d.style.left = ((v.x+1)/2*innerWidth)+'px'; d.style.top = ((1-v.y)/2*innerHeight)+'px'; } });
</script></body></html>"""
out = Path(__file__).parent / "out/stands_viewer.html"
out.write_text(html.replace("__IMPORTMAP__", importmap).replace("__PARTS__", json.dumps(parts))
               .replace("__LABELS__", json.dumps(labels)))
print("wrote", out)
