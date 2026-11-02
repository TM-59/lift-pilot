import createFlight from './physics.mjs';

/* Only presentation and input live here. Every state value, integration step and story number is C. */
const $ = id => document.getElementById(id);
const names = ['x','h','v','gamma','alpha','time','command','throttle','lift','drag','ld','sink','energy','status','runway','onRunway','mode','stall'];
// The last ferry (src/ferry.c): same order as enum FerryValue.
const ferryNames = ['x','h','v','gamma','alpha','time','command','throttle','lift','drag','ld','sink','stall','hop','scene','banner',
  'strip','length','startH','airLift','wind','charge','motor','low','lowOn','need','outlandings','route','pulled','ending','loud',
  'heavy','groundSpeed','making','hopTime','trailered','outHop'];
const scene = $('scene'), plot = $('plot');
const sky = new Image(), glider = new Image();
sky.src = 'art/airfield.jpg'; glider.src = 'art/glider-sprite.png';
let engine, state, paused = true, last = 0, accumulator = 0, samples = [], sampleTime = -1;
let ferry = false, shownScene = -2, command = 4, throttle = 0;
const heavyLog = [];
const radToDeg = 180 / Math.PI;
const modeNames = ['MANUAL APPROACH','TRIMMED LANDING','SPEED ↔ HEIGHT'];
const notes = [
  'An unpowered approach from 100 m. Land with sink below 2 m/s and a shallow path angle.',
  'Fixed 4° angle of attack, throttle off. The runway is placed from a dry run of the same model. Controls are locked during the demonstration.',
  'A trimmed glide at 3,000 m with 10% extra airspeed. Watch a 120 s phugoid experiment. Controls are locked; there is no autopilot in manual flight.'
];

// ------------------------------------------------------------------ the last ferry: words
// Scenes open where the C ferry says (src/ferry.c); the words live here. Index = FS_* in src/ferry.h.
const SCENES = [
  {id: 'hangar', title: 'Upper Field, before sunrise', lines: [
    ['Joost', "Battery's full. My own cells are in behind the seat. They're not in the sale."],
    ['Joost', "Height first. The motor is for after you've made the mistake."],
    ['Joost', "Weir, Orchard, Mill, Ferry Point, then the coast. Their man says she'll be teaching circuits by Friday."],
    ['', 'He goes back into the hangar before the wheels leave the grass.']],
    note: 'Pitch (W/S or − +) sets the angle of attack; the motor (R/F) climbs while the battery lasts. Land on the strip, gently. If the glide you need gets steeper than the glide you are making, you are low.',
    options: ['Take off']},
  {id: 'weir', title: 'Weir Strip', lines: [
    ['Ros', "Weir's behind you? Good. It's Ros, at the orchard. Joost's sister. He'll have told you I talk."],
    ['Ros', "The quarry's blasting today. The air over the pit goes down like a bucket in a well, but it's the short way, you'd be here for lunch."],
    ['Ros', 'Or round Hob Ridge. Long and dull, and you climb out along the slope. Joost would say the ridge. Joost says the ridge about everything.']],
    options: ['Cut across the quarry (the short way, sinking air)', 'Go round Hob Ridge (the long way, a higher climb-out)']},
  {id: 'orchard', title: 'Orchard Strip', lines: [
    ['Ros', "Sit. Eat. The panels have been on your battery since you rolled to a stop, so you're not leaving hungry and you're not leaving flat."],
    ['Ros', "And listen to this, it came this morning. 'The Kestrel will join our training line. Students will begin circuits on Friday.'"],
    ['Ros', 'Circuits, Joost! Children going round and round in her!'],
    ['Joost', '…Circuits.'],
    ['Joost', 'Tell them the left brake grabs.']],
    note: "Ros's solar panels have topped up the battery.",
    options: ['Stay the night']},
  {id: 'buyer', title: 'A second letter', heavy: true, lines: [
    ['', "It comes to Joost after dark. Ros reads it to you over the phone because he won't."],
    ['Ros', "'…the engine and instruments will be removed for resale. The airframe will be disposed of.'"],
    ['Ros', 'Disposed of. Like a fridge.'],
    ['Joost', 'He sat in her. He sat in her and asked me about the trim.'],
    ['Joost', "The club's debts come out of that money. It's signed. Fly her on."]],
    note: 'From Mill Strip on, the valley wind will be on the nose all the way down.',
    options: ['Fly on to Mill Strip']},
  {id: 'mill', title: 'Mill Strip', heavy: true, lines: [
    ['', "Joost's van is on the grass when you land. His toolbox is open before you've unstrapped."],
    ['Joost', "The contract says one motor glider. It doesn't say what's in her."],
    ['Joost', "Forty minutes and she goes to him as a glider. The motor stays here, with me. The miller's winch will get you up."],
    ['Ros', 'Joost. What are you going to put it in?'],
    ['Joost', 'Something.']],
    options: ["Let Joost take the motor out (the mill's winch, then no motor)", 'Fly her on as she was sold (the motor stays in)']},
  {id: 'doors', title: 'Ferry Point', heavy: true, lines: [
    ['Ros', 'They took the hangar doors off this morning. For the scrap.'],
    ['Ros', "He's sitting on the floor in there, where she used to stand. He asked me to tell you something."],
    ['Ros', "'The yard is short and the sea breeze is on the nose. Height first.' That's all. That's all he said."]],
    note: "The last hop: the sea breeze against you, and the breaker's yard is a short strip of cinders.",
    options: ['Fly the last hop']},
  {id: 'coast', title: "The breaker's yard", endings: [
    ['The breaker walks round her twice and asks where the engine is. You hand him the papers. One motor glider.',
     'He pays half and takes the wings anyway.',
     'On the train home Ros sends a photo: Joost at the mill bench with the motor clamped in front of him and his reading glasses on. No caption.'],
    ["The breaker walks round her twice and has the cowling off before you've unstrapped.",
     "The club's debts are paid to the last cent.",
     "You walk to the station with her logbook under your arm. Nobody asked for it. Joost doesn't answer his phone that night, or the next."]],
    options: ['Start again']}
];
const HOPS = ['Upper Field to Weir Strip', 'Weir Strip to Orchard Strip', 'Orchard Strip to Mill Strip', 'Mill Strip to Ferry Point',
  "Ferry Point to the breaker's yard"];

