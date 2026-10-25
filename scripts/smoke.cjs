/* Local verification: native/WASM comparison, real controls and actual screenshots. */
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {execFileSync} = require('node:child_process');
const root = path.resolve('dist');
const mime = {'.html':'text/html','.mjs':'text/javascript','.wasm':'application/wasm','.css':'text/css','.png':'image/png','.jpg':'image/jpeg'};
const server = http.createServer((req,res) => {
  const file=path.resolve(root,'.'+decodeURIComponent(new URL(req.url,'http://localhost').pathname));
  const target=file===root?path.join(root,'index.html'):file;
  if(!target.startsWith(root+path.sep)){res.writeHead(403).end();return;}
  fs.readFile(target,(error,data)=>{
    if(error)res.writeHead(404).end();
    else{res.setHeader('Content-Type',mime[path.extname(target)]||'application/octet-stream');res.end(data);}
  });
});
let browser,checks=0;
const check=(value,message)=>{assert.ok(value,message);checks++};
const snapshot=page=>page.evaluate(()=>window.liftView.snapshot);
async function capture(page,name){await page.screenshot({path:`docs/images/${name}.jpg`,type:'jpeg',quality:88,fullPage:true})}
async function ready(page,url){await page.goto(url);await page.waitForFunction(()=>window.liftView?.snapshot?.artReady);await page.clock.install()}

async function compareWasm() {
  const createFlight=(await import(pathToFileURL(path.resolve('dist/physics.mjs')).href)).default;
  const engine=await createFlight({wasmBinary:fs.readFileSync('dist/physics.wasm')});
  const rows=fs.readFileSync('build/native-trace.csv','utf8').trim().split('\n').slice(1).map(line=>line.split(',').map(Number));
  let row=0,maxError=0;
  for(let mode=0;mode<3;mode++) {
    engine._lp_reset(mode);
    for(let step=0;step<=6000;step++) {
      if(step%20===0) {
        const native=rows[row++];
        assert.equal(native[0],mode);assert.equal(native[1],step);
        for(let field=0;field<native.length-2;field++) {
          const error=Math.abs(engine._lp_value(field)-native[field+2]);
          maxError=Math.max(error,maxError);
          assert.ok(Number.isFinite(error)&&error<1e-8,`WASM/native mode=${mode} step=${step} field=${field}: ${error}`);
        }
      }
      if(mode===0&&step===100)engine._lp_controls(7,.2);
      if(mode===0&&step===400)engine._lp_controls(3,0);
      if(mode===0&&step===800)engine._lp_controls(4,0);
      engine._lp_step(1);
    }
  }
  check(row===rows.length,'all native trajectory samples checked');
  return {rows:row,fields:rows[0].length-2,maxError,tolerance:1e-8};
}

