import createFlight from './physics.mjs';

/* The pitch laboratory: the rigid-body model of src/pitch.c through src/pitch_lab.c. JavaScript only draws and
 * passes the slider values; trim, integration and eigenvalues are computed in C. */
/** @param {string} id */
const $ = (id) => {
  const element = document.getElementById(id);
  if (!element) throw new Error(`missing #${id}`);
  return element;
};
/**
 * @template {HTMLElement} T
 * @param {string} id
 * @param {{ new (): T, name: string }} type
 * @returns {T}
 */
const $as = (id, type) => {
  const element = $(id);
  if (!(element instanceof type)) throw new Error(`#${id} is not a ${type.name}`);
  return element;
};
/** @param {HTMLCanvasElement} canvas */
const context2d = (canvas) => {
  const context = canvas.getContext('2d');
  if (!context) throw new Error('no 2D canvas context');
  return context;
};
// Same order as the enum in src/pitch_lab.h.
const names = [
  'time',
  'alpha',
  'q',
  'theta',
  'gamma',
  'v',
  'elevator',
  'trimAlpha',
  'margin',
  'spWn',
  'spZeta',
  'approxWn',
  'approxZeta',
  'phPeriod',
  'phTheory',
  'oscillatory',
  'departed',
  're0',
  're1',
  're2',
  're3',
  'im0',
  'im1',
  'im2',
  'im3',
];
const RUN = 12; // seconds of simulated time per experiment
/** @type {import('./physics.mjs').FlightModule | null} */
let engine = null;
/** @type {Record<string, number> | null} */
let lab = null;
let running = false,
  last = 0,
  acc = 0,
  /** @type {Record<string, number>[]} */
  trace = [],
  experiment = 'trim';
/** The loaded C engine; the controls stay disabled until it is. */
function core() {
  if (!engine) throw new Error('the C engine is not loaded');
  return engine;
}

const read = () => Object.fromEntries(names.map((name, i) => [name, core()._pl_value(i)]));
/** @type {(x: number, d: number) => string} */
const fmt = (x, d) => (Number.isFinite(x) ? x.toFixed(d) : '—');
/** @param {HTMLCanvasElement} canvas */
function size(canvas) {
  const b = canvas.getBoundingClientRect(),
    dpr = Math.min(devicePixelRatio || 1, 2);
  const w = Math.round(b.width * dpr),
    h = Math.round(b.height * dpr);
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  const ctx = context2d(canvas);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.font = '14px system-ui,-apple-system,Segoe UI,sans-serif';
  return { ctx, w: b.width, h: b.height };
}

function reset() {
  core()._pl_reset(Number($as('pl-margin', HTMLInputElement).value));
  lab = read();
  trace = [{ ...lab }];
  running = false;
  acc = 0;
  experiment = 'trim';
}
/** @param {string} kind */
function start(kind) {
  reset();
  experiment = kind;
  if (kind === 'step') core()._pl_elevator(Number($as('pl-elevator', HTMLInputElement).value));
  else core()._pl_kick(6);
  lab = read();
  trace = [{ ...lab }];
  running = true;
  last = 0;
  render();
}
/** @param {number} timestamp */
function step(timestamp) {
  if (running) {
    if (!last) last = timestamp;
    acc += Math.min((timestamp - last) / 1000, 0.25);
    last = timestamp;
    while (acc >= 0.02 && running) {
      core()._pl_step(1);
      acc -= 0.02;
      lab = read();
      trace.push({ ...lab });
      if (lab.departed || lab.time >= RUN - 1e-9) running = false;
    }
    render();
  } else last = 0;
  requestAnimationFrame(step);
}