// ------------------------------------------------------------------ state and modes
const read = () => ferry
  ? Object.fromEntries(ferryNames.map((name, i) => [name, engine._fw_value(i)]))
  : Object.fromEntries(names.map((name, i) => [name, engine._lp_value(i)]));
const done = () => ferry ? state && state.hop >= 5 : state && (state.status !== 0 || (state.mode === 2 && state.time >= 120));
const inScene = () => ferry && state && state.scene >= 0;
function setMode(isFerry) {
  ferry = isFerry;
  document.body.dataset.mode = ferry ? 'ferry' : 'lab';
}
function syncControls() {
  const cmd = ferry ? command : state.command, thr = ferry ? throttle : state.throttle;
  $('pitch').value = cmd;
  $('throttle').value = thr * 100;
  $('pitch-value').textContent = `${cmd.toFixed(1)}°`;
  const noMotor = ferry && (!state.motor || state.charge <= 0);
  $('throttle-value').textContent = ferry && !state.motor ? 'no motor' : `${Math.round(thr * 100)}%`;
  const locked = ferry ? inScene() || done() : state.mode !== 0 || done();
  $('pitch').disabled = locked;
  $('throttle').disabled = locked || noMotor;
  document.querySelectorAll('[data-adjust]').forEach(button => button.disabled = locked);
}
function reset(mode = 0, run = false) {
  setMode(false);
  engine._lp_reset(mode);
  state = read(); paused = !run; accumulator = 0;
  samples = [{...state}]; sampleTime = 0;
  $('story').hidden = true; shownScene = -2;
  if (run) $('welcome').hidden = true;
  syncControls(); render();
}
function startFerry() {
  setMode(true);
  engine._fw_reset();
  command = 4; throttle = 0; heavyLog.length = 0; shownScene = -2;
  state = read(); paused = true; accumulator = 0;
  samples = [{...state}]; sampleTime = 0;
  $('welcome').hidden = true;
  showScene(); syncControls(); render();
}
function controls(nextCommand, nextThrottle) {
  if (!engine) return;
  if (ferry) {
    command = Math.max(-10, Math.min(20, nextCommand));
    throttle = state.motor ? Math.max(0, Math.min(1, nextThrottle)) : 0;
    syncControls(); render(); return;
  }
  engine._lp_controls(nextCommand, nextThrottle);
  state = read(); syncControls(); render();
}
function toggle() {
  if (!engine) return;
  $('welcome').hidden = true;
  if (ferry) {
    if (done()) { startFerry(); return; }
    if (inScene()) return;
    paused = !paused; accumulator = 0; render(); return;
  }
  if (done()) { reset(0, true); return; }
  paused = !paused; accumulator = 0; render();
}

