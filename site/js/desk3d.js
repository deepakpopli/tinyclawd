// The desk in the hero: a laptop running Claude Code, the USB-C cable, Clawd on his stand.
// clawd3d.js loads this when the canvas has data-desk. In the hero the camera starts on the laptop,
// then follows the cable to Clawd as the page scrolls. After the hero the laptop fades out and the
// poses work like on index.html, with Clawd still on his stand.
//
// world: mm, y up, front +z, desk at y = 0, the stand at the origin. the laptop is built here in code,
// the stand comes from assets/3d/desk.bin (tools/export_desk.py).

const DESK_URL = new URL('../assets/3d/desk.bin', import.meta.url);

const STAND_YAW = 15;                // Clawd turned a little toward the laptop
const LAPTOP_AT = [185, 0, -95];     // front centre of the laptop, behind Clawd on the right
const LAPTOP_YAW = -12;
const LID_BACK = 20;                 // lid open 110 deg

let THREE;

export async function setup(c) {
  THREE = c.THREE;
  const { scene, camera, renderer, model, key, floor, blob } = c;
  const deg = THREE.MathUtils.degToRad;

  const { head, parts } = await c.loadModel(DESK_URL);
  const desk = new THREE.Group();
  scene.add(desk);
  scene.userData.desk = true;        // site.js: no hero turntable here

  // ---------- Clawd on his stand ----------

  const stand = new THREE.Group();
  stand.rotation.y = deg(STAND_YAW);
  desk.add(stand);
  const fit = [];                    // the stand counts when the poses fit Clawd in the frame
  for (const p of head.parts) {
    const m = new THREE.Mesh(parts[p.name].geo, new THREE.MeshStandardMaterial({ color: p.color, roughness: 0.55 }));
    m.castShadow = m.receiveShadow = true;
    stand.add(m);
    fit.push(m);
  }
  const perch = new THREE.Group();
  perch.matrixAutoUpdate = false;
  perch.matrix.set(...head.perch);
  stand.add(perch);
  perch.add(model);                  // model keeps its own explode, float and screen, no yaw here
  blob.rotation.z = deg(STAND_YAW);

  // ---------- the laptop ----------

  const fading = [];                 // materials that fade out after the hero
  const mat = (color, rough, metal = 0) => {
    const m = new THREE.MeshStandardMaterial({ color, roughness: rough, metalness: metal });
    fading.push(m);
    return m;
  };
  const alu = mat(0x6a6c70, 0.5, 0.45);
  const laptop = new THREE.Group();
  laptop.position.set(...LAPTOP_AT);
  laptop.rotation.y = deg(LAPTOP_YAW);
  desk.add(laptop);
  const add = (geo, m, parent = laptop) => {
    const o = new THREE.Mesh(geo, m);
    o.castShadow = o.receiveShadow = true;
    parent.add(o);
    return o;
  };

  // 14 inch, MacBook like: 312 x 221 mm base, 9.5 mm thick, lid 5.5 mm
  const W = 312, D = 221, T = 9.5, LH = 215, LT = 5.5;
  add(geo(b => slab(b, W, D, T, 10, 2.2, 0, 0, -D / 2, 6, 4)), alu);
  const kw = 14.5 * 18.6;
  add(geo(b => slab(b, kw + 6, 112, 0.4, 4, 0, 0, T - 0.3, -(D - 12 - 56), 3, 0)), mat(0x1e1f22, 0.7));
  add(geo(b => keyboard(b, -(D - 14), T + 0.02)), mat(0x141517, 0.62));
  add(geo(b => slab(b, 150, 88, 0.3, 7, 0, 0, T - 0.05, -(10 + 44), 4, 0)), mat(0x606267, 0.5, 0.4));
  for (const sx of [-1, 1]) add(geo(b => slab(b, 9, 108, 0.25, 3, 0, sx * (kw / 2 + 11), T - 0.05, -(D - 12 - 56), 3, 0)), mat(0x232427, 0.7));
  for (const i of [0, 1]) add(geo(b => slab(b, 1, 9, 3.2, 1.4, 0, -W / 2 + 0.3, 3.2, -(34 + i * 14), 3, 0)), mat(0x111214, 0.6));
  add(geo(b => slab(b, W - 40, 8.4, 8.4, 4.1, 4, 0, T - 3.6, -(D - 4.5), 3, 3)), mat(0x1b1c1f, 0.55));

  // lid: hinged at the back, local +y up the lid, display on local +z
  const lid = new THREE.Group();
  lid.position.set(0, T + 1, -(D - 3));
  lid.rotation.x = deg(-LID_BACK);
  laptop.add(lid);
  const flat = new THREE.Group();    // slabs are built flat, this stands them up
  flat.rotation.x = Math.PI / 2;
  lid.add(flat);
  add(geo(b => slab(b, W, LH, LT, 10, 1.8, 0, -LT, -LH / 2, 6, 4)), alu, flat);
  add(geo(b => slab(b, W - 2.4, LH - 2.4, 0.5, 9, 0, 0, -0.3, -LH / 2, 6, 0)), mat(0x050506, 0.32), flat);

  const term = terminal(renderer);
  const SW = 302, SH = 188.75, TOP = 7;
  const screenMat = new THREE.MeshBasicMaterial({ map: term.tex, toneMapped: false });
  fading.push(screenMat);
  const display = new THREE.Mesh(new THREE.PlaneGeometry(SW, SH), screenMat);
  display.position.set(0, LH - TOP - SH / 2, 0.4);
  lid.add(display);

  // soft contact shadow under the laptop
  const lapBlob = new THREE.Mesh(new THREE.PlaneGeometry(W * 1.25, D * 1.3), new THREE.MeshBasicMaterial({
    map: c.radialTexture(), transparent: true, depthWrite: false, opacity: 0.32, toneMapped: false,
  }));
  fading.push(lapBlob.material);
  lapBlob.rotation.x = -Math.PI / 2;
  lapBlob.position.set(0, 0.06, -D / 2);
  laptop.add(lapBlob);

  // ---------- the cable, Clawd's back to the laptop's left side ----------

  desk.updateMatrixWorld(true);
  const plugMat = mat(0xc9ccd1, 0.42, 0.8);
  const [px0, py0, pz0] = head.port;
  add(geo(b => slab(b, 12, 15, 6.5, 3, 0.6, px0, py0 - 3.25, pz0, 3, 2)), plugMat, perch);
  add(geo(b => slab(b, 15, 8, 6, 2.8, 0.6, -W / 2 - 7.5, 1.8, -41, 3, 2)), plugMat);
  const V = (x, y, z) => new THREE.Vector3(x, y, z);
  const back = V(...head.back).transformDirection(perch.matrixWorld);
  const start = V(px0, py0, pz0).applyMatrix4(perch.matrixWorld).addScaledVector(back, 7.5);
  const port = laptop.localToWorld(V(-W / 2 - 15, 4.8, -41));
  const side = V(-1, 0, 0).transformDirection(laptop.matrixWorld);
  const R = 1.6;
  // down off the back plate, a lazy curve over the desk, straight into the port
  const drop = start.clone().addScaledVector(back, 26).setY(R);
  const near = port.clone().addScaledVector(side, 24).setY(R);
  const across = near.clone().sub(drop);
  const slack = V(across.z, 0, -across.x).normalize().multiplyScalar(across.length() * 0.14);
  const path = [
    start,
    start.clone().addScaledVector(back, 12).add(V(0, -5, 0)),
    drop,
    drop.clone().lerp(near, 0.4).add(slack),
    drop.clone().lerp(near, 0.75).addScaledVector(slack, 0.4),
    near,
    port,
  ];
  const cablePts = spline(path, 14);
  add(tube(cablePts, R, 10), mat(0xe9e6df, 0.55), desk);

  // ---------- lights and floor for a scene this big ----------

  floor.geometry.dispose();
  floor.geometry = new THREE.PlaneGeometry(2400, 2400);
  const mid = V(110, 0, -60);
  key.target.position.copy(mid);
  scene.add(key.target);
  key.position.copy(mid).add(V(-50, 240, 120).multiplyScalar(1.8));
  key.shadow.mapSize.set(2048, 2048);
  Object.assign(key.shadow.camera, { left: -270, right: 270, top: 230, bottom: -230, near: 50, far: 1100 });
  key.shadow.camera.updateProjectionMatrix();

  // ---------- the hero framing ----------

  // points that must be in the first shot: the lid, the front of the base, Clawd and his stand
  const heroPts = [
    ...[[-W / 2, LH], [W / 2, LH]].map(([x, y]) => lid.localToWorld(V(x, y, 0))),
    ...[[-W / 2, 0], [W / 2, 0]].map(([x, z]) => laptop.localToWorld(V(x, 0, z))),
    ...[[-31, -22], [31, -22], [-31, 22], [31, 22]].map(([x, z]) => stand.localToWorld(V(x, 0, z))),
    V(0, 0, 0).applyMatrix4(perch.matrixWorld).add(V(0, 60, 0)),
  ];
  const heroDir = V(-0.12, 0.36, 1).normalize();
  const cableMid = cablePts[Math.floor(cablePts.length * 0.42)].clone().add(V(0, 4, 0));

  // ---------- camera ----------

  const lookW = new THREE.Vector3(), offset = new THREE.Vector3(), up = new THREE.Vector3(0, 1, 0);

  // the canvas fills the stage, so nothing gets cut at its edges. the shot is framed for a box
  // inside it (#desk-frame, laid out in laptop.css like the canvas on index.html), then shifted there
  const frameEl = document.getElementById('desk-frame');
  // on wide screens the hero shot scrolls up with the hero text, like a picture in the page,
  // so it never slides over the big wordmark. on phones the stage is its own box on top
  const box = { x: 0, y: 0, w: 1, h: 1, cw: 1, ch: 1 };
  const wide = matchMedia('(min-width: 900px)');
  let follow = 0, lastFit = 0;
  function beginFit() {
    const cr = renderer.domElement.getBoundingClientRect();
    const fr = frameEl ? frameEl.getBoundingClientRect() : cr;
    const now = performance.now(), dt = Math.min(0.1, lastFit ? (now - lastFit) / 1000 : 1);
    lastFit = now;
    const want = wide.matches && document.documentElement.classList.contains('at-hero') ? scrollY : 0;
    follow = want > follow ? want : follow + (want - follow) * (1 - Math.exp(-dt * 7));
    Object.assign(box, { x: fr.left - cr.left, y: fr.top - cr.top - follow, w: Math.max(1, fr.width), h: Math.max(1, fr.height), cw: cr.width, ch: cr.height });
    camera.aspect = box.w / box.h;
    camera.clearViewOffset();
  }
  function endFit() {
    camera.setViewOffset(box.w, box.h, -box.x, -box.y, box.cw, box.ch);
  }

  // a pose in Clawd's own frame, then turned around the stand, so the desk stays level
  function place(look, pitch, yaw, dist) {
    lookW.copy(look).applyMatrix4(perch.matrixWorld);
    offset.set(0, Math.sin(pitch), Math.cos(pitch)).transformDirection(perch.matrixWorld).multiplyScalar(dist);
    offset.applyAxisAngle(up, -yaw);
    camera.position.copy(lookW).add(offset);
    camera.up.copy(up);
    camera.lookAt(lookW);
  }

  // where the camera has to be so all the points fit, looking along dir
  const hero = { pos: new THREE.Vector3(), target: new THREE.Vector3(), aspect: 0 };
  function frameHero() {
    if (hero.aspect === camera.aspect) return;
    hero.aspect = camera.aspect;
    const target = V(0, 0, 0);
    for (const p of heroPts) target.add(p);
    target.divideScalar(heroPts.length);
    let dist = 900;
    const margin = camera.aspect < 1.2 ? 0.88 : 0.9;
    const right = new THREE.Vector3(), upv = new THREE.Vector3(), q = new THREE.Vector3();
    for (let i = 0; i < 8; i++) {
      camera.position.copy(target).addScaledVector(heroDir, dist);
      camera.up.copy(up);
      camera.lookAt(target);
      camera.updateMatrixWorld();
      let x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
      for (const p of heroPts) {
        q.copy(p).project(camera);
        x0 = Math.min(x0, q.x); x1 = Math.max(x1, q.x);
        y0 = Math.min(y0, q.y); y1 = Math.max(y1, q.y);
      }
      const h = dist * Math.tan(deg(camera.fov / 2));
      right.setFromMatrixColumn(camera.matrixWorld, 0);
      upv.setFromMatrixColumn(camera.matrixWorld, 1);
      target.addScaledVector(right, (x0 + x1) / 2 * h * camera.aspect).addScaledVector(upv, (y0 + y1) / 2 * h);
      dist *= Math.max((x1 - x0) / 2, (y1 - y0) / 2) / margin;
    }
    hero.target.copy(target);
    hero.pos.copy(target).addScaledVector(heroDir, dist);
  }

  // ---------- scroll: the hero shot, then along the cable, then the pose ----------

  const heroEl = document.getElementById('hero');
  let hs = 0, opacity = 1;
  const heroAmount = () => {
    // the move starts as the hero leaves (site.js drops .at-hero at 35% of the screen)
    const range = Math.max(heroEl ? heroEl.offsetHeight : 0, innerHeight * 0.6);
    return Math.min(1, Math.max(0, (scrollY - range * 0.22) / (range * 0.62)));
  };
  hs = heroAmount();

  const smooth = (a, b, t) => {
    t = Math.min(1, Math.max(0, (t - a) / (b - a)));
    return t * t * (3 - 2 * t);
  };
  const p2 = new THREE.Vector3(), t2 = new THREE.Vector3(), p1 = new THREE.Vector3(), tmp = new THREE.Vector3();
  const bez = (out, a, b, c, t) => out.copy(a).multiplyScalar((1 - t) * (1 - t))
    .addScaledVector(b, 2 * t * (1 - t)).addScaledVector(c, t * t);

  function setOpacity(o) {
    if (o === opacity) return;
    opacity = o;
    for (const m of fading) {
      const want = o < 1;
      if (m.transparent !== want && m !== lapBlob.material) {
        m.transparent = want;
        m.needsUpdate = true;
      }
      m.opacity = m === lapBlob.material ? 0.32 * o : o;
    }
    laptop.visible = o > 0;
    for (const o2 of cableObjs) o2.visible = o > 0;
  }
  const cableObjs = desk.children.filter(o => o.isMesh);   // the cable tube
  cableObjs.push(perch.children[perch.children.length - 1]);  // and its plug on Clawd

  // after clawd3d.js placed the camera for the pose: blend from the hero shot along the cable
  function after(now, dt, pointer) {
    hs += (heroAmount() - hs) * (1 - Math.exp(-dt * 6));
    if (Math.abs(heroAmount() - hs) < 0.0005) hs = heroAmount();
    setOpacity(1 - smooth(0.4, 0.85, hs));
    term.update(now);
    if (hs < 0.2) cue(now);
    if (hs >= 0.999) return endFit();

    p2.copy(camera.position);
    t2.copy(lookW);
    frameHero();
    // the hero shot sways a little with the mouse
    const p0 = tmp.copy(hero.pos).sub(hero.target).applyAxisAngle(up, deg(pointer * 2 + Math.sin(now / 3100) * 0.6)).add(hero.target);
    p1.copy(cableMid).add(V(55, 52, 190));
    const t = ease(hs);
    const pos = bez(new THREE.Vector3(), p0, p1, p2, t);
    const tgt = bez(new THREE.Vector3(), hero.target, cableMid, t2, t);
    camera.position.copy(pos);
    camera.up.copy(up);
    camera.lookAt(tgt);
    endFit();
  }

  // Clawd's face follows the session on the laptop while we are in the hero
  let lastCue = null;
  function cue(now) {
    const name = term.cue(now);
    if (name && name !== lastCue) {
      lastCue = name;
      c.play([name]);
    }
  }
  addEventListener('scroll', () => { if (heroAmount() >= 0.2) lastCue = null; }, { passive: true });

  window.ClawdDesk = {
    // test only: hold the session at t seconds, and a png of the canvas, for the static fallback image
    hold(t) { term.hold(t); },
    snap() {
      renderer.render(scene, camera);
      return renderer.domElement.toDataURL('image/png');
    },
  };

  return { fit, beginFit, place, after };
}

