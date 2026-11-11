// The single web page. It polls /api/status once a second.
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bilge Alarm</title>
<style>
:root{--bg:#f3f5f7;--card:#fff;--fg:#16202a;--mut:#627084;--line:#e2e7ed;--ok:#11795a;--on:#d5281b;--acc:#0b62c9;--btn:#eef2f6}
@media(prefers-color-scheme:dark){:root{--bg:#0e1318;--card:#18202a;--fg:#e5ecf2;--mut:#92a0b0;--line:#293441;--ok:#3ccb8b;--on:#ff6a5d;--acc:#5ba8ff;--btn:#222d3a}}
*{box-sizing:border-box}
body{margin:0;font:15px/1.45 system-ui,-apple-system,Segoe UI,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;gap:10px;align-items:baseline;flex-wrap:wrap;padding:14px 16px 2px;max-width:1100px;margin:auto}
h1{font-size:19px;margin:0}
#link{font-size:13px;color:var(--mut)}#link.bad{color:var(--on);font-weight:600}
main{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px;padding:12px 16px 24px;max-width:1100px;margin:auto}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;min-width:0}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.07em;color:var(--mut);margin:0 0 6px}
table{width:100%;border-collapse:collapse}
td,th{padding:5px 0;border-bottom:1px solid var(--line);text-align:left;font-weight:400}
th{color:var(--mut);font-size:12px}
tr:last-child td{border-bottom:0}
td.n,th.n{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap}
.big{font-size:17px;font-weight:600}
.pill{display:inline-block;min-width:44px;text-align:center;padding:1px 9px;border-radius:99px;font-size:12px;font-weight:600;background:var(--btn);color:var(--mut)}
.pill.on{background:var(--on);color:#fff}.pill.ok{background:var(--ok);color:#fff}
.mut{color:var(--mut);font-size:13px}.warn{color:var(--on);font-size:13px}
button{font:inherit;padding:7px 12px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--fg);cursor:pointer}
button.act{background:var(--on);border-color:var(--on);color:#fff}
button.pri{background:var(--acc);border-color:var(--acc);color:#fff}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
input,select{font:inherit;width:100%;padding:7px 9px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg);margin-top:8px}
p{margin:8px 0 0}
</style></head><body>
<header><h1>Bilge Alarm</h1><span class="mut" id="fw"></span><span id="link">connecting...</span></header>
<main>
<section><h2>Inputs</h2>
<table><thead><tr><th>Input</th><th class="n">Terminal</th><th class="n">ADC pin</th><th class="n">State</th></tr></thead><tbody id="inputs"></tbody></table>
<p class="mut" id="innote"></p></section>

<section><h2>Power and sensors</h2><table id="sens"></table></section>

<section><h2>Outputs</h2>
<div class="row"><button id="b_light" onclick="tog('light')">Light</button><button id="b_buzzer" onclick="tog('buzzer')">Buzzer</button></div>
<div class="row"><button id="b_led_stat" onclick="tog('led_stat')">Status LED</button><button id="b_led_alm" onclick="tog('led_alm')">Alarm LED</button><button id="b_led_auto" onclick="setOut('led_auto',1)">LEDs automatic</button></div>
<p class="mut" id="outnote"></p></section>

<section><h2>NMEA 2000 (CAN)</h2><table id="can"></table>
<div class="row"><button onclick="selftest()">Run self-test</button></div>
<p class="mut">The self-test sends one frame. Use it on the bench, not on the boat's live network.</p>
<table id="pgns"></table></section>

<section><h2>Wi-Fi</h2><table id="wifi"></table>
<select id="nets" onchange="ssid.value=this.value"><option value="">Scan to list networks</option></select>
<input id="ssid" placeholder="Network name" autocomplete="off" maxlength="32">
<input id="pass" type="password" placeholder="Password" autocomplete="off" maxlength="63">
<div class="row"><button onclick="scan(1)">Scan</button><button class="pri" onclick="saveWifi()">Save and connect</button><button onclick="forget()">Forget network</button></div>
<p class="mut" id="wmsg"></p></section>

<section><h2>System</h2><table id="sys"></table>
<div class="row"><button onclick="reboot()">Reboot</button></div>
<input type="file" id="fwfile" accept=".bin">
<div class="row"><button onclick="upload()">Upload firmware</button></div>
<p class="mut" id="umsg"></p></section>
</main>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const row=(k,v)=>`<tr><td>${k}</td><td class="n">${v}</td></tr>`;
const pill=(on,t,f,cls)=>`<span class="pill ${on?(cls||'on'):''}">${on?t:f}</span>`;
let S=null;
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b||''});return r.json().catch(()=>({}))}
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+' d ':'')+(h?h+' h ':'')+m+' min '+(s%60)+' s'}
function render(s){
  S=s;
  $('fw').textContent=s.fw+' '+s.version;
  $('inputs').innerHTML=s.inputs.map(i=>`<tr><td>${i.name}<div class="mut">${esc(i.label)}</div></td>
    <td class="n big">${i.valid?i.v.toFixed(2)+' V':'no reading'}</td>
    <td class="n">${i.mv} mV<div class="${i.err?'warn':'mut'}">${i.adc}${i.err?', '+i.err+' failed':''}</div></td>
    <td class="n">${pill(i.on,'ON','off')}</td></tr>`).join('');
  $('innote').textContent='Active at '+s.on_v+' V or more, off below '+s.off_v+' V. Readings top out near 14.6 V.';
  $('sens').innerHTML=row('Supply voltage',`<span class="big">${s.vin.valid?s.vin.v.toFixed(2)+' V':'no reading'}</span>`)
    +row('Barometric pressure',s.baro.ok?s.baro.hpa.toFixed(1)+' hPa':'<span class="warn">sensor not answering</span>')
    +row('Board temperature',s.baro.ok?s.baro.temp.toFixed(1)+' &deg;C':'-')
    +row('Sensor calibration check',s.baro.present?pill(s.baro.crc,'pass','FAIL','ok'):'-')
    +row('I2C devices found',esc(s.i2c))
    +row('CPU temperature',s.cpu_temp.toFixed(1)+' &deg;C')
    +row('MUTE button',pill(s.mute.pressed,'pressed','released')+' <span class="mut">'+s.mute.presses+' presses</span>');
  for(const k of ['light','buzzer','led_stat','led_alm'])$('b_'+k).className=s.out[k]?'act':'';
  $('b_led_auto').className=s.out.led_auto?'pri':'';
  $('outnote').textContent=(s.out.lamp_test?'MUTE held: lamp and buzzer test running. ':'')
    +(s.hw_bypass?'IN4 is active: the hard-wired bypass holds the light and buzzer on, whatever these buttons say.':'Hold MUTE for a lamp and buzzer test.');
  const c=s.can;
  $('can').innerHTML=row('Mode',c.running?c.mode+', '+c.kbps+' kbit/s':'<span class="warn">driver not running</span>')
    +row('Controller state',c.state)+row('Frames received',c.rx)
    +row('Last frame',c.rx?'0x'+c.last_id.toString(16).toUpperCase().padStart(8,'0')+', '+c.last_age+' s ago':'-')
    +row('Bus errors / missed',c.bus_err+' / '+c.missed)
    +row('Self-test','<span class="'+(c.selftest.startsWith('FAIL')?'warn':'')+'">'+esc(c.selftest)+'</span>');
  $('pgns').innerHTML=c.pgns.length?'<tr><th>PGN</th><th class="n">Source</th><th class="n">Frames</th></tr>'+c.pgns.map(p=>`<tr><td>${p.pgn}</td><td class="n">${p.src}</td><td class="n">${p.n}</td></tr>`).join(''):'';
  const w=s.wifi;
  $('wifi').innerHTML=row('Mode',w.mode=='AP'?pill(1,'setup hotspot','')+' '+esc(w.ap_ssid):w.mode=='STA'?pill(1,'connected','','ok')+' '+esc(w.ssid):'joining '+esc(w.ssid))
    +(w.mode=='STA'?row('Signal',w.rssi+' dBm')+row('Channel',w.channel):row('Hotspot clients',w.clients))
    +row('Address',w.ip)+row('Name','http://'+w.host+'.local')+row('MAC',w.mac)+row('Saved network',w.saved?esc(w.saved):'none');
  $('sys').innerHTML=row('Uptime',dur(s.uptime))+row('Last reset',s.reset)+row('Chip',esc(s.chip)+', '+s.cores+' cores')
    +row('Flash / PSRAM',s.flash_mb+' MB / '+(s.psram_kb?Math.round(s.psram_kb/1024)+' MB':'none'))
    +row('Free memory',Math.round(s.heap/1024)+' kB (lowest '+Math.round(s.heap_min/1024)+' kB)')
    +row('Firmware size',Math.round(s.sketch_kb)+' kB of '+Math.round(s.sketch_max_kb)+' kB')+row('Built',esc(s.build));
}
async function tick(){
  try{const r=await fetch('/api/status',{cache:'no-store'});render(await r.json());$('link').textContent='live';$('link').className=''}
  catch(e){$('link').textContent='no answer from the board';$('link').className='bad'}
}
function setOut(n,v){post('/api/output','name='+n+'&state='+v).then(tick)}
function tog(n){setOut(n,S&&S.out[n]?0:1)}
function selftest(){post('/api/can/selftest').then(tick)}
async function scan(start){
  $('wmsg').textContent='Scanning...';
  const j=await (await fetch('/api/wifi/scan'+(start?'?start=1':''))).json();
  if(j.scanning){setTimeout(()=>scan(0),1500);return}
  $('nets').innerHTML='<option value="">'+j.nets.length+' networks found</option>'+j.nets.map(n=>`<option value="${esc(n.ssid)}">${esc(n.ssid)} (${n.rssi} dBm${n.open?', open':''})</option>`).join('');
  $('wmsg').textContent='';
}
async function saveWifi(){
  const s=$('ssid').value,p=$('pass').value;
  const j=await post('/api/wifi','ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p));
  $('wmsg').textContent=j.msg||'No answer';
}
async function forget(){if(confirm('Forget the saved network and reboot into the setup hotspot?'))$('wmsg').textContent=(await post('/api/wifi/forget')).msg||''}
async function reboot(){if(confirm('Reboot the board?'))post('/api/reboot')}
async function upload(){
  const f=$('fwfile').files[0];if(!f){$('umsg').textContent='Choose a .bin file first.';return}
  $('umsg').textContent='Uploading '+Math.round(f.size/1024)+' kB...';
  const d=new FormData();d.append('firmware',f,f.name);
  try{const r=await fetch('/update',{method:'POST',body:d});$('umsg').textContent=await r.text()}catch(e){$('umsg').textContent='Upload failed: '+e}
}
tick();setInterval(tick,1000);
</script></body></html>
)HTML";