async function run() {
  const started=performance.now(),equivalence=await compareWasm();
  fs.mkdirSync('docs/images',{recursive:true});
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve)});
  const origin=`http://127.0.0.1:${server.address().port}`;
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  const errors=[],external=[];
  const context=await browser.newContext({viewport:{width:1440,height:1050},deviceScaleFactor:1});
  const track=page=>{
    page.on('pageerror',error=>errors.push(error.message));
    page.on('console',msg=>{if(msg.type()==='error')errors.push(msg.text())});
    page.on('request',req=>{if(!req.url().startsWith(origin)&&!req.url().startsWith('data:'))external.push(req.url())});
  };
  const before=await context.newPage();
  const terminal=execFileSync('./lift-pilot',['--snapshot'],{encoding:'utf8'});
  await before.setContent('<html lang="en"><body style="margin:0;background:#0c1220;color:#dce8e9;padding:40px"><p style="font:16px monospace">Original terminal output / ./lift-pilot --snapshot</p><pre style="font:16px/1.45 monospace"></pre></body></html>');
  await before.locator('pre').evaluate((el,text)=>el.textContent=text,terminal);
  await capture(before,'before-terminal');await before.close();
  const page=await context.newPage();track(page);await ready(page,origin+'/');
  check((await snapshot(page)).paused,'starts paused');await capture(page,'observatory');
  const initial=await snapshot(page);
  await page.getByRole('button',{name:'Start flying'}).click();
  await page.keyboard.press('w');await page.keyboard.press('r');await page.clock.runFor(1500);
  const manual=await snapshot(page);
  check(Math.abs(manual.command-5)<1e-10&&Math.abs(manual.throttle-.05)<1e-10,'keyboard reaches C controls');
  check(manual.time>1&&manual.h!==initial.h,'manual flight advances');
  await page.keyboard.press('p');const paused=await snapshot(page);await page.clock.runFor(500);
  check((await snapshot(page)).time===paused.time,'pause freezes C state');await capture(page,'manual-flight');
  await page.getByRole('button',{name:'Trimmed landing',exact:true}).click();
  await page.clock.runFor(94000);
  const landing=await snapshot(page);
  check(landing.status===1&&landing.onRunway===1,'UI demo lands gently on runway');
  check(landing.sink<2&&Math.abs(landing.gamma)<.15,'landing matches C thresholds');
  check(landing.paused,'touchdown stops simulation');await capture(page,'landing');
  await page.getByRole('button',{name:'Speed ↔ height',exact:true}).click();await page.clock.runFor(30000);
  const phugoid=await snapshot(page);
  check(phugoid.mode===2&&phugoid.samples>100,'phugoid has measured history');
  check(await page.locator('#pitch').isDisabled(),'demo locks manual controls');
  await page.getByRole('button',{name:'Pause flight'}).click();await capture(page,'phugoid');
  await page.getByRole('button',{name:'Resume flight'}).click();await page.clock.runFor(91000);
  check((await snapshot(page)).paused&&(await snapshot(page)).time>=120,'phugoid stops at observation limit');
  check(await page.getByText('Observation complete · 120 s').isVisible(),'completed observation is clearly labelled');
  await page.getByRole('button',{name:'Reset flight',exact:true}).click();
  check((await snapshot(page)).mode===0&&(await snapshot(page)).time===0,'reset clears mode and time');
  await page.locator('#pitch').fill('20');await page.getByRole('button',{name:'Start flying'}).click();await page.clock.runFor(1300);
  check((await snapshot(page)).stall===1&&await page.locator('#stall').isVisible(),'real C stall is visible');
  await page.keyboard.press('p');await capture(page,'stall');
  check(await page.locator('body').evaluate(el=>el.scrollWidth<=innerWidth),'desktop has no horizontal overflow');
  await page.getByRole('button',{name:'Reset flight',exact:true}).click();
  await page.locator('#pitch').fill('-10');await page.getByRole('button',{name:'Start flying'}).click();await page.clock.runFor(30000);
  const crash=await snapshot(page);
  check(crash.status===2&&crash.paused,'hard landing is distinguished from a gentle landing');
  await page.getByRole('link',{name:'How it flies'}).click();
  check(page.url().endsWith('#field-notes')&&await page.locator('#field-notes').evaluate(el=>el.getBoundingClientRect().top<innerHeight),'field notes link reaches explanation');
  const mobileContext=await browser.newContext({viewport:{width:390,height:844},deviceScaleFactor:1,isMobile:true,hasTouch:true});
  const mobile=await mobileContext.newPage();track(mobile);await ready(mobile,origin+'/');
  await mobile.getByRole('button',{name:'Increase angle of attack'}).tap();
  check((await snapshot(mobile)).command===5,'touch control reaches C');
  await mobile.getByRole('button',{name:'Start flying'}).tap();await mobile.clock.runFor(600);
  check((await snapshot(mobile)).time>.4,'touch starts real simulation');
  await mobile.getByRole('button',{name:'Pause flight'}).tap();
  check(await mobile.locator('body').evaluate(el=>el.scrollWidth<=innerWidth),'390px has no overflow');
  for(const el of await mobile.locator('button').all()) {
    const rect=await el.boundingBox();check(rect.width>=44&&rect.height>=44,'buttons have 44px touch target');
  }
  await mobile.evaluate(()=>scrollTo(0,0));await capture(mobile,'mobile');await mobileContext.close();
  // Degraded art must not block the physics or produce a synthetic replacement drawing.
  const fallback=await context.newPage();track(fallback);
  await fallback.route('**/art/**',route=>route.fulfill({status:200,body:''}));
  await fallback.goto(origin+'/');await fallback.waitForFunction(()=>window.liftView?.snapshot);
  await fallback.clock.install();await fallback.getByRole('button',{name:'Start flying'}).click();await fallback.clock.runFor(300);
  check((await snapshot(fallback)).time>0&&!(await snapshot(fallback)).artReady,'art failure leaves controls usable');
  await fallback.close();
  // Engine failure is a visible, noninteractive error state, not fake telemetry.
  const broken=await context.newPage();
  await broken.route('**/physics.wasm',route=>route.fulfill({status:200,body:''}));
  await broken.goto(origin+'/');await broken.getByText('Flight model unavailable — reload to retry').waitFor();
  check(await broken.locator('#toggle').isDisabled()&&await broken.locator('#pitch').isDisabled(),'WASM failure disables controls');
  await broken.close();
  check(errors.length===0,`console/page errors ${errors.join(';')}`);check(external.length===0,'no external requests');
  const result={checks,seconds:(performance.now()-started)/1000,equivalence,errors:errors.length,externalRequests:external.length,
    landing:{time:landing.time,x:landing.x,sink:landing.sink,status:landing.status,onRunway:landing.onRunway},
    manual:{time:manual.time,command:manual.command,throttle:manual.throttle},phugoid:{time:phugoid.time,samples:phugoid.samples},
    screenshots:fs.readdirSync('docs/images').filter(file=>file.endsWith('.jpg')).sort()};
  fs.writeFileSync('docs/validation.json',JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));
}
run().catch(error=>{console.error(error);process.exitCode=1}).finally(async()=>{if(browser)await browser.close();server.close()});