const ease = t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;

// ---------- geometry ----------

function geo(build) {
  const b = { p: [], n: [], i: [] };
  build(b);
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(b.p), 3));
  g.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(b.n), 3));
  const n = b.p.length / 3;
  g.setIndex(new THREE.BufferAttribute(n > 65535 ? new Uint32Array(b.i) : new Uint16Array(b.i), 1));
  g.computeBoundingSphere();
  return g;
}

// a rounded rectangle block: w (x) by d (z), h tall from y0, corner radius r, edges rounded by bev
function slab(b, w, d, h, r, bev, cx, y0, cz, cs = 4, bs = 3) {
  r = Math.max(0.01, Math.min(r, w / 2 - 0.01, d / 2 - 0.01));
  bev = Math.min(bev, h / 2, r * 0.98);
  const rings = [];                  // [inset, y, normal out, normal up]
  if (bev > 0) {
    for (let k = 0; k <= bs; k++) {
      const a = -Math.PI / 2 + k / bs * Math.PI / 2;
      rings.push([bev * (1 - Math.cos(a)), y0 + bev * (1 + Math.sin(a)), Math.cos(a), Math.sin(a)]);
    }
    for (let k = 0; k <= bs; k++) {
      const a = k / bs * Math.PI / 2;
      rings.push([bev * (1 - Math.cos(a)), y0 + h - bev + bev * Math.sin(a), Math.cos(a), Math.sin(a)]);
    }
  } else {
    rings.push([0, y0, 1, 0], [0, y0 + h, 1, 0]);
  }
  const corners = [[1, 1], [-1, 1], [-1, -1], [1, -1]];
  const N = 4 * (cs + 1);
  const ring = (inset, y, no, nu) => {
    const first = b.p.length / 3;
    corners.forEach(([sx, sz], i) => {
      const ox = cx + sx * (w / 2 - r), oz = cz + sz * (d / 2 - r);
      for (let k = 0; k <= cs; k++) {
        const a = (i + k / cs) * Math.PI / 2, ca = Math.cos(a), sa = Math.sin(a);
        b.p.push(ox + (r - inset) * ca, y, oz + (r - inset) * sa);
        b.n.push(ca * no, nu, sa * no);
      }
    });
    return first;
  };
  // sides: flat walls get their own rings so the edges stay sharp when there is no bevel
  const starts = rings.map(rg => ring(...rg));
  for (let j = 0; j < starts.length - 1; j++) {
    for (let k = 0; k < N; k++) {
      const a = starts[j] + k, bb = starts[j] + (k + 1) % N, cc = starts[j + 1] + k, dd = starts[j + 1] + (k + 1) % N;
      b.i.push(a, cc, bb, bb, cc, dd);
    }
  }
  // caps
  for (const top of [false, true]) {
    const [inset, y] = top ? rings[rings.length - 1] : rings[0];
    const s = ring(inset, y, 0, top ? 1 : -1);
    const c = b.p.length / 3;
    b.p.push(cx, y, cz);
    b.n.push(0, top ? 1 : -1, 0);
    for (let k = 0; k < N; k++) {
      const p = s + k, q = s + (k + 1) % N;
      if (top) b.i.push(c, q, p); else b.i.push(c, p, q);
    }
  }
}

