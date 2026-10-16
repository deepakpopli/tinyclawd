// Tiny Clawd in 3D. Follows the page scroll and plays the screen animations.
//
// The page gives us:
//   <canvas id="clawd3d"> inside a sticky box
//   sections with data-clawd-anim="name[,name...]", data-clawd-pose="front|three-quarter|side|back|top|close|exploded|
//     quarter-right|near|low|top-front"
// The canvas is its own box next to the text, so the model always fits inside it.
// We give back window.Clawd3D = { play(name), pose(name) }

import * as THREE from '../vendor/three.js';

const MODEL_URL = new URL('../assets/3d/clawd.bin', import.meta.url);
const FRAMES_URL = new URL('../assets/frames/', import.meta.url);
const FALLBACK_PHOTO = 'assets/photos/p1-desk.jpg';
const FALLBACK_GIF = 'assets/hero.gif';

// yaw = model turn, pitch = camera height angle, dist = camera distance in mm,
// look = what the camera looks at ('body' or 'screen'), explode = 0..1
const POSES = {
  'front':         { yaw: 0,    pitch: 7,  dist: 270, look: 0, explode: 0 },
  'three-quarter': { yaw: -32,  pitch: 14, dist: 280, look: 0, explode: 0 },
  'side':          { yaw: -74,  pitch: 9,  dist: 280, look: 0, explode: 0 },
  'back':          { yaw: -180, pitch: 14, dist: 280, look: 0, explode: 0 },
  'top':           { yaw: -24,  pitch: 56, dist: 290, look: 0, explode: 0 },
  'close':         { yaw: -6,   pitch: 3,  dist: 125, look: 1, explode: 0 },
  'exploded':      { yaw: -38,  pitch: 16, dist: 320, look: 0, explode: 1 },
  // softer angles that always keep the screen in view
  'quarter-right': { yaw: 26,   pitch: 10, dist: 260, look: 0, explode: 0 },
  'near':          { yaw: -18,  pitch: 9,  dist: 210, look: 0.4, explode: 0 },
  'low':           { yaw: 14,   pitch: 1,  dist: 240, look: 0.2, explode: 0 },
  'top-front':     { yaw: -10,  pitch: 34, dist: 270, look: 0, explode: 0 },
};
const KEYS = ['yaw', 'pitch', 'dist', 'look', 'explode'];
const SAFE = 0.84;   // 8% margin on every side of the canvas

const ON = [232, 240, 255];   // lit pixel, like the gifs
const OFF = [10, 12, 16];

const canvas = document.getElementById('clawd3d');
if (canvas) start(canvas);

function start(canvas) {
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (reduce || !hasWebGL()) return fallback(canvas);
  init(canvas).catch(err => {
    console.warn('clawd3d:', err);
    fallback(canvas);
  });
}

function hasWebGL() {
  try {
    const c = document.createElement('canvas');
    return !!(window.WebGLRenderingContext && (c.getContext('webgl2') || c.getContext('webgl')));
  } catch (e) {
    return false;
  }
}

function fallback(canvas) {
  // the desk photo if it is there, else the gif in a red frame
  const img = document.createElement('img');
  img.alt = 'Tiny Clawd, a small red pixel crab with a screen for a face';
  img.className = (canvas.className + ' clawd3d-fallback').trim();
  img.onerror = () => {
    img.onerror = null;
    img.classList.add('gif');
    img.src = new URL(FALLBACK_GIF, document.baseURI).href;
  };
  img.src = new URL(FALLBACK_PHOTO, document.baseURI).href;
  img.id = 'clawd3d';
  canvas.replaceWith(img);
  window.Clawd3D = { play() {}, pose() {}, fallback: true };
}

async function inflate(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(url + ' ' + res.status);
  const stream = res.body.pipeThrough(new DecompressionStream('deflate'));
  return new Response(stream).arrayBuffer();
}

// ---------- model ----------

