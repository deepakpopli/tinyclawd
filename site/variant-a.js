// variant a: the hero turntable, the part callouts, the status line and the big OLED.
// clawd3d.js is untouched. we only hook Scene.onBeforeRender, which three.js calls
// once per frame right before it draws, to add a slow turn and to find the parts on screen.

import * as THREE from './vendor/three.js';

const root = document.documentElement;
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

// ---------- hero or not ----------

function heroCheck() {
  root.classList.toggle('at-hero', scrollY < innerHeight * 0.35);
}
addEventListener('scroll', heroCheck, { passive: true });
addEventListener('resize', heroCheck);
heroCheck();
const atHero = () => root.classList.contains('at-hero');

// ---------- turntable and callouts ----------

const TURN = 360 / 40;   // degrees a second, one turn in 40 s
const fixed = new URLSearchParams(location.search).get('turn');   // ?turn=30 holds one angle, for screenshots
let turn = fixed ? +fixed : 0, lastT = 0;

const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
svg.setAttribute('class', 'leaders');
svg.setAttribute('aria-hidden', 'true');
document.body.appendChild(svg);

const callouts = [...document.querySelectorAll('.callouts li')].map(li => {
  const line = document.createElementNS(svg.namespaceURI, 'polyline');
  const dot = document.createElementNS(svg.namespaceURI, 'rect');
  dot.setAttribute('width', 7);
  dot.setAttribute('height', 7);
  svg.append(line, dot);
  return { li, part: li.dataset.part, line, dot, right: li.matches('.c-brain, .c-usb') };
});

let parts = null;
const v = new THREE.Vector3(), n = new THREE.Vector3(), cam = new THREE.Vector3();

// the anchor points, in the model's own space, from the real meshes
function findParts(lift) {
  const meshes = lift.children;
  const body = meshes[0], plate = meshes[2], face = meshes[4];
  body.geometry.computeBoundingBox();
  plate.geometry.computeBoundingBox();
  const b = body.geometry.boundingBox, p = plate.geometry.boundingBox;
  const cx = (b.min.x + b.max.x) / 2, cz = (b.min.z + b.max.z) / 2;
  return {
    pad: { at: new THREE.Vector3(cx, b.max.y, cz - 2), normal: new THREE.Vector3(0, 1, 0) },
    screen: { at: face.position.clone(), normal: new THREE.Vector3(0, 0, 1) },
    brain: { at: new THREE.Vector3(cx + 14, (b.min.y + b.max.y) * 0.5, cz - 8), normal: null },
    usb: { at: new THREE.Vector3(cx, p.min.y + (p.max.y - p.min.y) * 0.22, p.min.z), normal: new THREE.Vector3(0, 0, -1) },
  };
}

THREE.Scene.prototype.onBeforeRender = function (renderer, scene, camera) {
  const model = scene.children.find(o => o.isGroup && o.children[0] && o.children[0].isGroup);
  if (!model) return;
  const lift = model.children[0];
  if (!parts && lift.children.length > 4) parts = findParts(lift);

  const now = performance.now() / 1000;
  const dt = lastT ? Math.min(0.1, now - lastT) : 0;
  lastT = now;
  if (atHero() && fixed) turn = +fixed;
  else if (atHero() && !still) turn += TURN * dt;
  else turn += (Math.round(turn / 360) * 360 - turn) * (1 - Math.exp(-dt * 3));
  model.rotation.y += THREE.MathUtils.degToRad(turn);
  model.updateMatrixWorld(true);

  if (atHero() && parts) drawLeaders(lift, camera, renderer.domElement);
};

