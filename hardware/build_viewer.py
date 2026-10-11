#!/usr/bin/env python3
"""Build a 3D viewer with the STLs embedded (opens offline from file://,
three.js itself comes from the jsdelivr CDN).
  python3 build_viewer.py          -> out/viewer.html (Mac)
  python3 build_viewer.py clawd    -> out/clawd_viewer.html"""
import base64
import struct
import sys
from pathlib import Path

here = Path(__file__).parent
designs = {
    "mac": ("out/mac_housing.stl", "out/mac_back_plate.stl", "0xf4f3ee", "Mochi Mac housing", "out/viewer.html"),
    "clawd": ("out/clawd_v1_housing.stl", "original/Back Plate-Body.stl", "0xc8372d", "Clawd pet with football", "out/clawd_viewer.html"),
    "clawd_inlay": ("out/clawd_ball_body_red.stl", "original/Back Plate-Body.stl", "0xc8372d",
                    "Clawd, football built into the body (3 colours)", "out/clawd_inlay_viewer.html"),
}
housing, plate, colour, title, out_name = designs[sys.argv[1] if len(sys.argv) > 1 else "mac"]
parts = {
    "housing": here / housing,
    "plate": here / plate,
    "original": here / "original/Housing-Body.stl",
}
if out_name.endswith("clawd_viewer.html"):   # the football badge, glued on the face
    parts["ball_black"] = here / "out/football_badge_black.stl"
    parts["ball_white"] = here / "out/football_badge_white.stl"
if "inlay" in out_name:                    # the football as part of the body
    parts["inlay_black"] = here / "out/clawd_ball_body_black.stl"
    parts["inlay_white"] = here / "out/clawd_ball_body_white.stl"

def min_z(path):
    d = path.read_bytes()
    if d[:5] == b"solid" and b"vertex" in d[:1000]:
        return min(float(line.split()[3]) for line in d.decode().splitlines() if line.strip().startswith("vertex"))
    n = struct.unpack("<I", d[80:84])[0]
    return min(struct.unpack_from("<f", d, 84 + i * 50 + 12 + j * 12 + 8)[0] for i in range(n) for j in range(3))
floor = min_z(parts["housing"])
data = {k: base64.b64encode(p.read_bytes()).decode() for k, p in parts.items()}

