import createFlight from './physics.mjs';

const $ = id => document.getElementById(id);
const names = ['x','h','v','gamma','alpha','time','command','throttle','lift','drag','ld','sink','energy','status','runway','onRunway','mode','stall'];
const scene = $('scene'), plot = $('plot');
const sky = new Image(), glider = new Image();
sky.src = 'art/airfield.jpg'; glider.src = 'art/glider-sprite.png';
let engine, state, paused = true, last = 0, accumulator = 0, samples = [], sampleTime = -1;
const radToDeg = 180 / Math.PI;
const modeNames = ['MANUAL APPROACH','TRIMMED LANDING','SPEED ↔ HEIGHT'];
const notes = [
  'An unpowered approach from 100 m. Land with sink below 2 m/s and a shallow path angle.',
  'Fixed 4° angle of attack, throttle off. The runway is placed from a dry run of the same model. Controls are locked during the demonstration.',
  'A trimmed glide at 3,000 m with 10% extra airspeed. Watch a 120 s phugoid experiment. Controls are locked; there is no autopilot in manual flight.'
];
const read = () => Object.fromEntries(names.map((name,i) => [name,engine._lp_value(i)]));
const done = () => state && (state.status !== 0 || (state.mode === 2 && state.time >= 120));
function syncControls() {
  $('pitch').value = state.command;
  $('throttle').value = state.throttle * 100;
  $('pitch-value').textContent = `${state.command.toFixed(1)}°`;
  $('throttle-value').textContent = `${Math.round(state.throttle*100)}%`;
  const locked = state.mode !== 0 || done();
  $('pitch').disabled = $('throttle').disabled = locked;
  document.querySelectorAll('[data-adjust]').forEach(button => button.disabled = locked);
}
function reset(mode = 0, run = false) {
  engine._lp_reset(mode);
  state = read(); paused = !run; accumulator = 0;
  samples = [{...state}]; sampleTime = 0;
  syncControls(); render();
}
function controls(command, throttle) {
  if (!engine) return;
  engine._lp_controls(command, throttle);
  state = read(); syncControls(); render();
}
function toggle() {
  if (!engine) return;
  if (done()) { reset(0,true); return; }
  paused = !paused; accumulator = 0; render();
}
$('toggle').onclick = toggle;
$('reset').onclick = () => reset();
$('glide').onclick = () => reset(1,true);
$('phugoid').onclick = () => reset(2,true);
$('pitch').oninput = event => controls(Number(event.target.value), state.throttle);
$('throttle').oninput = event => controls(state.command,Number(event.target.value)/100);
document.querySelectorAll('[data-adjust]').forEach(button => {
  button.onclick = () => controls(state.command+Number(button.dataset.adjust),state.throttle);
});
document.addEventListener('keydown',event => {
  if (!engine || event.ctrlKey || event.metaKey || event.altKey || event.target.matches('input,textarea,select')) return;
  const key = event.key.toLowerCase();
  if (!['w','s','r','f','p'].includes(key)) return;
  event.preventDefault();
  if (key === 'p') { if (!event.repeat) toggle(); return; }
  if (paused || done() || state.mode) return;
  controls(state.command+(key === 'w' ? 1 : key === 's' ? -1 : 0),
    state.throttle+(key === 'r' ? .05 : key === 'f' ? -.05 : 0));
});
document.addEventListener('visibilitychange',() => {
  if (document.hidden) { paused = true; accumulator = 0; if (engine) render(); }
});
window.addEventListener('blur',() => { paused = true; accumulator = 0; if (engine) render(); });