// ------------------------------------------------------------------ scenes
function showScene() {
  const box = $('story');
  const index = inScene() ? state.scene : -1;
  if (index === shownScene) return;
  shownScene = index;
  if (index < 0) { box.hidden = true; return; }
  const s = SCENES[index];
  box.hidden = false;
  box.className = s.heavy ? 'heavy' : s.endings ? 'ending' : '';
  box.dataset.scene = s.id;
  // The C ferry clears its banner when a heavy scene opens; a scene on screen always hides the banner here.
  if (s.heavy) heavyLog.push({id: s.id, banner: state.banner, loud: state.loud, bannerShown: !$('banner').hidden});
  $('banner').hidden = true;
  $('story-title').textContent = s.title;
  const text = $('story-text');
  text.replaceChildren();
  const lines = s.endings ? s.endings[state.ending === 0 ? 0 : 1].map(line => ['', line]) : s.lines;
  for (const [who, words] of lines) {
    const p = document.createElement('p');
    if (who) { const b = document.createElement('b'); b.textContent = who; p.append(b, ' '); }
    else p.className = 'narration';
    p.append(words);
    text.append(p);
  }
  if (s.note) { const p = document.createElement('p'); p.className = 'note'; p.textContent = s.note; text.append(p); }
  const options = $('story-options');
  options.replaceChildren();
  s.options.forEach((label, option) => {
    const button = document.createElement('button');
    button.textContent = label;
    if (!option) button.className = 'first';
    button.addEventListener('click', () => answer(option));
    options.append(button);
  });
  box.scrollTop = 0;
}
function answer(option) {
  if (!inScene()) return;
  if (SCENES[state.scene].id === 'coast') { startFerry(); return; }
  engine._fw_answer(option);
  state = read();
  if (!state.motor) throttle = 0;
  if (state.scene < 0) { samples = [{...state}]; sampleTime = state.time; paused = false; accumulator = 0; }
  showScene(); syncControls(); render();
}

// ------------------------------------------------------------------ input
$('toggle').addEventListener('click', toggle);
$('reset').addEventListener('click', () => ferry ? startFerry() : reset());
$('glide').addEventListener('click', () => reset(1, true));
$('phugoid').addEventListener('click', () => reset(2, true));
$('ferry').addEventListener('click', startFerry);
$('welcome-start').addEventListener('click', startFerry);
$('welcome-lab').addEventListener('click', () => { $('welcome').hidden = true; render(); });
$('pitch').addEventListener('input', event => controls(Number(event.target.value), ferry ? throttle : state.throttle));
$('throttle').addEventListener('input', event => controls(ferry ? command : state.command, Number(event.target.value) / 100));
document.querySelectorAll('[data-adjust]').forEach(button => {
  button.addEventListener('click', () => controls((ferry ? command : state.command) + Number(button.dataset.adjust), ferry ? throttle : state.throttle));
});
document.addEventListener('keydown', event => {
  if (!engine || event.ctrlKey || event.metaKey || event.altKey || event.target.matches('input,textarea,select')) return;
  const key = event.key.toLowerCase();
  if (!['w','s','r','f','p'].includes(key)) return;
  event.preventDefault();
  if (key === 'p') { if (!event.repeat) toggle(); return; }
  if (paused || done() || inScene() || (!ferry && state.mode)) return;
  const cmd = ferry ? command : state.command, thr = ferry ? throttle : state.throttle;
  controls(cmd + (key === 'w' ? 1 : key === 's' ? -1 : 0), thr + (key === 'r' ? .05 : key === 'f' ? -.05 : 0));
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden) { paused = true; accumulator = 0; if (engine) render(); }
});
window.addEventListener('blur', () => { paused = true; accumulator = 0; if (engine) render(); });

