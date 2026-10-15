// the dot matrix page: real firmware frames drawn as round dots,
// one dot per OLED pixel. the 3D Clawd (js/clawd3d.js) follows the same sections.

const FRAMES_URL = new URL('assets/frames/', document.baseURI);
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

// ---------- frames, same format as clawd3d.js ----------

const cache = new Map();

function loadFrames(name) {
  if (!cache.has(name)) {
    const p = fetch(new URL(name + '.bin', FRAMES_URL)).then(res => {
      if (!res.ok) throw new Error(name + ' ' + res.status);
      return new Response(res.body.pipeThrough(new DecompressionStream('deflate'))).arrayBuffer();
    }).then(buf => {
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
    p.catch(() => cache.delete(name));
    cache.set(name, p);
  }
  return cache.get(name);
}

function frameAt(f, t) {
  let lo = 0, hi = f.count - 1;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (f.ends[mid] > t) hi = mid; else lo = mid + 1;
  }
  return lo;
}

// ---------- a screen made of dots ----------

const LIT = '#f4f4f4';
const UNLIT = 'rgba(255,255,255,0.075)';
const players = [];

function dotGrid(w, h, pitch, color) {
  const c = document.createElement('canvas');
  c.width = w;
  c.height = h;
  const g = c.getContext('2d');
  g.fillStyle = color;
  g.beginPath();
  const r = pitch * 0.4;
  for (let y = 0; y < 64; y++) {
    for (let x = 0; x < 128; x++) {
      const cx = (x + 0.5) * pitch, cy = (y + 0.5) * pitch;
      g.moveTo(cx + r, cy);
      g.arc(cx, cy, r, 0, Math.PI * 2);
    }
  }
  g.fill();
  return c;
}

function makeDots(canvas, onAnim) {
  const ctx = canvas.getContext('2d');
  const small = document.createElement('canvas');
  small.width = 128;
  small.height = 64;
  const sctx = small.getContext('2d');
  const img = sctx.createImageData(128, 64);
  const px = new Uint32Array(img.data.buffer);
  const tmp = document.createElement('canvas');
  const tctx = tmp.getContext('2d');
  let lit = null, unlit = null, pitch = 1;
  let list = ['idle'], pos = 0, cur = null, start = 0, last = -1, shown = null;
  let seen = true;

  function fit() {
    const w = Math.round(canvas.clientWidth * Math.min(devicePixelRatio || 1, 2));
    if (!w || w === canvas.width && lit) return;
    canvas.width = tmp.width = w;
    canvas.height = tmp.height = Math.round(w / 2);
    pitch = w / 128;
    lit = dotGrid(w, canvas.height, pitch, LIT);
    unlit = dotGrid(w, canvas.height, pitch, UNLIT);
    last = -1;
    if (shown) paint(shown.f, shown.n);
    else paint(null, 0);
  }

  function paint(f, n) {
    shown = f ? { f, n } : null;
    const W = canvas.width, H = canvas.height;
    ctx.clearRect(0, 0, W, H);
    if (unlit) ctx.drawImage(unlit, 0, 0);
    if (!f || !lit) return;
    const base = 2 + f.count * 2 + n * 1024, b = f.bytes;
    for (let y = 0; y < 64; y++) {
      const row = base + (y >> 3) * 128, bit = y & 7, o = y * 128;
      for (let x = 0; x < 128; x++) px[o + x] = (b[row + x] >> bit) & 1 ? 0xffffffff : 0;
    }
    sctx.putImageData(img, 0, 0);
    tctx.globalCompositeOperation = 'copy';
    tctx.imageSmoothingEnabled = false;
    tctx.drawImage(small, 0, 0, W, H);
    tctx.globalCompositeOperation = 'destination-in';
    tctx.drawImage(lit, 0, 0);
    ctx.drawImage(tmp, 0, 0);
  }

  async function begin(name) {
    const token = cur = { name, frames: null };
    try {
      token.frames = await loadFrames(name);
    } catch (e) {
      return;
    }
    if (token !== cur) return;
    start = performance.now();
    last = -1;
    if (onAnim) onAnim(name);
    if (still) paint(token.frames, Math.floor(token.frames.count / 2));
    if (list.length > 1) loadFrames(list[(pos + 1) % list.length]).catch(() => {});
  }

  function tick(now) {
    if (still || !seen || !cur || !cur.frames) return;
    const f = cur.frames;
    let t = now - start;
    if (t >= f.ms) {
      if (list.length > 1) {
        pos = (pos + 1) % list.length;
        begin(list[pos]);
        return;
      }
      start = now - (t % f.ms);
      t = now - start;
    }
    const n = frameAt(f, t);
    if (n !== last) {
      last = n;
      paint(f, n);
    }
  }

  fit();
  new ResizeObserver(fit).observe(canvas);
  new IntersectionObserver(es => { seen = es[es.length - 1].isIntersecting; }).observe(canvas);
  const p = {
    tick,
    play(names) {
      list = names.length ? names : ['idle'];
      pos = 0;
      return begin(list[0]);
    },
    get list() { return list; },
  };
  players.push(p);
  return p;
}

function loop(now) {
  requestAnimationFrame(loop);
  for (const p of players) p.tick(now);
}
requestAnimationFrame(loop);

// ---------- the stage screen follows the story ----------

const nowAnim = document.getElementById('now-anim');
const nowHook = document.getElementById('now-hook');
const stage = makeDots(document.getElementById('dots-stage'), name => {
  nowAnim.textContent = name.replace(/_/g, ' ');
});
const scenes = [...document.querySelectorAll('.scene[data-clawd-anim]')];
let active = null;

function activeScene() {
  const c = innerHeight / 2;
  let best = null, bestD = Infinity;
  for (const s of scenes) {
    const r = s.getBoundingClientRect();
    if (r.top <= c && r.bottom > c) return s;
    const d = Math.min(Math.abs(c - r.top), Math.abs(c - r.bottom));
    if (d < bestD) { bestD = d; best = s; }
  }
  return best;
}

function follow() {
  const s = activeScene();
  if (!s || s === active) return;
  if (active) active.classList.remove('on');
  active = s;
  s.classList.add('on');
  nowHook.textContent = s.dataset.hook || '';
  stage.play(s.dataset.clawdAnim.split(','));
}
addEventListener('scroll', follow, { passive: true });
addEventListener('resize', follow);
follow();

// pet it: both Clawds
document.querySelectorAll('[data-play]').forEach(b => {
  b.addEventListener('click', () => {
    const names = b.dataset.play.split(',');
    stage.play(names);
    if (window.Clawd3D) window.Clawd3D.play(names);
  });
});

// ---------- try it: the hook keys ----------

const tryOut = {
  hook: document.getElementById('t-hook'),
  line: document.getElementById('t-line'),
  anim: document.getElementById('t-anim'),
};
const trier = makeDots(document.getElementById('dots-try'), name => {
  tryOut.anim.textContent = name;
});
const keys = [...document.querySelectorAll('.keys button')];
let picked = null, touched = 0;

function press(k, fromUser) {
  if (picked) picked.classList.remove('on');
  picked = k;
  k.classList.add('on');
  tryOut.hook.textContent = k.dataset.hook;
  tryOut.line.textContent = k.dataset.line;
  trier.play(k.dataset.anim.split(','));
  if (fromUser) touched = performance.now();
}

keys.forEach(k => {
  k.addEventListener('pointerenter', e => { if (e.pointerType === 'mouse') press(k, true); });
  k.addEventListener('click', () => press(k, true));
  k.addEventListener('focus', () => press(k, true));
});
press(keys[1], false);

// nobody touching the keys: walk through them slowly
if (!still) setInterval(() => {
  if (performance.now() - touched < 9000) return;
  const r = document.getElementById('hooks').getBoundingClientRect();
  if (r.bottom < 0 || r.top > innerHeight) return;
  press(keys[(keys.indexOf(picked) + 1) % keys.length], false);
}, 3400);

// ---------- the rest ----------

document.querySelectorAll('.copy-btn').forEach(b => {
  b.addEventListener('click', () => {
    const text = document.getElementById(b.dataset.copy).textContent.replace(/^\$\s*/, '');
    navigator.clipboard.writeText(text).then(() => {
      b.textContent = 'copied';
      setTimeout(() => { b.textContent = 'copy'; }, 1500);
    });
  });
});

if (still) document.querySelectorAll('img[src$=".gif"]').forEach(g => {
  g.src = g.getAttribute('src').replace('assets/', 'assets/still/').replace('.gif', '.png');
});

// test only: ?shot=id puts that section in the middle of the screen
const shot = new URLSearchParams(location.search).get('shot');
if (shot) addEventListener('load', () => {
  const el = document.getElementById(shot);
  if (!el) return scrollTo(0, 0);
  const r = el.getBoundingClientRect();
  // on phones the stage sits on top, so show the start of the section under it
  const under = innerWidth < 900 && el.classList.contains('scene') ? document.getElementById('stage').offsetHeight + 56 : 0;
  const top = under ? r.top - under + 8 : r.height > innerHeight ? r.top - 56 : r.top + r.height / 2 - innerHeight / 2;
  scrollTo({ top: scrollY + top, behavior: 'instant' });
  follow();
});