html = """<!doctype html>
<html><head><meta charset="utf-8"><title>__TITLE__</title>
<style>
 body { margin: 0; background: #20232a; color: #ddd; font: 14px sans-serif; overflow: hidden; }
 #ui { position: absolute; top: 12px; left: 12px; background: #2c3038cc; padding: 10px 12px; border-radius: 8px; }
 #ui h3 { margin: 0 0 8px; font-size: 15px; color: #fff; }
 label { display: block; margin: 4px 0; cursor: pointer; }
 input[type=range] { width: 180px; }
 #hint { position: absolute; bottom: 10px; left: 12px; color: #999; font-size: 12px; }
</style>
<script type="importmap">
{ "imports": { "three": "https://cdn.jsdelivr.net/npm/three@0.169.0/build/three.module.js",
               "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.169.0/examples/jsm/" } }
</script></head><body>
<div id="ui">
 <h3>__TITLE__</h3>
 <label><input type="checkbox" id="showPlate" checked> back plate</label>
 <label>slide plate out <input type="range" id="slide" min="0" max="60" value="0"></label>
 <label><input type="checkbox" id="xray"> see through housing</label>
 <label><input type="checkbox" id="showOrig"> original housing (side by side)</label>
 <label><input type="checkbox" id="spin"> spin</label>
</div>
<div id="hint">drag to rotate, scroll to zoom, right drag to pan.</div>
<script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { STLLoader } from 'three/addons/loaders/STLLoader.js';

const DATA = __DATA__;
const geo = k => {
  const b = Uint8Array.from(atob(DATA[k]), c => c.charCodeAt(0)).buffer;
  const g = new STLLoader().parse(b); g.computeVertexNormals(); return g;
};

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(devicePixelRatio); renderer.setSize(innerWidth, innerHeight);
document.body.appendChild(renderer.domElement);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x20232a);
const camera = new THREE.PerspectiveCamera(35, innerWidth / innerHeight, 1, 2000);
camera.up.set(0, 0, 1); camera.position.set(-70, -160, 70);
const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(23, 24, 17); controls.update();

scene.add(new THREE.HemisphereLight(0xffffff, 0x444455, 1.4));
const sun = new THREE.DirectionalLight(0xffffff, 2.2); sun.position.set(-80, -120, 160); scene.add(sun);
const fill = new THREE.DirectionalLight(0xffffff, 0.8); fill.position.set(120, 100, 40); scene.add(fill);

const mat = (c, o = {}) => new THREE.MeshStandardMaterial({ color: c, roughness: 0.55, metalness: 0.02, ...o });
const housingMat = mat(__COLOUR__);
const housing = new THREE.Mesh(geo('housing'), housingMat); scene.add(housing);

// plate STL coords -> housing coords: x+44.75, y = 42.5 + z, z = y
const plate = new THREE.Mesh(geo('plate'), mat(__PLATE_COLOUR__));
const plateBase = new THREE.Matrix4().set(1,0,0,44.75, 0,0,1,42.5, 0,1,0,0, 0,0,0,1);
const plateGroup = new THREE.Group(); plate.matrixAutoUpdate = false; plate.matrix.copy(plateBase);
plateGroup.add(plate); scene.add(plateGroup);

// preview of the OLED (not printed): dark screen, plus Clawd's eyes on the Clawd design
const screen = new THREE.Mesh(new THREE.BoxGeometry(32, 0.4, 16), new THREE.MeshBasicMaterial({ color: 0x050607 }));
screen.position.set(23, 2.3, 22.75); scene.add(screen);
// football badge (Clawd): print coords -> on the face, see football_badge.py
if (DATA.ball_black) {
  const T = new THREE.Matrix4().set(1,0,0,50.5, 0,0,-1,-1.5, 0,1,0,4.0, 0,0,0,1);
  for (const [k, c] of [['ball_black', 0x1a1a1a], ['ball_white', 0xf2f2f2]]) {
    const m = new THREE.Mesh(geo(k), mat(c)); m.matrixAutoUpdate = false; m.matrix.copy(T); scene.add(m);
  }
}
for (const [k, c] of [['inlay_black', 0x1a1a1a], ['inlay_white', 0xf2f2f2]])
  if (DATA[k]) scene.add(new THREE.Mesh(geo(k), mat(c)));
if (__EYES__) for (const x of [15, 31]) {
  const eye = new THREE.Mesh(new THREE.BoxGeometry(3.5, 0.2, 7), new THREE.MeshBasicMaterial({ color: 0xffffff }));
  eye.position.set(x, 2.0, 23.5); scene.add(eye);
}

const orig = new THREE.Mesh(geo('original'), mat(0x8fa3b8));
orig.position.set(46 + 75, 0, 0); orig.visible = false; scene.add(orig);

const grid = new THREE.GridHelper(300, 30, 0x3a3f4a, 0x2e323b);
grid.rotation.x = Math.PI / 2; grid.position.z = __FLOOR__; scene.add(grid);

const $ = id => document.getElementById(id);
$('showPlate').onchange = e => plateGroup.visible = e.target.checked;
$('slide').oninput = e => plateGroup.position.z = -e.target.value;
$('xray').onchange = e => { housingMat.transparent = e.target.checked; housingMat.opacity = e.target.checked ? 0.35 : 1; housingMat.depthWrite = !e.target.checked; };
$('showOrig').onchange = e => orig.visible = e.target.checked;

addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});
renderer.setAnimationLoop(() => {
  if ($('spin').checked) { scene.rotation.z += 0.005; }
  controls.update(); renderer.render(scene, camera);
});
</script></body></html>
"""
import json
out = here / out_name
out.write_text(html.replace("__DATA__", json.dumps(data)).replace("__TITLE__", title)
               .replace("__COLOUR__", colour)
               .replace("__PLATE_COLOUR__", colour if "clawd" in out_name else "0xe2e1dc").replace("__FLOOR__", f"{floor - 0.01:.2f}")
               .replace("__EYES__", "true" if "clawd" in out_name else "false"))
print("wrote", out, f"{out.stat().st_size / 1e6:.1f} MB")