function canvasSize(canvas) {
  const bounds = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1,2);
  const w = Math.round(bounds.width*dpr), h = Math.round(bounds.height*dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  const ctx = canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0);
  return {ctx,w:bounds.width,h:bounds.height};
}
function cover(ctx,img,w,h) {
  const scale = Math.max(w/img.naturalWidth,h/img.naturalHeight);
  ctx.drawImage(img,(w-img.naturalWidth*scale)/2,(h-img.naturalHeight*scale)/2,img.naturalWidth*scale,img.naturalHeight*scale);
}
function drawScene() {
  const {ctx,w,h} = canvasSize(scene);
  ctx.clearRect(0,0,w,h);
  if (sky.complete && sky.naturalWidth) cover(ctx,sky,w,h);
  else {ctx.fillStyle='#c9dde0';ctx.fillRect(0,0,w,h);}
  const ground = h*.85;
  // Illustration is intentionally separated from the quantitative chart below.
  ctx.strokeStyle='#3b5f51b0';ctx.lineWidth=1;ctx.setLineDash([5,6]);
  ctx.beginPath();ctx.moveTo(0,ground);ctx.lineTo(w,ground);ctx.stroke();ctx.setLineDash([]);
  const px=w*.38, maxH=state.mode===2?3400:180;
  const py=ground-Math.min(state.h/maxH,1)*h*.56;
  const scale=w/1500, runwayX=px+(state.runway-state.x)*scale;
  if (state.mode!==2 && runwayX<w && runwayX+300*scale>0) {
    ctx.strokeStyle='#8a591d';ctx.lineWidth=5;
    ctx.beginPath();ctx.moveTo(runwayX,ground);ctx.lineTo(runwayX+300*scale,ground);ctx.stroke();
    ctx.fillStyle='#203b3b';ctx.font='10px system-ui';ctx.fillText('RUNWAY · 300 m',Math.max(8,runwayX),ground+20);
  }
  const spriteW=Math.min(w*.38,310), spriteH=spriteW*glider.naturalHeight/glider.naturalWidth;
  ctx.save();ctx.translate(px,py);ctx.rotate(-(state.gamma+state.alpha));
  if (glider.complete && glider.naturalWidth) ctx.drawImage(glider,-spriteW/2,-spriteH*.52,spriteW,spriteH);
  else {
    ctx.fillStyle='#244c48';ctx.beginPath();ctx.arc(0,0,4,0,2*Math.PI);ctx.fill();
    ctx.font='11px system-ui';ctx.fillText('Aircraft · art unavailable',10,0);
  }
  ctx.restore();
  // Flight-path direction differs from the airframe attitude; label the distinction.
  const len=52, vx=Math.cos(state.gamma)*len, vy=-Math.sin(state.gamma)*len;
  ctx.strokeStyle='#254b48';ctx.lineWidth=1.5;
  ctx.beginPath();ctx.moveTo(px+spriteW*.38,py+15);ctx.lineTo(px+spriteW*.38+vx,py+15+vy);ctx.stroke();
  ctx.fillStyle='#244b48';ctx.font='10px system-ui';ctx.fillText('V',px+spriteW*.38+vx+5,py+19+vy);
}
function drawPlot() {
  const {ctx,w,h}=canvasSize(plot), left=48,right=14,top=24,bottom=34;
  const phugoid=state.mode===2;
  const minY=phugoid?Math.floor(Math.min(...samples.map(s=>s.v))-1):0;
  const maxY=phugoid?Math.ceil(Math.max(...samples.map(s=>s.v))+1):Math.max(120,Math.ceil(Math.max(...samples.map(s=>s.h))/50)*50);
  const maxX=phugoid?120:Math.max(state.runway+400,state.x+200);
  const x=value=>left+value/maxX*(w-left-right);
  const y=value=>top+(maxY-value)/(maxY-minY)*(h-top-bottom);
  ctx.clearRect(0,0,w,h);ctx.font='9px system-ui';ctx.textAlign='right';
  for(let i=0;i<=3;i++) {
    const value=minY+(maxY-minY)*i/3, yy=y(value);
    ctx.strokeStyle='#dfe5dd';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(left,yy);ctx.lineTo(w-right,yy);ctx.stroke();
    ctx.fillStyle='#536b64';ctx.fillText(value.toFixed(phugoid?1:0),left-8,yy+3);
  }
  ctx.textAlign='center';
  for(let i=0;i<=4;i++) {
    const value=maxX*i/4, xx=x(value);
    ctx.fillStyle='#536b64';ctx.fillText(value.toFixed(0),xx,h-bottom+15);
  }
  if(!phugoid) {
    ctx.fillStyle='#b8863b88';ctx.fillRect(x(state.runway),h-bottom-4,300/maxX*(w-left-right),4);
  }
  ctx.textAlign='left';ctx.fillStyle='#3c5a53';ctx.fillText(phugoid?'V [m/s]':'h [m]',5,top-9);
  ctx.textAlign='right';ctx.fillText(phugoid?'Time t [s]':'Distance x [m]',w-right,h-2);
  ctx.strokeStyle='#336960';ctx.lineWidth=2;ctx.beginPath();
  samples.forEach((s,i)=>{const xx=x(phugoid?s.time:s.x),yy=y(phugoid?s.v:s.h);i?ctx.lineTo(xx,yy):ctx.moveTo(xx,yy)});ctx.stroke();
  ctx.fillStyle='#336960';ctx.beginPath();ctx.arc(x(phugoid?state.time:state.x),y(phugoid?state.v:state.h),3,0,2*Math.PI);ctx.fill();
  $('chart-title').textContent=phugoid?'Speed becomes height':'The path you fly';
  $('chart-legend').lastChild.textContent=phugoid?' Airspeed V [m/s]':' Altitude h [m]';
  plot.setAttribute('aria-label',phugoid?'Measured airspeed in metres per second versus time in seconds':'Measured altitude in metres versus distance in metres');
  $('plot-note').textContent=phugoid?'120 s observation · +10% initial speed · solid line = measured airspeed':'Independent axis scales · runway = ochre band · past flight = solid line';
}
function render() {
  $('mode-name').textContent=modeNames[state.mode];
  $('clock').textContent=`${String(Math.floor(state.time/60)).padStart(2,'0')}:${(state.time%60).toFixed(1).padStart(4,'0')}`;
  $('airspeed').textContent=state.v.toFixed(1);$('altitude').textContent=state.h.toFixed(1);
  $('sink').textContent=state.sink.toFixed(2);$('ld').textContent=state.ld.toFixed(1);
  $('lift').textContent=`${state.lift.toFixed(0)} N`;$('drag').textContent=`${state.drag.toFixed(0)} N`;
  $('alpha').textContent=`${(state.alpha*radToDeg).toFixed(1)}°`;$('gamma').textContent=`${(state.gamma*radToDeg).toFixed(1)}°`;
  $('stall').hidden=!state.stall;
  let label=paused?(state.time?'Flight paused':'Ready for approach'):(state.throttle>0?'Powered flight':'Gliding');
  if(state.status) label=state.status===2?'Hard landing — reset to try again':state.onRunway?'Gentle landing · on runway':'Gentle touchdown · off runway';
  else if(done()) label='Observation complete · 120 s';
  $('status').textContent=label;
  $('toggle').textContent=done()?'Fly again ↗':paused?(state.time?'Resume flight ↗':'Start flying ↗'):'Pause flight Ⅱ';
  $('toggle').setAttribute('aria-pressed',String(!paused));
  $('experiment-note').textContent=notes[state.mode];
  syncControls();drawScene();drawPlot();
  // Read-only observation; tests advance the real UI clock and exercise the C exports.
  window.liftView={snapshot:{...state,paused,artReady:Boolean(sky.naturalWidth&&glider.naturalWidth),samples:samples.length},engine};
}
function frame(timestamp) {
  if(!last) last=timestamp;
  const dt=Math.min((timestamp-last)/1000,.25);last=timestamp;
  if(engine&&!paused&&!done()) {
    accumulator+=dt;
    while(accumulator>=.02&&!done()) {
      engine._lp_step(1);state=read();accumulator-=.02;
      if(state.time-sampleTime>=.2||done()) {
        samples.push({...state});sampleTime=state.time;
        if(samples.length>3000)samples.splice(0,1);
      }
    }
    if(done()) paused=true;
    render();
  }
  requestAnimationFrame(frame);
}
for(const img of [sky,glider]) img.onload=()=>{if(engine)render()};
new ResizeObserver(()=>{if(engine)render()}).observe(scene);
new ResizeObserver(()=>{if(engine)drawPlot()}).observe(plot);
$('pitch').disabled=$('throttle').disabled=true;
document.querySelectorAll('[data-adjust]').forEach(button=>button.disabled=true);
try {
  engine=await createFlight();
  for(const id of ['toggle','reset','glide','phugoid'])$(id).disabled=false;
  reset();requestAnimationFrame(frame);
} catch {
  $('status').textContent='Flight model unavailable — reload to retry';
  $('experiment-note').textContent='The flight model could not load. Serve the built files over HTTP with WebAssembly enabled.';
}
