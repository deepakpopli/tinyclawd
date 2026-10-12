#!/usr/bin/env python3
"""Clawd 3D viewer with the simulator animations playing on the OLED.
  python3 build_live_viewer.py   -> out/clawd_live.html
needs build.sh outputs in out/ and the sim frames in ../firmware/sim/out/"""
import base64
import json
import re
import zlib
from pathlib import Path

here = Path(__file__).parent
sim = here / "../firmware/sim/out"
src = (here / "build_viewer.py").read_text()

# each anim: unique frames back to back (1024 bytes, SH1106 pages) + how many 20 ms ticks each lasts
anims = {}
for f in sorted(sim.glob("*.frames")):
    data = f.read_bytes()
    frames, ticks = [], []
    for i in range(0, len(data) - 1023, 1024):
        buf = data[i:i + 1024]
        if frames and frames[-1] == buf:
            ticks[-1] += 1
        else:
            frames.append(buf)
            ticks.append(1)
    anims[f.stem] = {"d": base64.b64encode(zlib.compress(b"".join(frames), 9)).decode(), "t": ticks}

parts = {
    "body": here / "out/clawd_ball_body_red.stl",
    "black": here / "out/clawd_ball_body_black.stl",
    "white": here / "out/clawd_ball_body_white.stl",
    "plate": here / "original/Back Plate-Body.stl",
}
stl = {k: base64.b64encode(p.read_bytes()).decode() for k, p in parts.items()}