// Attitude: the horizon, the glider at its pitch attitude theta, the velocity along gamma, alpha between them.
function drawAttitude() {
  if (!lab) return;
  const { ctx, w, h } = size($as('pl-attitude', HTMLCanvasElement)),
    cx = w * 0.55,
    cy = h * 0.55,
    len = Math.min(w * 0.3, h * 0.45, 140);
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#e8f0ef';
  ctx.fillRect(0, 0, w, cy);
  ctx.fillStyle = '#eee7d6';
  ctx.fillRect(0, cy, w, h - cy);
  ctx.strokeStyle = '#8b958f';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, cy);
  ctx.lineTo(w, cy);
  ctx.stroke();
  const th = (lab.theta * Math.PI) / 180,
    ga = (lab.gamma * Math.PI) / 180;
  ctx.save();
  ctx.translate(cx, cy);
  ctx.rotate(-th);
  ctx.strokeStyle = '#243f43';
  ctx.fillStyle = '#243f43';
  ctx.lineWidth = 4;
  ctx.lineCap = 'round';
  ctx.beginPath();
  ctx.moveTo(-len, 0);
  ctx.lineTo(len * 0.8, 0);
  ctx.stroke(); // fuselage
  ctx.beginPath();
  ctx.ellipse(len * 0.55, -3, len * 0.22, 9, 0, 0, Math.PI * 2);
  ctx.fill(); // cockpit
  ctx.lineWidth = 6;
  ctx.beginPath();
  ctx.moveTo(-len * 0.05, 2);
  ctx.lineTo(len * 0.3, 2);
  ctx.stroke(); // wing root
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.moveTo(-len, 0);
  ctx.lineTo(-len * 0.92, -34);
  ctx.stroke(); // fin
  ctx.beginPath();
  ctx.moveTo(-len * 1.05, 0);
  ctx.lineTo(-len * 0.78, 0);
  ctx.stroke(); // tailplane
  ctx.fillStyle = '#c98a2e';
  ctx.beginPath();
  ctx.arc(len * 0.06, 0, 6, 0, Math.PI * 2);
  ctx.fill(); // centre of gravity
  ctx.restore();
  // velocity vector along the path, and the angle of attack between body axis and path
  ctx.strokeStyle = '#336960';
  ctx.fillStyle = '#336960';
  ctx.lineWidth = 3;
  const vx = Math.cos(ga) * len * 1.1,
    vy = -Math.sin(ga) * len * 1.1;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(cx + vx, cy + vy);
  ctx.stroke();
  const a = Math.atan2(vy, vx);
  ctx.beginPath();
  ctx.moveTo(cx + vx, cy + vy);
  ctx.lineTo(cx + vx - 12 * Math.cos(a - 0.4), cy + vy - 12 * Math.sin(a - 0.4));
  ctx.lineTo(cx + vx - 12 * Math.cos(a + 0.4), cy + vy - 12 * Math.sin(a + 0.4));
  ctx.fill();
  ctx.strokeStyle = '#a3473a';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(cx, cy, len * 0.7, -th, -ga, th < ga);
  ctx.stroke();
  ctx.fillStyle = '#243f43';
  ctx.textBaseline = 'top';
  ctx.fillText(`θ attitude ${fmt(lab.theta, 2)}°`, 12, 10);
  ctx.fillStyle = '#336960';
  ctx.fillText(`γ path ${fmt(lab.gamma, 2)}°  ·  V ${fmt(lab.v, 1)} m/s`, 12, 30);
  ctx.fillStyle = '#a3473a';
  ctx.fillText(`α = θ − γ  ${fmt(lab.alpha, 2)}°`, 12, 50);
  ctx.fillStyle = '#566663';
  ctx.textBaseline = 'bottom';
  ctx.fillText('true angles · ● centre of gravity', 12, h - 8);
}

