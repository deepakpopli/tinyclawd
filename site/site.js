// the hero turntable and the big OLED.
// we only hook Scene.onBeforeRender, which three.js calls
// once per frame right before it draws, to add a slow turn.

// the YouTube id of the film, like 'dQw4w9WgXcQ'. the film band stays hidden while this is empty
const VIDEO_ID = 'aFLVqkyy6DE';

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

// three.js is only fetched when the 3D model will run, same check as js/clawd3d.js
if (!still && window.WebGLRenderingContext) import('./vendor/three.js').then(THREE => {
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
}).catch(() => {});

// ---------- frame data, recorded from the firmware sim (tools/export_frames.py) ----------

const cache = new Map();
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
const frameAt = (f, t) => {
  let k = 0;
  while (k < f.count - 1 && f.ends[k] <= t) k++;
  return k;
};

// ---------- the big OLED: real frame data, one square per pixel ----------

const matrix = document.getElementById('matrix');
if (matrix) {
  const ctx = matrix.getContext('2d');
  const nameEl = document.getElementById('mx-name');
  let anim = null, start = 0, last = -1, visible = false, scale = 8;

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
    const k = frameAt(anim, (now - start) % anim.ms);
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

// ---------- the small screens: each OLED pixel is one dot with a gap, like on the real panel ----------

const minis = [];
let miniRunning = false, seen = null;

function miniLoop(now) {
  const live = minis.filter(m => m.visible && m.f);
  if (!live.length) { miniRunning = false; return; }
  requestAnimationFrame(miniLoop);
  for (const m of live) {
    const k = frameAt(m.f, (now - m.start) % m.f.ms);
    if (k !== m.last) { m.last = k; drawMini(m, k); }
  }
}

function drawMini(m, k) {
  const b = m.f.bytes, base = 2 + m.f.count * 2 + k * 1024, px = m.px;
  for (let y = 0; y < 64; y++) {
    const row = base + (y >> 3) * 128, bit = y & 7, o = y * 512;
    for (let x = 0; x < 128; x++) px[o + x * 2] = (b[row + x] >> bit) & 1 ? m.on : m.off;
  }
  m.ctx.putImageData(m.img, 0, 0);
}

function startMini(m) {
  load(m.name).then(f => {
    const c = document.createElement('canvas');
    c.width = 256;
    c.height = 128;
    c.setAttribute('aria-hidden', 'true');
    m.ctx = c.getContext('2d');
    m.img = m.ctx.createImageData(256, 128);
    m.px = new Uint32Array(m.img.data.buffer);
    const le = new Uint8Array(new Uint32Array([1]).buffer)[0] === 1;
    const pack = (r, g, b) => (le ? (255 << 24 | b << 16 | g << 8 | r) : (r << 24 | g << 16 | b << 8 | 255)) >>> 0;
    m.on = pack(232, 240, 255);
    m.off = pack(10, 12, 16);
    m.px.fill(m.off);
    m.f = f;
    m.start = performance.now();
    m.last = -1;
    drawMini(m, 0);
    seen.unobserve(m.el);
    m.el.replaceWith(c);
    m.el = c;
    seen.observe(c);
    if (!miniRunning) { miniRunning = true; requestAnimationFrame(miniLoop); }
  }).catch(() => {});   // the still frame stays
}

if (!still) {
  seen = new IntersectionObserver(es => {
    for (const e of es) {
      const m = minis.find(m => m.el === e.target);
      if (!m) continue;
      m.visible = e.isIntersecting;
      if (m.visible && !m.asked) { m.asked = true; startMini(m); }
      if (m.visible && m.f && !miniRunning) { miniRunning = true; requestAnimationFrame(miniLoop); }
    }
  }, { rootMargin: '200px 0px' });
  document.querySelectorAll('img[data-frames]').forEach(img => {
    const m = { name: img.dataset.frames, el: img, visible: false };
    minis.push(m);
    seen.observe(img);
  });
}

// ---------- the film: a still until you press play, then the YouTube player ----------

const filmId = VIDEO_ID || new URLSearchParams(location.search).get('video');   // ?video=id, test only
const film = document.getElementById('film');
if (film && filmId) {
  film.hidden = false;
  const btn = film.querySelector('.film-play');
  btn.addEventListener('click', () => {
    const f = document.createElement('iframe');
    f.src = 'https://www.youtube-nocookie.com/embed/' + encodeURIComponent(filmId) + '?autoplay=1&rel=0&playsinline=1';
    f.title = 'tiny clawd, the film';
    f.allow = 'autoplay; encrypted-media; picture-in-picture; fullscreen';
    f.allowFullscreen = true;
    btn.replaceWith(f);
    f.focus();
  });
}

// ---------- small things ----------

document.querySelectorAll('[data-play]').forEach(b => b.addEventListener('click', () => {
  if (window.Clawd3D) window.Clawd3D.play(b.dataset.play);
}));

document.querySelectorAll('.copy-btn').forEach(b => b.addEventListener('click', () => {
  const el = document.getElementById(b.dataset.copy);
  const done = text => {
    b.textContent = text;
    setTimeout(() => { b.textContent = 'copy'; }, 1500);
  };
  // no clipboard (plain http, old browser): select the text so it can be copied by hand
  const select = () => {
    getSelection().selectAllChildren(el);
    done('selected');
  };
  if (navigator.clipboard) navigator.clipboard.writeText(el.textContent).then(() => done('copied'), select);
  else select();
}));

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
