import createFlight from './physics.mjs';

/* The pitch laboratory: the rigid-body model of src/pitch.c through src/pitch_lab.c. JavaScript only draws and
 * passes the slider values; trim, integration and eigenvalues are computed in C. */
const $ = id => document.getElementById(id);
// Same order as the enum in src/pitch_lab.h.
const names = ['time','alpha','q','theta','gamma','v','elevator','trimAlpha','margin','spWn','spZeta','approxWn','approxZeta',
  'phPeriod','phTheory','oscillatory','departed','re0','re1','re2','re3','im0','im1','im2','im3'];
const RUN = 12;   // seconds of simulated time per experiment
let engine, lab, running = false, last = 0, acc = 0, trace = [], experiment = 'trim';

const read = () => Object.fromEntries(names.map((name, i) => [name, engine._pl_value(i)]));
const fmt = (x, d) => Number.isFinite(x) ? x.toFixed(d) : '—';
function size(canvas) {
  const b = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const w = Math.round(b.width * dpr), h = Math.round(b.height * dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.font = '14px system-ui,-apple-system,Segoe UI,sans-serif';
  return {ctx, w: b.width, h: b.height};
}

function reset() {
  engine._pl_reset(Number($('pl-margin').value));
  lab = read(); trace = [{...lab}]; running = false; acc = 0; experiment = 'trim';
}
function start(kind) {
  reset();
  experiment = kind;
  if (kind === 'step') engine._pl_elevator(Number($('pl-elevator').value));
  else engine._pl_kick(6);
  lab = read(); trace = [{...lab}]; running = true; last = 0;
  render();
}
function step(timestamp) {
  if (running) {
    if (!last) last = timestamp;
    acc += Math.min((timestamp - last) / 1000, .25); last = timestamp;
    while (acc >= .02 && running) {
      engine._pl_step(1); acc -= .02; lab = read(); trace.push({...lab});
      if (lab.departed || lab.time >= RUN - 1e-9) running = false;
    }
    render();
  } else last = 0;
  requestAnimationFrame(step);
}

// Attitude: the horizon, the glider at its pitch attitude theta, the velocity along gamma, alpha between them.
function drawAttitude() {
  const {ctx, w, h} = size($('pl-attitude')), cx = w * .55, cy = h * .55, len = Math.min(w * .3, h * .45, 140);
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#e8f0ef'; ctx.fillRect(0, 0, w, cy); ctx.fillStyle = '#eee7d6'; ctx.fillRect(0, cy, w, h - cy);
  ctx.strokeStyle = '#8b958f'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(w, cy); ctx.stroke();
  const th = lab.theta * Math.PI / 180, ga = lab.gamma * Math.PI / 180;
  ctx.save(); ctx.translate(cx, cy); ctx.rotate(-th);
  ctx.strokeStyle = '#243f43'; ctx.fillStyle = '#243f43'; ctx.lineWidth = 4; ctx.lineCap = 'round';
  ctx.beginPath(); ctx.moveTo(-len, 0); ctx.lineTo(len * .8, 0); ctx.stroke();                         // fuselage
  ctx.beginPath(); ctx.ellipse(len * .55, -3, len * .22, 9, 0, 0, Math.PI * 2); ctx.fill();            // cockpit
  ctx.lineWidth = 6; ctx.beginPath(); ctx.moveTo(-len * .05, 2); ctx.lineTo(len * .3, 2); ctx.stroke(); // wing root
  ctx.lineWidth = 4; ctx.beginPath(); ctx.moveTo(-len, 0); ctx.lineTo(-len * .92, -34); ctx.stroke();   // fin
  ctx.beginPath(); ctx.moveTo(-len * 1.05, 0); ctx.lineTo(-len * .78, 0); ctx.stroke();                 // tailplane
  ctx.fillStyle = '#c98a2e'; ctx.beginPath(); ctx.arc(len * .06, 0, 6, 0, Math.PI * 2); ctx.fill();     // centre of gravity
  ctx.restore();
  // velocity vector along the path, and the angle of attack between body axis and path
  ctx.strokeStyle = '#336960'; ctx.fillStyle = '#336960'; ctx.lineWidth = 3;
  const vx = Math.cos(ga) * len * 1.1, vy = -Math.sin(ga) * len * 1.1;
  ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(cx + vx, cy + vy); ctx.stroke();
  const a = Math.atan2(vy, vx); ctx.beginPath(); ctx.moveTo(cx + vx, cy + vy);
  ctx.lineTo(cx + vx - 12 * Math.cos(a - .4), cy + vy - 12 * Math.sin(a - .4)); ctx.lineTo(cx + vx - 12 * Math.cos(a + .4), cy + vy - 12 * Math.sin(a + .4)); ctx.fill();
  ctx.strokeStyle = '#a3473a'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(cx, cy, len * .7, -th, -ga, th < ga); ctx.stroke();
  ctx.fillStyle = '#243f43'; ctx.textBaseline = 'top';
  ctx.fillText(`θ attitude ${fmt(lab.theta, 2)}°`, 12, 10);
  ctx.fillStyle = '#336960'; ctx.fillText(`γ path ${fmt(lab.gamma, 2)}°  ·  V ${fmt(lab.v, 1)} m/s`, 12, 30);
  ctx.fillStyle = '#a3473a'; ctx.fillText(`α = θ − γ  ${fmt(lab.alpha, 2)}°`, 12, 50);
  ctx.fillStyle = '#566663'; ctx.textBaseline = 'bottom'; ctx.fillText('true angles · ● centre of gravity', 12, h - 8);
}

// Alpha and q against time, two panels with their own axes and units.
function drawTrace() {
  const {ctx, w, h} = size($('pl-trace')), left = 56, right = w - 12, mid = h / 2;
  ctx.clearRect(0, 0, w, h);
  const panels = [
    {key: 'alpha', label: 'α [deg]', color: '#1f5f8b', top: 24, bottom: mid - 14, extra: lab.trimAlpha},
    {key: 'q', label: 'q [deg/s]', color: '#a3473a', top: mid + 20, bottom: h - 48, extra: 0},
  ];
  const tx = t => left + (right - left) * t / RUN;
  for (const p of panels) {
    let lo = Infinity, hi = -Infinity;
    for (const s of trace) { lo = Math.min(lo, s[p.key]); hi = Math.max(hi, s[p.key]); }
    if (Number.isFinite(p.extra)) { lo = Math.min(lo, p.extra); hi = Math.max(hi, p.extra); }
    if (hi - lo < .2) { lo -= .1; hi += .1; }
    const pad = (hi - lo) * .08; lo -= pad; hi += pad;
    const ty = v => p.bottom - (p.bottom - p.top) * (v - lo) / (hi - lo);
    ctx.strokeStyle = '#d9dfd7'; ctx.lineWidth = 1; ctx.strokeRect(left, p.top, right - left, p.bottom - p.top);
    ctx.fillStyle = '#566663'; ctx.textBaseline = 'middle'; ctx.textAlign = 'right';
    ctx.fillText(hi.toFixed(1), left - 6, p.top + 2); ctx.fillText(lo.toFixed(1), left - 6, p.bottom - 2);
    ctx.textAlign = 'left'; ctx.textBaseline = 'bottom'; ctx.fillStyle = p.color; ctx.fillText(p.label, left, p.top - 4);
    if (Number.isFinite(p.extra)) {
      ctx.setLineDash([6, 5]); ctx.strokeStyle = '#888'; ctx.beginPath(); ctx.moveTo(left, ty(p.extra)); ctx.lineTo(right, ty(p.extra)); ctx.stroke(); ctx.setLineDash([]);
    }
    ctx.strokeStyle = p.color; ctx.lineWidth = 2.5; ctx.beginPath();
    trace.forEach((s, i) => i ? ctx.lineTo(tx(s.time), ty(s[p.key])) : ctx.moveTo(tx(s.time), ty(s[p.key]))); ctx.stroke();
  }
  ctx.fillStyle = '#566663'; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom';
  ctx.fillText('0', left, h - 28); ctx.textAlign = 'right'; ctx.fillText(`${RUN} s`, right, h - 28);
  ctx.textAlign = 'center'; ctx.fillText('time', (left + right) / 2, h - 28);
  ctx.textAlign = 'left'; ctx.fillText('dashed: steady α (top), zero (bottom)', left, h - 6);
}

// The four eigenvalues in the complex plane, with the two-state approximation of the short period.
function drawRoots() {
  const {ctx, w, h} = size($('pl-roots')), left = 48, right = w - 14, top = 30, bottom = h - 40;
  const x0 = -6, x1 = 1, y0 = -5, y1 = 5, X = v => left + (right - left) * (v - x0) / (x1 - x0), Y = v => bottom - (bottom - top) * (v - y0) / (y1 - y0);
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#fbe9e6'; ctx.fillRect(X(0), top, right - X(0), bottom - top);
  ctx.strokeStyle = '#d9dfd7'; ctx.strokeRect(left, top, right - left, bottom - top);
  ctx.strokeStyle = '#8b958f'; ctx.beginPath(); ctx.moveTo(X(0), top); ctx.lineTo(X(0), bottom); ctx.moveTo(left, Y(0)); ctx.lineTo(right, Y(0)); ctx.stroke();
  ctx.fillStyle = '#566663'; ctx.textBaseline = 'top'; ctx.textAlign = 'left';
  ctx.fillText(String(x0), left, bottom + 4); ctx.textAlign = 'right'; ctx.fillText(String(x1), right, bottom + 4);
  ctx.textAlign = 'center'; ctx.fillText('real part [1/s] · shaded: unstable', (left + right) / 2, bottom + 20);
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle'; ctx.fillText(`${y1}`, left - 6, top + 4); ctx.fillText(`${y0}`, left - 6, bottom - 4);
  ctx.textAlign = 'left'; ctx.textBaseline = 'bottom'; ctx.fillText('imaginary [rad/s]', left, top - 6);
  const dot = (re, im, color, filled) => {
    const x = Math.min(right, Math.max(left, X(re))), y = Math.min(bottom, Math.max(top, Y(im)));
    ctx.beginPath(); ctx.arc(x, y, 7, 0, Math.PI * 2);
    if (filled) { ctx.fillStyle = color; ctx.fill(); } else { ctx.strokeStyle = color; ctx.lineWidth = 2.5; ctx.stroke(); }
  };
  for (let i = 0; i < 4; i++) dot(lab[`re${i}`], lab[`im${i}`], lab[`re${i}`] > 0 ? '#a3473a' : !lab.oscillatory ? '#8b958f' : i < 2 ? '#1f5f8b' : '#336960', true);
  const s = lab.approxZeta < 1 ? Math.sqrt(1 - lab.approxZeta ** 2) : 0;
  dot(-lab.approxZeta * lab.approxWn, lab.approxWn * s, '#c98a2e', false); dot(-lab.approxZeta * lab.approxWn, -lab.approxWn * s, '#c98a2e', false);
}

function render() {
  if (!lab) return;
  $('pl-margin-value').textContent = Number($('pl-margin').value).toFixed(2);
  const e = Number($('pl-elevator').value);
  $('pl-elevator-value').textContent = `${e > 0 ? '+' : ''}${e.toFixed(1)}°`;
  const unstable = [0, 1, 2, 3].some(i => lab[`re${i}`] > 1e-9);
  const lambda = Math.max(...[0, 1, 2, 3].map(i => lab[`re${i}`]));
  $('pl-sp').textContent = lab.oscillatory ? `${fmt(lab.spWn, 3)} rad/s · ζ ${fmt(lab.spZeta, 3)}` : 'not an oscillation';
  $('pl-approx').textContent = `${fmt(lab.approxWn, 3)} rad/s · ζ ${fmt(lab.approxZeta, 3)}`;
  $('pl-ph').textContent = lab.oscillatory ? `${fmt(lab.phPeriod, 2)} s` : 'not an oscillation';
  $('pl-ph-theory').textContent = Number.isFinite(lab.phTheory) && lab.margin > 0 ? `${fmt(lab.phTheory, 2)} s` : '— (needs a positive margin)';
  $('pl-verdict').textContent = unstable ? `Diverges: real eigenvalue +${fmt(lambda, 3)} /s — the centre of gravity is behind the neutral point`
    : Math.abs(lab.margin) < 1e-9 ? 'Neutral: one eigenvalue is zero' : 'Stable: every eigenvalue has a negative real part';
  $('pl-verdict').dataset.tone = unstable ? 'warn' : '';
  $('pl-steady').textContent = Number.isFinite(lab.trimAlpha) ? `${fmt(lab.trimAlpha, 3)}°` : '— (no trim at zero margin)';
  $('pl-status').textContent = lab.departed ? `Departed at ${fmt(lab.time, 1)} s: |α| > 25° or airspeed out of range`
    : running ? `${experiment === 'step' ? 'Elevator step' : 'Nose kick'} · ${fmt(lab.time, 1)} s` : lab.time > 0 ? `Observation complete · ${RUN} s` : 'Trimmed at 4°. Step the elevator or kick the nose.';
  $('pl-clock').textContent = `${fmt(lab.time, 1)} s`;
  drawAttitude(); drawTrace(); drawRoots();
  window.pitchLab = {snapshot: {...lab, running, experiment, samples: trace.length, unstable, lambda,
    verdict: $('pl-verdict').textContent, status: $('pl-status').textContent}};
}

$('pl-margin').addEventListener('input', () => { reset(); render(); });
$('pl-elevator').addEventListener('input', render);
$('pl-step').addEventListener('click', () => start('step'));
$('pl-kick').addEventListener('click', () => start('kick'));
$('pl-reset').addEventListener('click', () => { reset(); render(); });
for (const id of ['pl-attitude', 'pl-trace', 'pl-roots']) new ResizeObserver(render).observe($(id));
try {
  engine = await createFlight();
  for (const id of ['pl-margin', 'pl-elevator', 'pl-step', 'pl-kick', 'pl-reset']) $(id).disabled = false;
  reset(); render(); requestAnimationFrame(step);
} catch {
  $('pl-status').textContent = 'Pitch model unavailable — reload the page to retry';
}
