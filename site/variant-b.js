// variant b: the big screen, the made-up session and the hook keys.
// The 3D Clawd (js/clawd3d.js) and the big screen always play the same animation.

const FRAMES_URL = new URL('assets/frames/', document.baseURI);
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
const wait = ms => new Promise(r => setTimeout(r, ms));

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

// ---------- the big screen ----------

const LIT = '#e8f0ff';
const OFF = '#0a0a0a';
const DARK = '#111214';   // unlit pixels, just visible, like the real panel

function makeOled(canvas, scaleLabel) {
  const ctx = canvas.getContext('2d');
  let scale = 0, list = ['idle'], pos = 0, cur = null, start = 0, last = -1;

  function fit() {
    const box = canvas.parentElement.clientWidth - 2 * parseFloat(getComputedStyle(canvas.parentElement).paddingLeft);
    const s = Math.max(1, Math.min(5, Math.floor(box / 128)));
    if (s === scale) return;
    scale = s;
    canvas.width = 128 * s;
    canvas.height = 64 * s;
    canvas.style.width = 128 * s + 'px';
    canvas.style.height = 64 * s + 'px';
    if (scaleLabel) scaleLabel.textContent = s === 1 ? 'real size' : s + 'x';
    last = -1;
    if (cur && cur.frames) draw(cur.frames, frameAt(cur.frames, performance.now() - start));
  }

  function draw(f, n) {
    const s = scale, dot = s >= 3 ? s - 1 : s;
    const base = 2 + f.count * 2 + n * 1024, b = f.bytes;
    ctx.fillStyle = OFF;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    for (const lit of [false, true]) {
      if (!lit && s < 3) continue;
      ctx.fillStyle = lit ? LIT : DARK;
      for (let y = 0; y < 64; y++) {
        const row = base + (y >> 3) * 128, bit = y & 7;
        for (let x = 0; x < 128; x++) {
          if (((b[row + x] >> bit) & 1) === (lit ? 1 : 0)) ctx.fillRect(x * s, y * s, dot, dot);
        }
      }
    }
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
    if (still) draw(token.frames, Math.floor(token.frames.count / 2));
    if (list.length > 1) loadFrames(list[(pos + 1) % list.length]).catch(() => {});
  }

  function tick(now) {
    requestAnimationFrame(tick);
    if (still || !cur || !cur.frames) return;
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
      draw(f, n);
    }
  }

  fit();
  new ResizeObserver(fit).observe(canvas.parentElement);
  requestAnimationFrame(tick);

  return {
    play(names) {
      list = names;
      pos = 0;
      return begin(list[0]);
    },
  };
}

// ---------- one call plays on both Clawds and fills the serial line ----------

const oled = makeOled(document.getElementById('oled'), document.getElementById('oled-scale'));
const out = {
  hook: document.getElementById('r-hook'),
  line: document.getElementById('r-line'),
  anim: document.getElementById('r-anim'),
};
let showing = ['idle'];

function show(anim, hook, line) {
  const names = anim.split(',');
  showing = names;
  oled.play(names);
  if (window.Clawd3D) window.Clawd3D.play(names);
  out.hook.textContent = hook || '-';
  out.line.textContent = line || '-';
  out.anim.textContent = names[names.length - 1];
}

// the 3D model loads after us, catch it up when it is ready
const POSE = new URLSearchParams(location.search).get('pose') || 'close';
addEventListener('clawd3d:ready', () => {
  window.Clawd3D.pose(POSE);
  window.Clawd3D.play(showing);
});

// ---------- the made-up session ----------

const term = document.getElementById('term');
const typed = term.querySelector('.typed');
const spin = document.getElementById('spin');
const promptText = typed.textContent;
const lines = [...term.querySelectorAll('.term-lines li')];
const step = name => term.querySelector(`[data-step="${name}"]`);

const WORDS = ['Percolating', 'Noodling', 'Clauding', 'Pondering', 'Ruminating', 'Simmering'];
const GLYPHS = ['·', '+', '*', '×', '*', '+'];

// each step: which line shows, what the hook says, what plays, how long it stays
const SCRIPT = [
  { line: 'grep', hook: 'PreToolUse', usb: 'ev tool Grep', anim: 'searching', ms: 2800 },
  { line: 'grep-out', ms: 500 },
  { line: 'test1', hook: 'PreToolUse', usb: 'ev tool Test', anim: 'test_tubes', ms: 3000 },
  { line: 'test1-out', hook: 'PostToolUse', usb: 'ev test fail', anim: 'yellow_card', ms: 3160 },
  { line: 'edit', hook: 'PreToolUse', usb: 'ev tool Edit', anim: 'typing', ms: 2800 },
  { line: 'edit-out', ms: 500 },
  { line: 'test2', hook: 'PreToolUse', usb: 'ev tool Test', anim: 'test_tubes', ms: 2600 },
  { line: 'test2-out', hook: 'PostToolUse', usb: 'ev test pass', anim: 'bug_squash', ms: 2900 },
  { line: 'commit', hook: 'PreToolUse', usb: 'ev tool Git', anim: 'git_graph', ms: 3000 },
  { line: 'commit-out', hook: 'PostToolUse', usb: 'ev commit', anim: 'party', ms: 3300 },
];