function drawLeaders(lift, camera, canvas) {
  const r = canvas.getBoundingClientRect();
  camera.getWorldPosition(cam);
  for (const c of callouts) {
    const p = parts[c.part];
    v.copy(p.at).applyMatrix4(lift.matrixWorld);
    let behind = true;
    if (p.normal) {
      n.copy(p.normal).transformDirection(lift.matrixWorld);
      behind = n.dot(cam.clone().sub(v)) <= 0;
    }
    v.project(camera);
    const x = r.left + (v.x + 1) / 2 * r.width;
    const y = r.top + (1 - v.y) / 2 * r.height;
    const l = c.li.getBoundingClientRect();
    const sx = c.right ? l.left : l.right, sy = l.top + 0.5;
    const ex = sx + (c.right ? -16 : 16);
    c.line.setAttribute('points', `${sx},${sy} ${ex},${sy} ${x},${y}`);
    c.line.classList.toggle('behind', behind);
    c.dot.setAttribute('x', Math.round(x) - 3.5);
    c.dot.setAttribute('y', Math.round(y) - 3.5);
    c.dot.classList.toggle('behind-dot', behind);
    c.li.classList.toggle('hidden-part', behind);
  }
}

// ---------- the status line under the hero: one short session, on a loop ----------

const SCRIPT = [
  [0.0, 'SessionStart', '~/projects/api', 'idle'],
  [2.0, 'UserPromptSubmit', '"fix the flaky login test"', 'thinking_words'],
  [6.5, 'PreToolUse', 'Grep "login"', 'searching'],
  [10.0, 'PreToolUse', 'Bash npm test', 'test_tubes'],
  [14.0, 'PostToolUseFailure', 'Bash npm test', 'yellow_card'],
  [17.0, 'PreToolUse', 'Edit src/auth.ts', 'typing'],
  [20.5, 'Stop', 'took 18.5 s', 'done'],
];
const LOOP = 25;
const status = document.querySelector('.status');
if (status) {
  const el = k => status.querySelector('.st-' + k);
  let step = -1, t0 = performance.now();
  const fmt = s => {
    const m = Math.floor(s / 60), r = s - m * 60;
    return '00:' + String(m).padStart(2, '0') + ':' + r.toFixed(1).padStart(4, '0');
  };
  const tick = () => {
    if (!atHero()) { t0 = performance.now() - (SCRIPT[Math.max(0, step)][0]) * 1000; return; }
    let s = (performance.now() - t0) / 1000;
    if (s >= LOOP) { t0 = performance.now(); s = 0; step = -1; }
    el('time').textContent = fmt(s);
    let i = 0;
    while (i + 1 < SCRIPT.length && SCRIPT[i + 1][0] <= s) i++;
    if (i !== step) {
      step = i;
      const [, ev, arg, anim] = SCRIPT[i];
      el('event').textContent = ev;
      el('arg').textContent = arg;
      el('anim').textContent = '-> ' + anim;
      if (window.Clawd3D && !window.Clawd3D.fallback) window.Clawd3D.play(anim);
    }
  };
  if (!still) setInterval(tick, 100);
  addEventListener('clawd3d:ready', () => { step = -1; });
}

// ---------- the big OLED: real frame data, one square per pixel ----------