async function loadModel() {
  const buf = await inflate(MODEL_URL);
  const headLen = new DataView(buf).getUint32(0, true);
  const head = JSON.parse(new TextDecoder().decode(new Uint8Array(buf, 4, headLen)));
  let off = 4 + headLen;
  const parts = {};
  for (const p of head.parts) {
    const pos = new Int16Array(buf.slice(off, off + p.verts * 6));
    off += p.verts * 6;
    const nrm = new Int8Array(buf.slice(off, off + p.verts * 3));
    off += p.verts * 3;
    off += (4 - off % 4) % 4;
    const size = p.wide ? 4 : 2;
    const idx = p.wide ? new Uint32Array(buf.slice(off, off + p.index * 4)) : new Uint16Array(buf.slice(off, off + p.index * 2));
    off += p.index * size;
    off += (4 - off % 4) % 4;

    const xyz = new Float32Array(pos.length);
    for (let i = 0; i < pos.length; i++) xyz[i] = pos[i] / 100;
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(xyz, 3));
    geo.setAttribute('normal', new THREE.BufferAttribute(nrm, 3, true));
    geo.setIndex(new THREE.BufferAttribute(idx, 1));
    geo.computeBoundingSphere();
    parts[p.name] = { geo, color: p.color };
  }
  return { head, parts };
}

function pla(color) {
  return new THREE.MeshStandardMaterial({ color, roughness: 0.62, metalness: 0, envMapIntensity: 0.9 });
}

// ---------- screen ----------

function makeScreen() {
  const oled = document.createElement('canvas');
  oled.width = 128;
  oled.height = 64;
  const ctx = oled.getContext('2d');
  const img = ctx.createImageData(128, 64);
  const px = new Uint32Array(img.data.buffer);
  const le = new Uint8Array(new Uint32Array([1]).buffer)[0] === 1;
  const pack = c => le ? (255 << 24 | c[2] << 16 | c[1] << 8 | c[0]) >>> 0 : (c[0] << 24 | c[1] << 16 | c[2] << 8 | 255) >>> 0;
  const on = pack(ON), off = pack(OFF);

  // a soft blurred copy, drawn on top with additive blending, is the glow
  const glow = document.createElement('canvas');
  glow.width = 96;
  glow.height = 56;
  const gctx = glow.getContext('2d');

  const tex = new THREE.CanvasTexture(oled);
  tex.magFilter = THREE.NearestFilter;
  tex.minFilter = THREE.LinearFilter;
  tex.generateMipmaps = false;
  tex.colorSpace = THREE.SRGBColorSpace;
  const glowTex = new THREE.CanvasTexture(glow);
  glowTex.colorSpace = THREE.SRGBColorSpace;

  function draw(frames, n) {
    const base = 2 + frames.count * 2 + n * 1024;
    const b = frames.bytes;
    for (let y = 0; y < 64; y++) {
      const row = base + (y >> 3) * 128, bit = y & 7, o = y * 128;
      for (let x = 0; x < 128; x++) px[o + x] = (b[row + x] >> bit) & 1 ? on : off;
    }
    ctx.putImageData(img, 0, 0);
    tex.needsUpdate = true;
    gctx.clearRect(0, 0, 96, 56);
    gctx.filter = 'blur(3px)';
    gctx.drawImage(oled, 16, 12, 64, 32);
    gctx.filter = 'none';
    glowTex.needsUpdate = true;
  }

  function clear() {
    px.fill(off);
    ctx.putImageData(img, 0, 0);
    tex.needsUpdate = true;
  }
  clear();
  return { tex, glowTex, draw };
}

const frameCache = new Map();

function loadFrames(name) {
  if (!frameCache.has(name)) {
    const p = inflate(new URL(name + '.bin', FRAMES_URL)).then(buf => {
      const bytes = new Uint8Array(buf);
      const count = bytes[0] | bytes[1] << 8;
      const ends = new Float64Array(count);
      let ms = 0;
      for (let i = 0; i < count; i++) {
        ms += (bytes[2 + i * 2] | bytes[3 + i * 2] << 8) * 20;
        ends[i] = ms;
      }
      return { bytes, count, ends, ms };
    });
    p.catch(() => frameCache.delete(name));
    frameCache.set(name, p);
  }
  return frameCache.get(name);
}

function frameAt(frames, t) {
  const e = frames.ends;
  let lo = 0, hi = frames.count - 1;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (e[mid] > t) hi = mid; else lo = mid + 1;
  }
  return lo;
}

// ---------- scene ----------