function keyboard(b, z0, y) {
  // mac style layout, keys in units of 18.6 mm, top row half height. z0 = back edge of the top row
  const u = 18.6, gap = 2.6;
  const rows = [
    [[1.5, ...Array(13).fill(1)], 0.55],
    [[...Array(13).fill(1), 1.5], 1],
    [[1.5, ...Array(13).fill(1)], 1],
    [[1.75, ...Array(11).fill(1), 1.75], 1],
    [[2.25, ...Array(10).fill(1), 2.25], 1],
    [[1, 1, 1, 1.25, 5, 1.25, 1, 1, 1, 1], 1],
  ];
  let z = z0;
  for (const [widths, hgt] of rows) {
    let x = -widths.reduce((s, v) => s + v, 0) * u / 2;
    const depth = hgt * u;
    widths.forEach((wu, i) => {
      const w = wu * u;
      const arrows = widths.length === 10;
      if (arrows && i === 8) {
        for (const k of [0, 1]) slab(b, w - gap, depth / 2 - gap / 2, 0.7, 1.2, 0.3, x + w / 2, y, z + depth / 4 + k * depth / 2, 2, 1);
      } else if (arrows && (i === 7 || i === 9)) {
        slab(b, w - gap, depth / 2 - gap / 2, 0.7, 1.2, 0.3, x + w / 2, y, z + depth * 0.75, 2, 1);
      } else {
        slab(b, w - gap, depth - gap, 0.7, 1.4, 0.3, x + w / 2, y, z + depth / 2, 2, 1);
      }
      x += w;
    });
    z += depth;
  }
}