html = """<!doctype html>
<html><head><meta charset="utf-8"><title>Tiny Clawd</title>
<style>
 body { margin: 0; background: #20232a; color: #ddd; font: 14px sans-serif; overflow: hidden; }
 #ui { position: absolute; top: 12px; left: 12px; background: #2c3038cc; padding: 10px 12px; border-radius: 8px; width: 230px; }
 #ui h3 { margin: 0 0 8px; font-size: 15px; color: #fff; }
 label { display: block; margin: 6px 0; cursor: pointer; }
 select { width: 100%; font-size: 14px; }
 #list { max-height: 55vh; overflow-y: auto; margin-top: 8px; }
 #list div { padding: 2px 6px; cursor: pointer; border-radius: 4px; }
 #list div:hover { background: #3a3f4a; }
 #list div.on { background: #c8372d; color: #fff; }
 #oled { position: absolute; top: 12px; right: 12px; width: 256px; image-rendering: pixelated; border: 6px solid #111; border-radius: 4px; }
 #hint { position: absolute; bottom: 10px; left: 12px; color: #999; font-size: 12px; }
</style>
<script type="importmap">
{ "imports": { "three": "https://cdn.jsdelivr.net/npm/three@0.169.0/build/three.module.js",
               "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.169.0/examples/jsm/" } }
</script></head><body>
<div id="ui">
 <h3>Tiny Clawd</h3>
 <label><input type="checkbox" id="auto" checked> play all in turn</label>
 <label><input type="checkbox" id="spin"> spin</label>
 <label><input type="checkbox" id="plate" checked> back plate</label>
 <div id="list"></div>
</div>
<canvas id="oled" width="128" height="64"></canvas>
<div id="hint">drag to rotate, scroll to zoom, right drag to pan. click a name to play it.</div>
<script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { STLLoader } from 'three/addons/loaders/STLLoader.js';

const STL = __STL__, ANIMS = __ANIMS__;
const bytes = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
const geo = k => { const g = new STLLoader().parse(bytes(STL[k]).buffer); g.computeVertexNormals(); return g; };
async function inflate(s) {
  const st = new Blob([bytes(s)]).stream().pipeThrough(new DecompressionStream('deflate'));
  return new Uint8Array(await new Response(st).arrayBuffer());
}

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(devicePixelRatio); renderer.setSize(innerWidth, innerHeight);
document.body.appendChild(renderer.domElement);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x20232a);
const camera = new THREE.PerspectiveCamera(35, innerWidth / innerHeight, 1, 2000);
camera.up.set(0, 0, 1); camera.position.set(-30, -150, 50);
const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(23, 20, 20); controls.update();
scene.add(new THREE.HemisphereLight(0xffffff, 0x444455, 1.4));
const sun = new THREE.DirectionalLight(0xffffff, 2.2); sun.position.set(-80, -120, 160); scene.add(sun);
const fill = new THREE.DirectionalLight(0xffffff, 0.8); fill.position.set(120, 100, 40); scene.add(fill);

const mat = c => new THREE.MeshStandardMaterial({ color: c, roughness: 0.55, metalness: 0.02 });
const model = new THREE.Group(); scene.add(model);
for (const [k, c] of [['body', 0xc8372d], ['black', 0x1a1a1a], ['white', 0xf2f2f2]]) model.add(new THREE.Mesh(geo(k), mat(c)));
// plate STL coords -> housing coords, same as build_viewer.py
const plate = new THREE.Mesh(geo('plate'), mat(0xc8372d)); plate.matrixAutoUpdate = false;
plate.matrix.set(1,0,0,44.75, 0,0,1,42.5, 0,1,0,0, 0,0,0,1); model.add(plate);

// the OLED: 128x64 canvas drawn like the real panel, mapped onto the screen window
const oled = document.getElementById('oled'), ctx = oled.getContext('2d');
const img = ctx.createImageData(128, 64);
const tex = new THREE.CanvasTexture(oled); tex.magFilter = THREE.NearestFilter; tex.minFilter = THREE.LinearFilter;
tex.colorSpace = THREE.SRGBColorSpace;
const back = new THREE.Mesh(new THREE.BoxGeometry(32, 0.4, 16), new THREE.MeshBasicMaterial({ color: 0x050607 }));
back.position.set(23, 2.3, 22.75); model.add(back);
const face = new THREE.Mesh(new THREE.PlaneGeometry(32, 16), new THREE.MeshBasicMaterial({ map: tex }));
face.rotation.x = Math.PI / 2; face.position.set(23, 2.05, 22.75); model.add(face);

function draw(buf, off) {
  const p = img.data;
  for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
    const on = buf[off + (y >> 3) * 128 + x] >> (y & 7) & 1, i = (y * 128 + x) * 4;
    p[i] = on ? 232 : 10; p[i + 1] = on ? 240 : 12; p[i + 2] = on ? 255 : 16; p[i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0); tex.needsUpdate = true;
}

const names = Object.keys(ANIMS), cache = {};
const list = document.getElementById('list');
for (const n of names) {
  const d = document.createElement('div'); d.textContent = n; d.onclick = () => play(n); list.appendChild(d);
}
let cur = null, start = 0;
async function play(n) {
  cache[n] ??= await inflate(ANIMS[n].d);
  cur = n; start = performance.now();
  for (const d of list.children) d.className = d.textContent === n ? 'on' : '';
  list.querySelector('.on')?.scrollIntoView({ block: 'nearest' });
}
function frameAt(n, ms) {
  const t = ANIMS[n].t, tick = Math.floor(ms / 20);
  let sum = 0;
  for (let i = 0; i < t.length; i++) { sum += t[i]; if (tick < sum) return i; }
  return -1;   // finished
}

const $ = id => document.getElementById(id);
$('plate').onchange = e => plate.visible = e.target.checked;
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight);
});
renderer.setAnimationLoop(now => {
  if (cur && cache[cur]) {
    let i = frameAt(cur, now - start);
    if (i < 0) {
      if ($('auto').checked) { play(names[(names.indexOf(cur) + 1) % names.length]); i = 0; }
      else { start = now; i = 0; }
    }
    if (cache[cur]) draw(cache[cur], i * 1024);
  }
  if ($('spin').checked) model.rotation.z += 0.005;
  controls.update(); renderer.render(scene, camera);
});
play(names.includes('blink') ? 'blink' : names[0]);
</script></body></html>
"""
out = here / "out/clawd_live.html"
out.write_text(html.replace("__STL__", json.dumps(stl)).replace("__ANIMS__", json.dumps(anims)))
print("wrote", out, f"{out.stat().st_size / 1e6:.1f} MB,", len(anims), "animations")