const matrix = document.getElementById('matrix');
if (matrix) {
  const ctx = matrix.getContext('2d');
  const cache = new Map();
  const nameEl = document.getElementById('mx-name');
  const frameEl = document.getElementById('mx-frame');
  let anim = null, start = 0, last = -1, visible = false, scale = 8;

  const load = name => {
    if (!cache.has(name)) {
      cache.set(name, fetch('assets/frames/' + name + '.bin')
        .then(r => new Response(r.body.pipeThrough(new DecompressionStream('deflate'))).arrayBuffer())
        .then(buf => {
          const bytes = new Uint8Array(buf);
          const count = bytes[0] | bytes[1] << 8;
          const ends = [];
          let ms = 0;
          for (let i = 0; i < count; i++) { ms += (bytes[2 + i * 2] | bytes[3 + i * 2] << 8) * 20; ends.push(ms); }
          return { name, bytes, count, ends, ms };
        }));
    }
    return cache.get(name);
  };

  const size = () => {
    const w = matrix.clientWidth * Math.min(devicePixelRatio || 1, 2);
    scale = Math.max(2, Math.round(w / 128));
    if (matrix.width !== 128 * scale) {
      matrix.width = 128 * scale;
      matrix.height = 64 * scale;
      last = -1;
    }
  };

  const draw = (f, k) => {
    const s = scale, g = Math.max(1, Math.round(s / 7)), d = s - g;
    const base = 2 + f.count * 2 + k * 1024, b = f.bytes;
    ctx.fillStyle = '#0a0a0a';
    ctx.fillRect(0, 0, matrix.width, matrix.height);
    ctx.fillStyle = '#171717';
    for (let pass = 0; pass < 2; pass++) {
      if (pass) ctx.fillStyle = '#e8f0ff';
      for (let y = 0; y < 64; y++) {
        const row = base + (y >> 3) * 128, bit = y & 7;
        for (let x = 0; x < 128; x++) {
          const on = (b[row + x] >> bit) & 1;
          if (on === pass) ctx.fillRect(x * s, y * s, d, d);
        }
      }
    }
    frameEl.textContent = 'frame ' + String(k + 1).padStart(3, '0') + ' / ' + String(f.count).padStart(3, '0');
  };

  const loop = now => {
    if (!visible || !anim) return;
    requestAnimationFrame(loop);
    let t = (now - start) % anim.ms;
    let k = 0;
    while (k < anim.count - 1 && anim.ends[k] <= t) k++;
    if (k !== last) { last = k; draw(anim, k); }
  };

  const pick = name => load(name).then(f => {
    anim = f;
    start = performance.now();
    last = -1;
    nameEl.textContent = name + ', ' + (f.ms / 1000).toFixed(2) + ' s';
    size();
    if (still || !visible) draw(f, Math.min(f.count - 1, 20));
    else requestAnimationFrame(loop);
  });

  document.querySelectorAll('.picker button').forEach(b => b.addEventListener('click', () => {
    document.querySelectorAll('.picker button').forEach(o => o.classList.toggle('on', o === b));
    visible = !still && visible;
    pick(b.dataset.anim);
  }));

  new IntersectionObserver(es => {
    const was = visible;
    visible = !still && es[es.length - 1].isIntersecting;
    if (visible && !was && anim) requestAnimationFrame(loop);
  }).observe(matrix);
  addEventListener('resize', () => { size(); if (anim) draw(anim, Math.max(0, last)); });
  pick('thinking_words');
}

// ---------- small things ----------

document.querySelectorAll('[data-play]').forEach(b => b.addEventListener('click', () => {
  if (window.Clawd3D) window.Clawd3D.play(b.dataset.play);
}));

document.querySelectorAll('.copy-btn').forEach(b => b.addEventListener('click', () => {
  navigator.clipboard.writeText(document.getElementById(b.dataset.copy).textContent).then(() => {
    b.textContent = 'copied';
    setTimeout(() => { b.textContent = 'copy'; }, 1500);
  });
}));

if (still) document.querySelectorAll('img[src$=".gif"]').forEach(g => {
  g.src = g.getAttribute('src').replace('assets/', 'assets/still/').replace('.gif', '.png');
});

// footer clock, local time, like a status line
const clock = document.getElementById('clock');
if (clock) {
  const t = () => { clock.textContent = new Date().toTimeString().slice(0, 8) + ' local'; };
  t();
  setInterval(t, 1000);
}

// the 2D stand-in only shows if the 3D script never starts
setTimeout(() => {
  const c = document.getElementById('clawd3d');
  const started = root.classList.contains('clawd3d-ready') || (c && c.tagName === 'IMG');
  if (!started) document.getElementById('stage').classList.add('empty');
}, 3000);

// test only: ?shot=id puts the middle of that section in the middle of the screen
const shot = new URLSearchParams(location.search).get('shot');
if (shot) addEventListener('load', () => {
  const r = document.getElementById(shot).getBoundingClientRect();
  scrollTo({ top: shot === 'hero' ? 0 : scrollY + r.top + r.height / 2 - innerHeight / 2, behavior: 'instant' });
  heroCheck();
});