// catmull-rom through the points, n samples per span
function spline(pts, n) {
  const out = [];
  const at = i => pts[Math.max(0, Math.min(pts.length - 1, i))];
  for (let i = 0; i < pts.length - 1; i++) {
    const p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
    for (let k = 0; k < n; k++) {
      const t = k / n, t2 = t * t, t3 = t2 * t;
      const f = (a, b, c, d) => 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
      out.push(new THREE.Vector3(f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y), f(p0.z, p1.z, p2.z, p3.z)));
    }
  }
  out.push(pts[pts.length - 1].clone());
  return out;
}

function tube(pts, r, sides) {
  return geo(b => {
    let nrm = new THREE.Vector3(0, 1, 0);
    const t = new THREE.Vector3(), bi = new THREE.Vector3();
    pts.forEach((p, i) => {
      t.copy(pts[Math.min(i + 1, pts.length - 1)]).sub(pts[Math.max(i - 1, 0)]).normalize();
      nrm.addScaledVector(t, -nrm.dot(t));
      if (nrm.lengthSq() < 1e-6) nrm.set(1, 0, 0).addScaledVector(t, -t.x);
      nrm.normalize();
      bi.crossVectors(t, nrm);
      for (let k = 0; k <= sides; k++) {
        const a = k / sides * Math.PI * 2, ca = Math.cos(a), sa = Math.sin(a);
        const nx = nrm.x * ca + bi.x * sa, ny = nrm.y * ca + bi.y * sa, nz = nrm.z * ca + bi.z * sa;
        b.p.push(p.x + nx * r, p.y + ny * r, p.z + nz * r);
        b.n.push(nx, ny, nz);
      }
      if (i) {
        const s0 = (i - 1) * (sides + 1), s1 = i * (sides + 1);
        for (let k = 0; k < sides; k++) b.i.push(s0 + k, s0 + k + 1, s1 + k, s0 + k + 1, s1 + k + 1, s1 + k);
      }
    });
  });
}