async function init(canvas) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: 'high-performance' });
  renderer.setClearColor(0x000000, 0);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.NeutralToneMapping;
  renderer.toneMappingExposure = 1.0;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  const pmrem = new THREE.PMREMGenerator(renderer);
  scene.environment = pmrem.fromScene(new THREE.RoomEnvironment(), 0.04).texture;
  scene.environmentIntensity = 0.55;
  pmrem.dispose();

  const camera = new THREE.PerspectiveCamera(28, 1, 5, 2000);

  const key = new THREE.DirectionalLight(0xfff4ea, 2.6);
  key.position.set(-50, 240, 120);   // high, so the desk shadow stays short and inside the canvas
  key.castShadow = true;
  key.shadow.mapSize.set(1024, 1024);
  key.shadow.camera.left = -90;
  key.shadow.camera.right = 90;
  key.shadow.camera.top = 90;
  key.shadow.camera.bottom = -90;
  key.shadow.camera.near = 20;
  key.shadow.camera.far = 500;
  key.shadow.radius = 6;
  key.shadow.bias = -0.0008;
  key.shadow.normalBias = 0.3;
  scene.add(key);

  const rim = new THREE.DirectionalLight(0xbfd4ff, 1.1);   // half, the page is light
  rim.position.set(90, 70, -140);
  scene.add(rim);
  const fill = new THREE.DirectionalLight(0xffffff, 0.5);
  fill.position.set(120, 20, 80);
  scene.add(fill);

  const floor = new THREE.Mesh(new THREE.PlaneGeometry(600, 600), new THREE.ShadowMaterial({ opacity: 0.18 }));
  floor.rotation.x = -Math.PI / 2;
  floor.receiveShadow = true;
  scene.add(floor);

  // soft contact shadow right under the feet
  const blob = new THREE.Mesh(new THREE.PlaneGeometry(110, 66), new THREE.MeshBasicMaterial({
    map: radialTexture(), transparent: true, depthWrite: false, opacity: 0.35, toneMapped: false,
  }));
  blob.rotation.x = -Math.PI / 2;
  blob.position.y = 0.05;
  scene.add(blob);

  const { head, parts } = await loadModel();
  const model = new THREE.Group();     // turns with yaw
  const lift = new THREE.Group();      // small float, lives inside model
  model.add(lift);
  scene.add(model);

  const mesh = name => {
    const m = new THREE.Mesh(parts[name].geo, pla(parts[name].color));
    m.castShadow = true;
    m.receiveShadow = true;
    lift.add(m);
    return m;
  };
  mesh('body');
  const inlay = new THREE.Group();
  lift.add(inlay);
  inlay.add(mesh('black'), mesh('white'));
  const plate = mesh('plate');

  const screen = makeScreen();
  const [sx, sy, sz] = head.screen.pos;
  const panel = new THREE.Mesh(new THREE.BoxGeometry(head.screen.w + 2, head.screen.h + 2, 0.6),
    new THREE.MeshStandardMaterial({ color: 0x050607, roughness: 0.3 }));
  panel.position.set(sx, sy, sz - 0.45);
  lift.add(panel);
  const face = new THREE.Mesh(new THREE.PlaneGeometry(head.screen.w, head.screen.h),
    new THREE.MeshBasicMaterial({ map: screen.tex, toneMapped: false }));
  face.position.set(sx, sy, sz);
  lift.add(face);
  const glow = new THREE.Mesh(new THREE.PlaneGeometry(head.screen.w * 1.5, head.screen.h * 1.75),
    new THREE.MeshBasicMaterial({
      map: screen.glowTex, transparent: true, blending: THREE.AdditiveBlending,
      depthWrite: false, opacity: 0.45, toneMapped: false,
    }));
  glow.position.set(sx, sy, sz + 0.3);
  lift.add(glow);

  const bodyLook = new THREE.Vector3(0, head.size[1] * 0.48, 0);
  const screenLook = new THREE.Vector3(sx, sy, sz);

  // ---------- sections and scroll ----------

  const sections = [...document.querySelectorAll('[data-clawd-anim]')].map(el => ({
    el,
    anims: el.dataset.clawdAnim.split(',').map(s => s.trim()).filter(Boolean),
    pose: POSES[el.dataset.clawdPose] ? el.dataset.clawdPose : 'front',
    mid: 0,
  }));

  function measure() {
    const y = scrollY;
    for (const s of sections) {
      const r = s.el.getBoundingClientRect();
      s.top = r.top + y;
      s.bottom = r.bottom + y;
      s.mid = (s.top + s.bottom) / 2;
    }
  }

  const poseOf = name => ({ ...POSES[name] });
  const mix = (a, b, t) => {
    const o = {};
    for (const k of KEYS) o[k] = a[k] + (b[k] - a[k]) * t;
    return o;
  };
  const ease = t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;   // cubic in-out

  let forcedPose = null;      // from Clawd3D.pose(), cleared when the section changes

  function scrollTarget() {
    if (!sections.length) return poseOf(forcedPose || 'three-quarter');
    const c = scrollY + innerHeight / 2;
    let a = sections[0], b = sections[0], t = 0;
    if (c >= sections[sections.length - 1].mid) {
      a = b = sections[sections.length - 1];
    } else if (c > sections[0].mid) {
      let i = 0;
      while (sections[i + 1].mid <= c) i++;
      a = sections[i];
      b = sections[i + 1];
      // hold the pose for the middle half of each section, move in between
      const raw = (c - a.mid) / Math.max(1, b.mid - a.mid);
      t = ease(Math.min(1, Math.max(0, (raw - 0.25) / 0.5)));
    }
    if (forcedPose) return poseOf(forcedPose);
    return mix(poseOf(a.pose), poseOf(b.pose), t);
  }

  function activeSection() {
    const c = scrollY + innerHeight / 2;
    let best = null, bestD = Infinity;
    for (const s of sections) {
      if (c >= s.top && c < s.bottom) return s;
      const d = Math.min(Math.abs(c - s.top), Math.abs(c - s.bottom));
      if (d < bestD) { bestD = d; best = s; }
    }
    return best;
  }

  // ---------- screen playback ----------

  let playlist = ['idle'], listPos = 0, current = null, playStart = 0, lastFrame = -1, active = null;

  async function playList(list) {
    playlist = list.length ? list : ['idle'];
    listPos = 0;
    await startAnim(playlist[0]);
  }

  async function startAnim(name) {
    const want = name;
    current = { name, frames: null };
    const token = current;
    try {
      token.frames = await loadFrames(want);
    } catch (e) {
      console.warn('clawd3d: no frames for', want);
      return;
    }
    if (token !== current) return;
    playStart = performance.now();
    lastFrame = -1;
    // warm up the next one in the list
    if (playlist.length > 1) loadFrames(playlist[(listPos + 1) % playlist.length]).catch(() => {});
  }

  function tickScreen(now) {
    if (!current || !current.frames) return;
    const f = current.frames;
    let t = now - playStart;
    if (t >= f.ms) {
      if (playlist.length > 1) {
        listPos = (listPos + 1) % playlist.length;
        startAnim(playlist[listPos]);
        return;
      }
      playStart = now - (t % f.ms);
      t = now - playStart;
    }
    const n = frameAt(f, t);
    if (n !== lastFrame) {
      lastFrame = n;
      screen.draw(f, n);
    }
  }

  function updateActive() {
    const s = activeSection();
    if (!s || s === active) return;
    active = s;
    forcedPose = null;
    playList(s.anims);
    // fetch the neighbours too so the next section starts at once
    const i = sections.indexOf(s);
    for (const n of [sections[i - 1], sections[i + 1]]) if (n) loadFrames(n.anims[0]).catch(() => {});
  }

  // ---------- size ----------

  let width = 0, height = 0, dprCap = Math.min(devicePixelRatio || 1, 2);

  function resize() {
    const w = canvas.clientWidth || 300, h = canvas.clientHeight || 150;
    if (w === width && h === height) return;
    width = w;
    height = h;
    renderer.setPixelRatio(dprCap);
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }

  // ---------- motion state ----------

  const state = scrollTarget();
  state.yaw -= 40;              // little intro turn
  state.dist *= 1.25;
  let intro = 0;
  let pointerX = 0, pointerY = 0, px = 0, py = 0;

  // every corner of every part, to keep the whole model in its safe box
  const parts3d = [];
  lift.traverse(o => { if (o.isMesh) { o.geometry.computeBoundingBox(); parts3d.push(o); } });
  const corner = new THREE.Vector3();

  // how far the model spills out of the safe box: 1 = just fits
  function spill() {
    lift.updateMatrixWorld(true);
    camera.updateMatrixWorld();
    let s = 0;
    for (const m of parts3d) {
      const b = m.geometry.boundingBox;
      for (let i = 0; i < 8; i++) {
        corner.set(i & 1 ? b.max.x : b.min.x, i & 2 ? b.max.y : b.min.y, i & 4 ? b.max.z : b.min.z);
        corner.applyMatrix4(m.matrixWorld).project(camera);
        s = Math.max(s, Math.abs(corner.x) / SAFE, Math.abs(corner.y) / SAFE);
      }
    }
    return s;
  }

  function applyState(now) {
    const sway = Math.sin(now / 2600) * 2.5;
    model.rotation.y = THREE.MathUtils.degToRad(state.yaw + sway + px * 2);
    lift.position.y = (Math.sin(now / 1700) * 0.6 + 0.6) * (1 - state.explode);

    const e = state.explode;
    plate.position.z = -40 * e;
    inlay.position.z = 14 * e;
    face.position.z = sz + 22 * e;
    glow.position.z = sz + 0.3 + 22 * e;
    panel.position.z = sz - 0.45 + 22 * e;
    lift.position.y += 6 * e;
    blob.scale.setScalar(1 - 0.3 * e);

    const look = bodyLook.clone().lerp(screenLook, state.look);
    const pitch = THREE.MathUtils.degToRad(state.pitch + py);
    const dir = new THREE.Vector3(0, Math.sin(pitch), Math.cos(pitch));
    // the close up may fill the safe box, the other poses leave some air
    const room = 0.85 + 0.15 * state.look;
    let dist = state.dist;
    for (let i = 0; i < 4; i++) {
      camera.position.copy(look).addScaledVector(dir, dist);
      camera.lookAt(look);
      const s = spill() / room;
      if (s <= 1.001) break;
      dist *= s;
    }
  }

  // ---------- loop ----------

  let visible = true, running = false, last = 0, slow = 0;

  function frame(now) {
    if (!visible || document.hidden) {
      running = false;
      return;
    }
    requestAnimationFrame(frame);
    const dt = Math.min(0.1, last ? (now - last) / 1000 : 0.016);
    last = now;

    // drop the pixel ratio if the device struggles
    slow = dt > 0.026 ? slow + 1 : Math.max(0, slow - 1);
    if (slow > 45 && dprCap > 1) {
      dprCap = Math.max(1, dprCap - 0.5);
      width = 0;
      slow = 0;
    }
    resize();

    intro = Math.min(1, intro + dt * 0.8);
    const target = scrollTarget();
    const k = 1 - Math.exp(-dt * (intro < 1 ? 2.5 : 5.5));
    for (const key of KEYS) state[key] += (target[key] - state[key]) * k;
    px += (pointerX - px) * (1 - Math.exp(-dt * 3));
    py += (pointerY - py) * (1 - Math.exp(-dt * 3));

    applyState(now);
    tickScreen(now);
    renderer.render(scene, camera);
  }

  function run() {
    if (running || !visible) return;
    running = true;
    last = 0;
    requestAnimationFrame(frame);
  }

  new IntersectionObserver(entries => {
    visible = entries[entries.length - 1].isIntersecting;
    run();
  }).observe(canvas);
  document.addEventListener('visibilitychange', run);

  let measureQueued = false;
  const queueMeasure = () => {
    if (measureQueued) return;
    measureQueued = true;
    requestAnimationFrame(() => {
      measureQueued = false;
      measure();
      updateActive();
    });
  };
  addEventListener('resize', queueMeasure);
  new ResizeObserver(queueMeasure).observe(document.body);
  addEventListener('scroll', () => updateActive(), { passive: true });
  addEventListener('pointermove', e => {
    if (e.pointerType !== 'mouse') return;
    pointerX = e.clientX / innerWidth * 2 - 1;
    pointerY = e.clientY / innerHeight * 2 - 1;
  }, { passive: true });

  window.Clawd3D = {
    play(name) {
      const list = Array.isArray(name) ? name : String(name).split(',').map(s => s.trim());
      return playList(list.filter(Boolean));
    },
    pose(name) {
      forcedPose = POSES[name] ? name : null;
    },
  };

  measure();
  updateActive();
  if (!active) playList(['idle']);
  canvas.classList.add('is-ready');
  document.documentElement.classList.add('clawd3d-ready');
  window.dispatchEvent(new Event('clawd3d:ready'));
  run();
}

function radialTexture() {
  const c = document.createElement('canvas');
  c.width = c.height = 128;
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  grad.addColorStop(0, 'rgba(0,0,0,0.75)');
  grad.addColorStop(0.45, 'rgba(0,0,0,0.3)');
  grad.addColorStop(1, 'rgba(0,0,0,0)');
  g.fillStyle = grad;
  g.fillRect(0, 0, 128, 128);
  return new THREE.CanvasTexture(c);
}