// ------------------------------------------------------------------ drawing
function canvasSize(canvas) {
  const bounds = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const w = Math.round(bounds.width * dpr), h = Math.round(bounds.height * dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return {ctx, w: bounds.width, h: bounds.height};
}
function cover(ctx, img, w, h) {
  const scale = Math.max(w / img.naturalWidth, h / img.naturalHeight);
  ctx.drawImage(img, (w - img.naturalWidth * scale) / 2, (h - img.naturalHeight * scale) / 2, img.naturalWidth * scale, img.naturalHeight * scale);
}
function drawScene() {
  const {ctx, w, h} = canvasSize(scene);
  ctx.clearRect(0, 0, w, h);
  if (sky.complete && sky.naturalWidth) cover(ctx, sky, w, h);
  else { ctx.fillStyle = '#c9dde0'; ctx.fillRect(0, 0, w, h); }
  // Heavy scenes get a still, darkened view.
  if (inScene() && SCENES[state.scene].heavy) { ctx.fillStyle = '#0e1416cc'; ctx.fillRect(0, 0, w, h); }
  const ground = h * .85;
  // Illustration is intentionally separated from the quantitative chart below.
  ctx.strokeStyle = '#3b5f51b0'; ctx.lineWidth = 1; ctx.setLineDash([5, 6]);
  ctx.beginPath(); ctx.moveTo(0, ground); ctx.lineTo(w, ground); ctx.stroke(); ctx.setLineDash([]);
  const px = w * .38, maxH = ferry ? 200 : state.mode === 2 ? 3400 : 180;
  const py = ground - Math.min(state.h / maxH, 1) * h * .56;
  const scale = w / 1500;
  const runway = ferry ? state.strip : state.runway, length = ferry ? state.length : 300;
  const runwayX = px + (runway - state.x) * scale;
  ctx.font = '15px system-ui';
  if ((ferry || state.mode !== 2) && runwayX < w && runwayX + length * scale > 0) {
    ctx.strokeStyle = '#8a591d'; ctx.lineWidth = 5;
    ctx.beginPath(); ctx.moveTo(runwayX, ground); ctx.lineTo(runwayX + length * scale, ground); ctx.stroke();
    ctx.fillStyle = '#203b3b';
    ctx.fillText(ferry ? `STRIP · ${length.toFixed(0)} m` : 'RUNWAY · 300 m', Math.max(8, Math.min(w - 130, runwayX)), ground + 20);
  } else if (ferry && runwayX >= w) {
    ctx.fillStyle = '#203b3b';
    ctx.fillText(`STRIP ${((runway - state.x) / 1000).toFixed(1)} km →`, w - 140, ground + 20);
  }
  const spriteW = Math.min(w * .38, 310), spriteH = spriteW * glider.naturalHeight / glider.naturalWidth;
  ctx.save(); ctx.translate(px, py); ctx.rotate(-(state.gamma + state.alpha));
  if (glider.complete && glider.naturalWidth) ctx.drawImage(glider, -spriteW / 2, -spriteH * .52, spriteW, spriteH);
  else {
    ctx.fillStyle = '#244c48'; ctx.beginPath(); ctx.arc(0, 0, 4, 0, 2 * Math.PI); ctx.fill();
    ctx.fillText('Aircraft · art unavailable', 10, 0);
  }
  ctx.restore();
  // Flight-path direction differs from the airframe attitude; label the distinction.
  const len = 52, vx = Math.cos(state.gamma) * len, vy = -Math.sin(state.gamma) * len;
  ctx.strokeStyle = '#254b48'; ctx.lineWidth = 1.5;
  ctx.beginPath(); ctx.moveTo(px + spriteW * .38, py + 15); ctx.lineTo(px + spriteW * .38 + vx, py + 15 + vy); ctx.stroke();
  ctx.fillStyle = '#244b48'; ctx.fillText('V', px + spriteW * .38 + vx + 5, py + 19 + vy);
}
function drawPlot() {
  const {ctx, w, h} = canvasSize(plot), left = 52, right = 14, top = 26, bottom = 38;
  const phugoid = !ferry && state.mode === 2;
  const minY = phugoid ? Math.floor(Math.min(...samples.map(s => s.v)) - 1) : 0;
  const maxY = phugoid ? Math.ceil(Math.max(...samples.map(s => s.v)) + 1)
    : Math.max(ferry ? 150 : 120, Math.ceil(Math.max(...samples.map(s => s.h)) / 50) * 50);
  const runway = ferry ? state.strip : state.runway, length = ferry ? state.length : 300;
  const maxX = phugoid ? 120 : Math.max(runway + length + 100, state.x + 200);
  const x = value => left + value / maxX * (w - left - right);
  const y = value => top + (maxY - value) / (maxY - minY) * (h - top - bottom);
  ctx.clearRect(0, 0, w, h); ctx.font = '15px system-ui'; ctx.textAlign = 'right';
  for (let i = 0; i <= 3; i++) {
    const value = minY + (maxY - minY) * i / 3, yy = y(value);
    ctx.strokeStyle = '#dfe5dd'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(left, yy); ctx.lineTo(w - right, yy); ctx.stroke();
    ctx.fillStyle = '#4f6560'; ctx.fillText(value.toFixed(phugoid ? 1 : 0), left - 6, yy + 4);
  }
  ctx.textAlign = 'center';
  const ticks = w < 500 ? 2 : 4;
  for (let i = 0; i <= ticks; i++) {
    const value = maxX * i / ticks, xx = x(value);
    ctx.fillStyle = '#4f6560'; ctx.fillText(value.toFixed(0), Math.max(left, Math.min(w - 30, xx)), h - bottom + 17);
  }
  if (!phugoid) { ctx.fillStyle = '#b8863b88'; ctx.fillRect(x(runway), h - bottom - 4, length / maxX * (w - left - right), 4); }
  if (ferry && state.scene < 0 && state.hop < 5 && state.x < state.strip && state.making > 1) {
    // The glide being made, drawn back from the aim point: below the dashed line the strip is out of reach.
    const aim = state.strip + 80, back = Math.max(0, aim - (maxY - minY) * state.making);
    ctx.setLineDash([6, 5]); ctx.strokeStyle = state.lowOn ? '#8a4b12' : '#8aa29c'; ctx.lineWidth = 1.5;
    ctx.beginPath(); ctx.moveTo(x(aim), y(0)); ctx.lineTo(x(back), y((aim - back) / state.making)); ctx.stroke(); ctx.setLineDash([]);
  }
  ctx.textAlign = 'left'; ctx.fillStyle = '#3c5a53'; ctx.fillText(phugoid ? 'V [m/s]' : 'h [m]', 5, top - 10);
  ctx.textAlign = 'right'; ctx.fillText(phugoid ? 'Time t [s]' : 'Distance x [m]', w - right, h - 4);
  ctx.strokeStyle = '#336960'; ctx.lineWidth = 2; ctx.beginPath();
  samples.forEach((s, i) => { const xx = x(phugoid ? s.time : s.x), yy = y(phugoid ? s.v : s.h); i ? ctx.lineTo(xx, yy) : ctx.moveTo(xx, yy); });
  ctx.stroke();
  ctx.fillStyle = '#336960'; ctx.beginPath(); ctx.arc(x(phugoid ? state.time : state.x), y(phugoid ? state.v : state.h), 3, 0, 2 * Math.PI); ctx.fill();
  $('chart-title').textContent = ferry ? 'This hop, measured' : phugoid ? 'Speed becomes height' : 'The path you fly';
  $('chart-legend').lastChild.textContent = phugoid ? ' Airspeed V [m/s]' : ' Altitude h [m]';
  plot.setAttribute('aria-label', phugoid ? 'Measured airspeed in metres per second versus time in seconds' : 'Measured altitude in metres versus distance in metres');
  $('plot-note').textContent = phugoid ? '120 s observation · +10% initial speed · solid line = measured airspeed'
    : ferry ? 'Ground distance in this hop · strip = ochre band · dashed = the glide you are making, back from the aim point'
    : 'Independent axis scales · runway = ochre band · past flight = solid line';
}

// ------------------------------------------------------------------ readouts
function renderFerry() {
  const hop = Math.min(state.hop, 4), arrived = state.hop >= 5;
  $('mode-name').textContent = 'THE LAST FERRY';
  $('controls-eyebrow').textContent = 'THE LAST FERRY';
  $('controls-title').innerHTML = 'Five hops.<br>Height first.';
  $('hop-name').textContent = arrived ? 'ARRIVED' : `HOP ${hop + 1} / 5 · ${HOPS[hop]}`;
  const air = [];
  if (state.wind < 0) air.push(`wind ${(-state.wind).toFixed(0)} m/s on the nose`);
  if (state.airLift < 0) air.push(`air sinking ${(-state.airLift).toFixed(2)} m/s`);
  $('hop-air').textContent = arrived ? '' : `Strip ${state.length.toFixed(0)} m long · ${air.length ? air.join(' · ') : 'still air'}` +
    (state.motor ? '' : ' · no motor');
  $('hop-tally').textContent = `Outlandings ${state.outlandings}` + (state.outHop ? ` (this hop ${state.outHop} of 3; the third goes by trailer)` : '');
  const toStrip = Math.max(0, state.strip - state.x);
  $('f-strip').textContent = (toStrip / 1000).toFixed(2);
  const over = state.x >= state.strip;
  $('f-glide').textContent = over ? 'over the strip' : `1:${state.need.toFixed(0)} / 1:${Math.min(state.making, 60).toFixed(0)}`;
  $('glide-cell').dataset.low = String(Boolean(state.lowOn));
  $('f-charge').textContent = state.motor ? state.charge.toFixed(0) : '—';
  $('f-charge-unit').textContent = state.motor ? 's of motor' : 'no motor';
  $('f-height').textContent = state.h.toFixed(0);
  $('f-speed').textContent = `m · ${state.v.toFixed(1)} m/s`;
  let label = paused ? (inScene() ? 'On the ground' : 'Paused') : state.throttle > 0 ? 'Motor running' : 'Gliding';
  if (arrived) label = "The breaker's yard";
  $('status').textContent = label;
  $('scene-caption').textContent = arrived ? 'The ferry is over' : `${HOPS[hop]}`;
  $('view-corner').textContent = state.wind < 0 ? `HEADWIND ${(-state.wind).toFixed(0)} m/s` : 'NO WIND';
  $('low').hidden = !state.lowOn || inScene() || done();
  const banner = $('banner');
  banner.hidden = !state.banner || inScene();
  banner.textContent = state.banner === 1 ? 'STRIP MADE' : state.trailered && state.outHop === 0 ? 'OUTLANDED THREE TIMES · ON BY TRAILER' : 'OUTLANDED · BACK TO THE START OF THE HOP';
  banner.dataset.kind = state.banner === 1 ? 'clear' : 'out';
  $('toggle').innerHTML = arrived ? 'Fly it again <span aria-hidden="true">↗</span>' : paused ? 'Resume flight <span aria-hidden="true">↗</span>' : 'Pause flight Ⅱ';
  $('control-note').textContent = state.motor
    ? 'The motor climbs at the best-glide angle while the battery lasts. Height first: it is easier to lose than to find.'
    : 'No motor now. A cleaner wing, and nothing to fix a mistake with.';
  $('experiment-note').textContent = 'The flight laboratory and its experiments are still here; choosing one leaves the ferry.';
}
function render() {
  if (ferry) renderFerry();
  else {
    $('mode-name').textContent = modeNames[state.mode];
    $('controls-eyebrow').textContent = 'YOUR CONTROLS';
    $('controls-title').innerHTML = 'Small inputs.<br>Watch what follows.';
    $('scene-caption').textContent = 'Illustrated landscape · schematic ground line';
    $('view-corner').textContent = 'STILL AIR · LONGITUDINAL FLIGHT';
    $('low').hidden = true; $('banner').hidden = true;
    let label = paused ? (state.time ? 'Flight paused' : 'Ready for approach') : (state.throttle > 0 ? 'Powered flight' : 'Gliding');
    if (state.status) label = state.status === 2 ? 'Hard landing — reset to try again' : state.onRunway ? 'Gentle landing · on runway' : 'Gentle touchdown · off runway';
    else if (done()) label = 'Observation complete · 120 s';
    $('status').textContent = label;
    $('toggle').innerHTML = done() ? 'Fly again <span aria-hidden="true">↗</span>' : paused ? `${state.time ? 'Resume flight' : 'Start flying'} <span aria-hidden="true">↗</span>` : 'Pause flight Ⅱ';
    $('experiment-note').textContent = notes[state.mode];
    $('control-note').textContent = 'Pitch commands the angle of attack, not the flight path. Give the aircraft time to respond.';
  }
  $('clock').textContent = `${String(Math.floor(state.time / 60)).padStart(2, '0')}:${(state.time % 60).toFixed(1).padStart(4, '0')}`;
  $('airspeed').textContent = state.v.toFixed(1); $('altitude').textContent = state.h.toFixed(1);
  $('sink').textContent = state.sink.toFixed(2); $('ld').textContent = state.ld.toFixed(1);
  $('lift').textContent = `${state.lift.toFixed(0)} N`; $('drag').textContent = `${state.drag.toFixed(0)} N`;
  $('alpha').textContent = `${(state.alpha * radToDeg).toFixed(1)}°`; $('gamma').textContent = `${(state.gamma * radToDeg).toFixed(1)}°`;
  $('stall').hidden = !state.stall || inScene();
  $('toggle').setAttribute('aria-pressed', String(!paused));
  $('toggle').disabled = inScene();
  syncControls(); drawScene(); drawPlot();
  // Read-only observation; tests advance the real UI clock and exercise the C exports.
  window.liftView = {snapshot: {...state, paused, ferry, artReady: Boolean(sky.naturalWidth && glider.naturalWidth), samples: samples.length,
    sceneId: inScene() ? SCENES[state.scene].id : null, sceneText: $('story').hidden ? '' : $('story-text').textContent,
    bannerShown: !$('banner').hidden, bannerText: $('banner').textContent, heavyLog: heavyLog.map(h => ({...h})),
    welcome: !$('welcome').hidden, inputCommand: command, inputThrottle: throttle}, engine};
}
function frame(timestamp) {
  if (!last) last = timestamp;
  const dt = Math.min((timestamp - last) / 1000, .25); last = timestamp;
  if (engine && !paused && !done() && !inScene()) {
    accumulator += dt;
    while (accumulator >= .02 && !done() && !inScene()) {
      if (ferry) {
        const outs = state.outlandings, hop = state.hop;
        engine._fw_tick(1, command, throttle); state = read();
        if (state.outlandings !== outs || state.hop !== hop) { samples = [{...state}]; sampleTime = state.time; }
      } else { engine._lp_step(1); state = read(); }
      accumulator -= .02;
      if (state.time - sampleTime >= .2 || done()) {
        samples.push({...state}); sampleTime = state.time;
        if (samples.length > 3000) samples.splice(0, 1);
      }
    }
    if (inScene()) { paused = true; showScene(); if (!state.motor) throttle = 0; }
    if (done()) paused = true;
    render();
  }
  requestAnimationFrame(frame);
}
for (const img of [sky, glider]) img.onload = () => { if (engine) render(); };
new ResizeObserver(() => { if (engine) render(); }).observe(scene);
new ResizeObserver(() => { if (engine) drawPlot(); }).observe(plot);
$('pitch').disabled = $('throttle').disabled = true;
document.querySelectorAll('[data-adjust]').forEach(button => button.disabled = true);

// Common test hook of the playability gate (TM-59 project-context, rules/playability.json). Read-only.
// Logical units are the scene canvas's CSS pixels. DOM words and buttons are measured by the gate directly.
function fromCanvas(el) {
  if (!el || el.hidden || !el.getClientRects().length) return null;
  const c = scene.getBoundingClientRect(), r = el.getBoundingClientRect();
  return {x: r.left + r.width / 2 - c.left, y: r.top + r.height / 2 - c.top};
}
window.__tmGate = {
  version: 1,
  get logical() { const r = scene.getBoundingClientRect(); return {w: r.width || 1, h: r.height || 1}; },
  scene: () => !state ? 'loading' : !$('welcome').hidden ? 'title' : inScene() ? `scene:${SCENES[state.scene].id}` : done() ? 'done' : paused ? 'paused' : 'play',
  // The first goal: the ferry's first scene answered (Joost's hangar closed, the Kestrel airborne on the first hop).
  goal: () => Boolean(ferry && state && (state.hop > 0 || (state.scene < 0 && state.charge > 5))),
  objective: () => ferry ? $('hop-name').textContent : $('status').textContent,
  texts: () => [],
  targets: () => [],
  // A longitudinal flight: there is no screen direction to steer.
  player: () => null,
  // Where a player who reads the screen would tap next: the welcome card, the open scene's first answer, or Resume.
  hint: () => {
    if (!state) return null;
    if (!$('welcome').hidden) return fromCanvas($('welcome-start'));
    if (inScene()) return fromCanvas($('story-options').firstChild);
    if (paused && !done()) return fromCanvas($('toggle'));
    return null;
  },
};
try {
  engine = await createFlight();
  for (const id of ['toggle', 'reset', 'glide', 'phugoid', 'ferry', 'welcome-start', 'welcome-lab']) $(id).disabled = false;
  $('welcome').hidden = false;
  reset(); requestAnimationFrame(frame);
} catch {
  $('status').textContent = 'Flight model unavailable — reload to retry';
  $('experiment-note').textContent = 'The flight model could not load. Serve the built files over HTTP with WebAssembly enabled.';
}