// Alpha and q against time, two panels with their own axes and units.
function drawTrace() {
  if (!lab) return;
  const { ctx, w, h } = size($as('pl-trace', HTMLCanvasElement)),
    left = 56,
    right = w - 12,
    mid = h / 2;
  ctx.clearRect(0, 0, w, h);
  const panels = [
    {
      key: 'alpha',
      label: 'α [deg]',
      color: '#1f5f8b',
      top: 24,
      bottom: mid - 14,
      extra: lab.trimAlpha,
    },
    { key: 'q', label: 'q [deg/s]', color: '#a3473a', top: mid + 20, bottom: h - 48, extra: 0 },
  ];
  /** @param {number} t */
  const tx = (t) => left + ((right - left) * t) / RUN;
  for (const p of panels) {
    let lo = Infinity,
      hi = -Infinity;
    for (const s of trace) {
      lo = Math.min(lo, s[p.key]);
      hi = Math.max(hi, s[p.key]);
    }
    if (Number.isFinite(p.extra)) {
      lo = Math.min(lo, p.extra);
      hi = Math.max(hi, p.extra);
    }
    if (hi - lo < 0.2) {
      lo -= 0.1;
      hi += 0.1;
    }
    const pad = (hi - lo) * 0.08;
    lo -= pad;
    hi += pad;
    /** @param {number} v */
    const ty = (v) => p.bottom - ((p.bottom - p.top) * (v - lo)) / (hi - lo);
    ctx.strokeStyle = '#d9dfd7';
    ctx.lineWidth = 1;
    ctx.strokeRect(left, p.top, right - left, p.bottom - p.top);
    ctx.fillStyle = '#566663';
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'right';
    ctx.fillText(hi.toFixed(1), left - 6, p.top + 2);
    ctx.fillText(lo.toFixed(1), left - 6, p.bottom - 2);
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    ctx.fillStyle = p.color;
    ctx.fillText(p.label, left, p.top - 4);
    if (Number.isFinite(p.extra)) {
      ctx.setLineDash([6, 5]);
      ctx.strokeStyle = '#888';
      ctx.beginPath();
      ctx.moveTo(left, ty(p.extra));
      ctx.lineTo(right, ty(p.extra));
      ctx.stroke();
      ctx.setLineDash([]);
    }
    ctx.strokeStyle = p.color;
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    trace.forEach((s, i) => {
      if (i) ctx.lineTo(tx(s.time), ty(s[p.key]));
      else ctx.moveTo(tx(s.time), ty(s[p.key]));
    });
    ctx.stroke();
  }
  ctx.fillStyle = '#566663';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'bottom';
  ctx.fillText('0', left, h - 28);
  ctx.textAlign = 'right';
  ctx.fillText(`${RUN} s`, right, h - 28);
  ctx.textAlign = 'center';
  ctx.fillText('time', (left + right) / 2, h - 28);
  ctx.textAlign = 'left';
  ctx.fillText('dashed: steady α (top), zero (bottom)', left, h - 6);
}

// The four eigenvalues in the complex plane, with the two-state approximation of the short period.
function drawRoots() {
  if (!lab) return;
  const { ctx, w, h } = size($as('pl-roots', HTMLCanvasElement)),
    left = 48,
    right = w - 14,
    top = 30,
    bottom = h - 40;
  const x0 = -6,
    x1 = 1,
    y0 = -5,
    y1 = 5;
  /** @param {number} v */
  const X = (v) => left + ((right - left) * (v - x0)) / (x1 - x0);
  /** @param {number} v */
  const Y = (v) => bottom - ((bottom - top) * (v - y0)) / (y1 - y0);
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#fbe9e6';
  ctx.fillRect(X(0), top, right - X(0), bottom - top);
  ctx.strokeStyle = '#d9dfd7';
  ctx.strokeRect(left, top, right - left, bottom - top);
  ctx.strokeStyle = '#8b958f';
  ctx.beginPath();
  ctx.moveTo(X(0), top);
  ctx.lineTo(X(0), bottom);
  ctx.moveTo(left, Y(0));
  ctx.lineTo(right, Y(0));
  ctx.stroke();
  ctx.fillStyle = '#566663';
  ctx.textBaseline = 'top';
  ctx.textAlign = 'left';
  ctx.fillText(String(x0), left, bottom + 4);
  ctx.textAlign = 'right';
  ctx.fillText(String(x1), right, bottom + 4);
  ctx.textAlign = 'center';
  ctx.fillText('real part [1/s] · shaded: unstable', (left + right) / 2, bottom + 20);
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  ctx.fillText(`${y1}`, left - 6, top + 4);
  ctx.fillText(`${y0}`, left - 6, bottom - 4);
  ctx.textAlign = 'left';
  ctx.textBaseline = 'bottom';
  ctx.fillText('imaginary [rad/s]', left, top - 6);
  /**
   * @param {number} re
   * @param {number} im
   * @param {string} color
   * @param {boolean} filled
   */
  const dot = (re, im, color, filled) => {
    const x = Math.min(right, Math.max(left, X(re))),
      y = Math.min(bottom, Math.max(top, Y(im)));
    ctx.beginPath();
    ctx.arc(x, y, 7, 0, Math.PI * 2);
    if (filled) {
      ctx.fillStyle = color;
      ctx.fill();
    } else {
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.5;
      ctx.stroke();
    }
  };
  for (let i = 0; i < 4; i++)
    dot(
      lab[`re${i}`],
      lab[`im${i}`],
      lab[`re${i}`] > 0 ? '#a3473a' : !lab.oscillatory ? '#8b958f' : i < 2 ? '#1f5f8b' : '#336960',
      true,
    );
  const s = lab.approxZeta < 1 ? Math.sqrt(1 - lab.approxZeta ** 2) : 0;
  dot(-lab.approxZeta * lab.approxWn, lab.approxWn * s, '#c98a2e', false);
  dot(-lab.approxZeta * lab.approxWn, -lab.approxWn * s, '#c98a2e', false);
}

