// variant a: the hero turntable and the big OLED.
// clawd3d.js is untouched. we only hook Scene.onBeforeRender, which three.js calls
// once per frame right before it draws, to add a slow turn.

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

// ---------- turntable ----------

const TURN = 360 / 40;   // degrees a second, one turn in 40 s
const fixed = new URLSearchParams(location.search).get('turn');   // ?turn=30 holds one angle, for screenshots
let turn = fixed ? +fixed : 0, lastT = 0;

THREE.Scene.prototype.onBeforeRender = function (renderer, scene, camera) {
  const model = scene.children.find(o => o.isGroup && o.children[0] && o.children[0].isGroup);
  if (!model) return;

  const now = performance.now() / 1000;
  const dt = lastT ? Math.min(0.1, now - lastT) : 0;
  lastT = now;
  if (atHero() && fixed) turn = +fixed;
  else if (atHero() && !still) turn += TURN * dt;
  else turn += (Math.round(turn / 360) * 360 - turn) * (1 - Math.exp(-dt * 3));
  model.rotation.y += THREE.MathUtils.degToRad(turn);
  model.updateMatrixWorld(true);
};

// ---------- the big OLED: real frame data, one square per pixel ----------

const matrix = document.getElementById('matrix');
if (matrix) {
  const ctx = matrix.getContext('2d');
  const cache = new Map();
  const nameEl = document.getElementById('mx-name');
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
    nameEl.textContent = name.replace(/_/g, ' ');
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