let runId = 0;          // bumping it stops a running session
let spinTimer = 0;

function spinner(on, t0) {
  clearInterval(spinTimer);
  if (!on) {
    spin.textContent = '';
    return;
  }
  let i = 0;
  const draw = () => {
    const word = WORDS[Math.floor(i / 6) % WORDS.length];
    const secs = Math.floor((performance.now() - t0) / 1000);
    spin.innerHTML = `${GLYPHS[i % GLYPHS.length]} ${word}… <em>(${secs}s, esc to interrupt)</em>`;
    i++;
  };
  draw();
  spinTimer = setInterval(draw, 300);
}

function reveal(li) {
  li.classList.remove('later');
  if (!li.querySelector('.dot')) return;
  lines.forEach(l => l.classList.remove('now'));
  li.classList.add('now');
}

async function session() {
  const id = ++runId;
  const alive = () => id === runId;
  term.classList.add('run');
  term.classList.remove('waiting');
  lines.forEach(l => { l.classList.add('later'); l.classList.remove('now'); });
  spinner(false);

  // you type
  const prompt = step('prompt');
  prompt.classList.remove('later');
  typed.textContent = '';
  term.classList.add('typing');
  show('blink', 'you type', 'typing 6');
  await wait(900);
  for (let i = 1; i <= promptText.length; i++) {
    if (!alive()) return;
    typed.textContent = promptText.slice(0, i);
    await wait(55 + Math.random() * 70);
  }
  await wait(500);
  if (!alive()) return;

  // enter
  term.classList.remove('typing');
  const t0 = performance.now();
  spin.classList.remove('later');
  spinner(true, t0);
  show('thinking_words', 'UserPromptSubmit', 'ev prompt');
  await wait(3600);

  for (const s of SCRIPT) {
    if (!alive()) return;
    reveal(step(s.line));
    if (s.anim) show(s.anim, s.hook, s.usb);
    await wait(s.ms);
  }
  if (!alive()) return;

  // done: how long the turn took picks the party
  const secs = Math.round((performance.now() - t0) / 1000);
  spinner(false);
  reveal(step('answer'));
  lines.forEach(l => l.classList.remove('now'));
  show('goal', `Stop, ${secs} s turn`, 'ev done');
  term.classList.add('waiting');
  await wait(4600);
  if (!alive()) return;
  show('coffee', 'nothing running', '-');
  await wait(6000);
  if (alive()) session();
}

function stopSession() {
  runId++;
  spinner(false);
}

// the finished session, for reduced motion and before anything runs
function finished() {
  lines.forEach(l => l.classList.remove('later', 'now'));
  typed.textContent = promptText;
  spin.textContent = '';
  term.classList.add('waiting');
}

// ---------- the keys ----------

const keys = [...document.querySelectorAll('.keys button')];
let picked = null, touched = 0, demoTimer = 0, demoAt = 0;

function press(k, fromUser) {
  if (picked) picked.classList.remove('on');
  picked = k;
  k.classList.add('on');
  show(k.dataset.anim, k.dataset.hook, k.dataset.line);
  if (fromUser) touched = performance.now();
}

keys.forEach(k => {
  k.addEventListener('pointerenter', e => { if (e.pointerType === 'mouse') press(k, true); });
  k.addEventListener('click', () => press(k, true));
  k.addEventListener('focus', () => press(k, true));
});

// nobody touching the keys: walk through them slowly
function demo() {
  clearInterval(demoTimer);
  demoTimer = setInterval(() => {
    if (performance.now() - touched < 9000) return;
    demoAt = (picked ? keys.indexOf(picked) + 1 : 0) % keys.length;
    press(keys[demoAt], false);
  }, 3400);
}

// ---------- which part is driving the screen ----------

const hooks = document.getElementById('hooks');
let mode = '';

function pickMode() {
  const r = hooks.getBoundingClientRect();
  const want = r.top < innerHeight * 0.55 ? 'keys' : 'session';
  if (want === mode) return;
  mode = want;
  if (mode === 'keys') {
    stopSession();
    finished();
    if (!still) demo();
    if (!picked) press(keys[0], false);
    else press(picked, false);
  } else {
    clearInterval(demoTimer);
    if (picked) picked.classList.remove('on');
    picked = null;
    if (still) {
      finished();
      show('goal', 'Stop', 'ev done');
    } else {
      session();
    }
  }
}

addEventListener('scroll', pickMode, { passive: true });
addEventListener('resize', pickMode);
pickMode();

// ---------- the rest of the page ----------

document.querySelectorAll('.copy-btn').forEach(b => {
  b.addEventListener('click', () => {
    navigator.clipboard.writeText(document.getElementById(b.dataset.copy).textContent).then(() => {
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
  if (!el) return;
  const r = el.getBoundingClientRect();
  const top = r.height > innerHeight ? r.top - 88 : r.top + r.height / 2 - innerHeight / 2;
  scrollTo({ top: scrollY + top, behavior: 'instant' });
});