// ---------- the laptop screen: a Claude Code session that types itself ----------

const BG = '#161514', TEXT = '#ece9e4', DIM = '#8c8882', ORANGE = '#d77757', GREEN = '#78be76';
const PROMPT = 'make me a tiny desk pet that reacts to Claude Code';
const TYPE_AT = 0.5, TYPE_DT = 0.05, SENT = 3.6, LOOP = 16;
const WORDS = ['Clauding', 'Pondering', 'Noodling', 'Percolating'];
// (time, lines). a line is [text, color] pieces; '*' at the start is the dot, '>' the result elbow
const EVENTS = [
  [5.6, [['*', TEXT], ['On it! I\'ll build Tiny Clawd,', TEXT]], [['  a little pet for your desk that', TEXT]], [['  watches me work.', TEXT]]],
  [7.4, [['*', GREEN], ['Write', TEXT, 1], ['(firmware/main/main.cpp)', TEXT]], [['>', DIM], ['Wrote 212 lines', DIM]]],
  [9.2, [['*', GREEN], ['Bash', TEXT, 1], ['(idf.py build)', TEXT]], [['>', DIM], ['Project build complete', GREEN]]],
  [11, [['*', TEXT], ['Done. Tiny Clawd is alive.', TEXT]]],
];
// what Clawd's own face plays at each moment of the session
const CUES = [[0, 'idle'], [TYPE_AT, 'typing_along'], [SENT, 'thinking_words'], [7.4, 'typing'], [9.2, 'crane'], [11, 'goal']];