function render() {
  if (!lab) return;
  $('pl-margin-value').textContent = Number($as('pl-margin', HTMLInputElement).value).toFixed(2);
  const e = Number($as('pl-elevator', HTMLInputElement).value);
  $('pl-elevator-value').textContent = `${e > 0 ? '+' : ''}${e.toFixed(1)}°`;
  const now = lab;
  const unstable = [0, 1, 2, 3].some((i) => now[`re${i}`] > 1e-9);
  const lambda = Math.max(...[0, 1, 2, 3].map((i) => now[`re${i}`]));
  $('pl-sp').textContent = lab.oscillatory
    ? `${fmt(lab.spWn, 3)} rad/s · ζ ${fmt(lab.spZeta, 3)}`
    : 'not an oscillation';
  $('pl-approx').textContent = `${fmt(lab.approxWn, 3)} rad/s · ζ ${fmt(lab.approxZeta, 3)}`;
  $('pl-ph').textContent = lab.oscillatory ? `${fmt(lab.phPeriod, 2)} s` : 'not an oscillation';
  $('pl-ph-theory').textContent =
    Number.isFinite(lab.phTheory) && lab.margin > 0
      ? `${fmt(lab.phTheory, 2)} s`
      : '— (needs a positive margin)';
  $('pl-verdict').textContent = unstable
    ? `Diverges: real eigenvalue +${fmt(lambda, 3)} /s — the centre of gravity is behind the neutral point`
    : Math.abs(lab.margin) < 1e-9
      ? 'Neutral: one eigenvalue is zero'
      : 'Stable: every eigenvalue has a negative real part';
  $('pl-verdict').dataset.tone = unstable ? 'warn' : '';
  $('pl-steady').textContent = Number.isFinite(lab.trimAlpha)
    ? `${fmt(lab.trimAlpha, 3)}°`
    : '— (no trim at zero margin)';
  $('pl-status').textContent = lab.departed
    ? `Departed at ${fmt(lab.time, 1)} s: |α| > 25° or airspeed out of range`
    : running
      ? `${experiment === 'step' ? 'Elevator step' : 'Nose kick'} · ${fmt(lab.time, 1)} s`
      : lab.time > 0
        ? `Observation complete · ${RUN} s`
        : 'Trimmed at 4°. Step the elevator or kick the nose.';
  $('pl-clock').textContent = `${fmt(lab.time, 1)} s`;
  drawAttitude();
  drawTrace();
  drawRoots();
  Object.defineProperty(window, 'pitchLab', {
    configurable: true,
    enumerable: true,
    writable: true,
    value: {
      snapshot: {
        ...lab,
        running,
        experiment,
        samples: trace.length,
        unstable,
        lambda,
        verdict: $('pl-verdict').textContent,
        status: $('pl-status').textContent,
      },
    },
  });
}

$('pl-margin').addEventListener('input', () => {
  reset();
  render();
});
$('pl-elevator').addEventListener('input', render);
$('pl-step').addEventListener('click', () => start('step'));
$('pl-kick').addEventListener('click', () => start('kick'));
$('pl-reset').addEventListener('click', () => {
  reset();
  render();
});
for (const id of ['pl-attitude', 'pl-trace', 'pl-roots']) new ResizeObserver(render).observe($(id));
try {
  engine = await createFlight();
  for (const id of ['pl-margin', 'pl-elevator', 'pl-step', 'pl-kick', 'pl-reset']) {
    const control = $(id);
    if (control instanceof HTMLButtonElement || control instanceof HTMLInputElement)
      control.disabled = false;
  }
  reset();
  render();
  requestAnimationFrame(step);
} catch {
  $('pl-status').textContent = 'Pitch model unavailable — reload the page to retry';
}