function terminal(renderer) {
  const W = 1280, H = 800;
  const cv = document.createElement('canvas');
  cv.width = W;
  cv.height = H;
  const g = cv.getContext('2d');
  const tex = new THREE.CanvasTexture(cv);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = renderer.capabilities.getMaxAnisotropy();

  const COLS = 40, X0 = 54;
  let cw = 0, fs = 0, lh = 0, ready = false, lastKey = '';
  const font = (size, bold) => `${bold ? 'bold ' : ''}${size}px "Departure Mono", monospace`;
  const measure = () => {
    g.font = font(100);
    cw = (W - 2 * X0) / COLS;
    fs = Math.floor(cw / (g.measureText('M').width / 100));
    lh = Math.round(fs * 1.3);
  };
  document.fonts.load(font(40)).then(() => { measure(); ready = true; lastKey = ''; }).catch(() => {});
  measure();

  const start = performance.now();
  let held = null;
  const clock = now => held !== null ? held : ((now - start) / 1000) % LOOP;

  function cue(now) {
    const t = clock(now);
    let name = null;
    for (const [at, n] of CUES) if (t >= at) name = n;
    return name;
  }

  // the clawd block art from the CLI banner, 16 x 13 cells
  const ART = [[3, 3, 10, 7], [1, 5, 2, 3], [13, 5, 2, 3], [4, 10, 1, 3], [6, 10, 1, 3], [9, 10, 1, 3], [11, 10, 1, 3]];
  function clawd(x, y, s) {
    g.fillStyle = ORANGE;
    for (const [a, b2, w, h] of ART) g.fillRect(x + a * s, y + b2 * s, w * s, h * s);
    g.fillStyle = BG;
    g.fillRect(x + 5 * s, y + 5 * s, s, 2 * s);
    g.fillRect(x + 10 * s, y + 5 * s, s, 2 * s);
  }

  function box(x, y, w, h, color) {
    g.strokeStyle = color;
    g.lineWidth = 3;
    g.beginPath();
    g.roundRect(x, y, w, h, 12);
    g.stroke();
  }

  function line(pieces, x, y) {
    for (const [text, color, bold] of pieces) {
      if (text === '*') {
        g.fillStyle = color;
        const s = Math.round(fs * 0.36);
        g.fillRect(x + (cw - s) / 2, y - fs * 0.36 - s / 2, s, s);
        x += cw * 2;
        continue;
      }
      if (text === '>') {
        g.strokeStyle = color;
        g.lineWidth = 3;
        g.beginPath();
        g.moveTo(x + cw * 2.6, y - fs * 0.85);
        g.lineTo(x + cw * 2.6, y - fs * 0.3);
        g.lineTo(x + cw * 3.6, y - fs * 0.3);
        g.stroke();
        x += cw * 4.5;
        continue;
      }
      g.fillStyle = color;
      g.font = font(fs, bold);
      g.fillText(text, x, y);
      x += cw * text.length;
    }
  }

  function draw(t) {
    g.fillStyle = BG;
    g.fillRect(0, 0, W, H);
    g.textBaseline = 'alphabetic';

    // window bar
    g.fillStyle = '#211f1d';
    g.fillRect(0, 0, W, 56);
    ['#ec6a5e', '#f4bf4f', '#61c554'].forEach((c, i) => {
      g.fillStyle = c;
      g.beginPath();
      g.arc(34 + i * 30, 28, 9, 0, Math.PI * 2);
      g.fill();
    });
    g.fillStyle = DIM;
    g.font = font(Math.round(fs * 0.62));
    g.textAlign = 'center';
    g.fillText('tinyclawd: claude', W / 2, 37);
    g.textAlign = 'left';

    const typed = Math.max(0, Math.min(PROMPT.length, Math.floor((t - TYPE_AT) / TYPE_DT)));
    const sent = t >= SENT;

    // the session above the prompt, newest at the bottom
    const rows = [];
    rows.push({ welcome: true }, { gap: 1 }, { gap: 1 }, { gap: 1 });
    if (sent) {
      rows.push({ gap: 1 });
      const split = PROMPT.lastIndexOf(' ', COLS - 3);
      rows.push({ p: [['> ' + PROMPT.slice(0, split), DIM]] }, { p: [['  ' + PROMPT.slice(split + 1), DIM]] });
    }
    for (const [at, ...lines] of EVENTS) {
      if (t < at) break;
      rows.push({ gap: 1 });
      for (const l of lines) rows.push({ p: l });
    }
    const done = t >= EVENTS[EVENTS.length - 1][0];
    if (sent && !done) {
      const secs = Math.floor(t - SENT) + 1;
      const word = WORDS[Math.floor((t - SENT) / 1.6) % WORDS.length];
      rows.push({ spin: true, p: [['  ' + word + '...', ORANGE], [` (${secs}s, esc to interrupt)`, DIM]] });
    }

    const top = 56 + lh * 0.4;
    const promptTop = H - lh * 3.6;
    const fit = Math.floor((promptTop - top) / lh);
    const shown = rows.slice(Math.max(0, rows.length - fit));
    let y = top + lh * 0.82;
    for (const r of shown) {
      if (r.welcome) {
        box(X0 - 10, y - lh * 0.82, W - 2 * X0 + 20, lh * 3.4, ORANGE);
        clawd(X0 + cw * 0.5, y - fs * 0.25, Math.round(fs * 0.13));
        line([['Welcome back!', TEXT, 1]], X0 + cw * 4, y + lh * 0.55);
        line([['~/code/tinyclawd', DIM]], X0 + cw * 4, y + lh * 1.55);
      } else if (r.p) {
        line(r.p, X0, y);
        if (r.spin) spinner(X0 + cw * 0.5, y - fs * 0.36, fs * 0.42, t);
      }
      y += lh;
    }

    // the prompt box
    box(X0 - 10, promptTop, W - 2 * X0 + 20, lh * 2.2, '#5a5652');
    const py = promptTop + lh * 0.95;
    const shownPrompt = sent ? '' : PROMPT.slice(0, typed);
    // long input wraps, keep the last line in the box
    const room = COLS - 3;
    const lines = shownPrompt.length > room ? [shownPrompt.slice(0, shownPrompt.lastIndexOf(' ', room)), shownPrompt.slice(shownPrompt.lastIndexOf(' ', room) + 1)] : [shownPrompt];
    line([['> ', TEXT], [lines[0], TEXT]], X0, py);
    let cx = X0 + cw * (2 + lines[0].length), cy = py;
    if (lines[1] !== undefined) {
      line([['  ' + lines[1], TEXT]], X0, py + lh);
      cx = X0 + cw * (2 + lines[1].length);
      cy = py + lh;
    }
    if (Math.floor(t * 1.9) % 2 === 0 || (t > TYPE_AT && t < SENT)) {
      g.fillStyle = TEXT;
      g.fillRect(cx, cy - fs * 0.78, cw * 0.9, fs * 0.92);
    }

    // status line, the one Tiny Clawd reads
    const ctx = 9 + Math.floor(Math.min(1, t / 12) * 12);
    g.font = font(Math.round(fs * 0.7));
    g.fillStyle = DIM;
    g.fillText(`ctx ${ctx}%  |  5h 42%  |  week 18%  |  main*  |  Opus`, X0, H - lh * 0.5);
  }

  function spinner(x, y, r, t) {
    g.strokeStyle = ORANGE;
    g.lineWidth = Math.max(3, r * 0.28);
    const a0 = t * 3;
    g.beginPath();
    for (let i = 0; i < 3; i++) {
      const a = a0 + i * Math.PI / 3;
      g.moveTo(x - Math.cos(a) * r, y - Math.sin(a) * r);
      g.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r);
    }
    g.stroke();
  }

  function update(now) {
    const t = clock(now);
    // redraw only when something on it changed: a letter, the cursor, a spinner step
    const k = [ready, Math.floor((t - TYPE_AT) / TYPE_DT), Math.floor(t * 1.9), Math.floor(t * 8)].join();
    if (k === lastKey) return;
    lastKey = k;
    draw(t);
    tex.needsUpdate = true;
  }
  draw(0);
  return { tex, update, cue, hold: t => { held = t; } };
}
