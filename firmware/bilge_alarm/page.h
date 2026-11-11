// The single web page. It polls /api/status once a second and /api/log every five.
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bilge Alarm</title>
<style>
:root{--bg:#f3f5f7;--card:#fff;--fg:#16202a;--mut:#627084;--line:#e2e7ed;--ok:#11795a;--on:#d5281b;--warn:#b25e00;--acc:#0b62c9;--btn:#eef2f6}
@media(prefers-color-scheme:dark){:root{--bg:#0e1318;--card:#18202a;--fg:#e5ecf2;--mut:#92a0b0;--line:#293441;--ok:#3ccb8b;--on:#ff6a5d;--warn:#f0a23b;--acc:#5ba8ff;--btn:#222d3a}}
*{box-sizing:border-box}
body{margin:0;font:15px/1.45 system-ui,-apple-system,Segoe UI,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;gap:10px;align-items:baseline;flex-wrap:wrap;padding:14px 16px 2px;max-width:1100px;margin:auto}
h1{font-size:19px;margin:0}
#link{font-size:13px;color:var(--mut)}#link.bad{color:var(--on);font-weight:600}
#banner{max-width:1068px;margin:10px auto 0;padding:12px 14px;border-radius:10px;display:flex;gap:12px;align-items:center;justify-content:space-between;flex-wrap:wrap;font-weight:600;background:var(--card);border:1px solid var(--line)}
#banner.ok{border-left:6px solid var(--ok)}#banner.alarm{background:var(--on);border-color:var(--on);color:#fff}#banner.was{border-left:6px solid var(--warn)}
.wrap{padding:0 16px}
main{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:12px;padding:12px 16px 24px;max-width:1100px;margin:auto}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;min-width:0}
section.wide{grid-column:1/-1}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.07em;color:var(--mut);margin:0 0 6px}
.scroll{overflow-x:auto}
table{width:100%;border-collapse:collapse}
td,th{padding:5px 8px 5px 0;border-bottom:1px solid var(--line);text-align:left;font-weight:400;vertical-align:top}
th{color:var(--mut);font-size:12px}
tr:last-child td{border-bottom:0}
td.n,th.n{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap;padding-right:0}
.big{font-size:17px;font-weight:600}
.pill{display:inline-block;min-width:44px;text-align:center;padding:1px 9px;border-radius:99px;font-size:12px;font-weight:600;background:var(--btn);color:var(--mut)}
.pill.on{background:var(--on);color:#fff}.pill.ok{background:var(--ok);color:#fff}.pill.run{background:var(--acc);color:#fff}
.mut{color:var(--mut);font-size:13px}.warn{color:var(--on);font-size:13px}
button{font:inherit;padding:7px 12px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--fg);cursor:pointer}
button.pri{background:var(--acc);border-color:var(--acc);color:#fff}
#banner button{background:#fff;color:#16202a;border-color:#fff}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
input[type=text],input[type=number],input[type=password],input[type=file],select{font:inherit;width:100%;padding:7px 9px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg);margin-top:4px}
label{display:block;margin-top:10px;font-size:13px;color:var(--mut)}
label.chk{display:flex;gap:8px;align-items:center;color:var(--fg);font-size:15px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
p{margin:8px 0 0}
#log{max-height:330px;overflow-y:auto}
</style></head><body>
<header><h1>Bilge Alarm</h1><span class="mut" id="fw"></span><span id="link">connecting...</span></header>
<div class="wrap"><div id="banner" class="ok"><span id="btext">...</span><button id="bmute" onclick="muteNow()" hidden>Mute</button></div></div>
<main>
<section class="wide"><h2>Inputs since power-on</h2><div class="scroll">
<table><thead><tr><th>Input</th><th>Now</th><th class="n">Times on</th><th class="n">Total time on</th><th class="n">Last time</th></tr></thead><tbody id="inputs"></tbody></table></div>
<p class="mut" id="innote"></p></section>

<section><h2>Alarms since power-on</h2>
<table><thead><tr><th>Alarm</th><th>Now</th><th class="n">Times</th><th class="n">Last</th></tr></thead><tbody id="alarms"></tbody></table></section>

<section><h2>Event log</h2><div id="log"><table id="logt"></table></div></section>

<section><h2>Power and sensors</h2><table id="sens"></table></section>

<section><h2>Outputs</h2><table id="outs"></table>
<div class="row"><button onclick="test('light')">Test light</button><button onclick="test('buzzer')">Test buzzer</button></div>
<p class="mut" id="outnote"></p></section>

<section><h2>Bluetooth (Victron Cerbo GX)</h2><table id="ble"></table>
<p class="mut">Sent as a RuuviTag temperature sensor with pressure. Enable it once on the Cerbo under Settings, Integrations, Bluetooth Sensors.</p></section>

<section><h2>NMEA 2000 (CAN)</h2><table id="can"></table>
<div class="row"><button onclick="selftest()">Run self-test</button></div>
<p class="mut">Sends pump and float states, supply voltage, pressure, temperature and alarms. The self-test sends one extra frame; use it on the bench, not on the boat's live network.</p>
<table id="pgns"></table></section>

<section><h2>Wi-Fi</h2><table id="wifi"></table>
<select id="nets" onchange="ssid.value=this.value"><option value="">Scan to list networks</option></select>
<input type="text" id="ssid" placeholder="Network name" autocomplete="off" maxlength="32">
<input type="password" id="pass" placeholder="Password" autocomplete="off" maxlength="63">
<div class="row"><button onclick="scan(1)">Scan</button><button class="pri" onclick="saveWifi()">Save and connect</button><button onclick="forget()">Forget network</button></div>
<p class="mut" id="wmsg"></p></section>

<section><h2>Settings</h2>
<div class="grid2">
<label>IN1 name<input type="text" id="n0" maxlength="23"></label><label>IN2 name<input type="text" id="n1" maxlength="23"></label>
<label>IN3 name<input type="text" id="n2" maxlength="23"></label><label>IN4 (float) name<input type="text" id="n3" maxlength="23"></label>
<label>High-water delay, seconds<input type="number" id="float_delay" min="0" max="60"></label>
<label>Pump runs too long after, minutes (0 = off)<input type="number" id="long_run" min="0" max="720"></label>
<label>Pump starts per hour limit (0 = off)<input type="number" id="cycles" min="0" max="30"></label>
<label>Low supply voltage, volts (0 = off)<input type="number" id="low_volt" min="0" max="14" step="0.1"></label>
</div>
<label class="chk"><input type="checkbox" id="light_on_pump">Light on while a pump runs</label>
<label class="chk"><input type="checkbox" id="buzz_long">Buzzer beeps when a pump runs too long</label>
<label class="chk"><input type="checkbox" id="buzz_cyc">Buzzer beeps when a pump starts too often</label>
<label class="chk"><input type="checkbox" id="buzz_low">Buzzer beeps on low supply voltage</label>
<label class="chk"><input type="checkbox" id="ble">Bluetooth sensor for the Cerbo GX</label>
<label class="chk"><input type="checkbox" id="n2k">NMEA 2000 output (changes after a restart)</label>
<label class="chk"><input type="checkbox" id="n2k_alerts">Send alarms to NMEA 2000 displays</label>
<div class="grid2">
<label>Switch bank instance<input type="number" id="n2k_bank" min="0" max="252"></label>
<label>Battery instance<input type="number" id="n2k_batt" min="0" max="252"></label>
<label>Temperature instance<input type="number" id="n2k_temp" min="0" max="252"></label>
<label>Temperature shown as<select id="n2k_src"><option value="2">Inside</option><option value="4">Main cabin</option><option value="3">Engine room</option><option value="1">Outside</option></select></label>
</div>
<label>Supply voltage correction factor (0.8 to 1.2)<input type="number" id="vin_cal" min="0.8" max="1.2" step="0.001"></label>
<label>New admin password (leave empty to keep; login name is "admin")<input type="password" id="apass" maxlength="31" autocomplete="new-password"></label>
<label class="chk"><input type="checkbox" id="apass_clear">Remove the admin password</label>
<div class="row"><button class="pri" onclick="saveSettings()">Save settings</button></div>
<p class="mut" id="smsg"></p></section>

<section><h2>System</h2><table id="sys"></table>
<div class="row"><button onclick="resetCounters()">Reset counters and log</button><button onclick="reboot()">Restart</button></div>
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
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b||''});return r.json().catch(()=>({msg:r.status==401?'Login needed.':''}))}
function dur(s){s=Math.max(0,Math.round(s));const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  if(d)return d+' d '+h+' h';if(h)return h+' h '+m+' min';if(m)return m+' min '+(s%60)+' s';return s+' s'}
const ago=(now,t)=>dur(now-t)+' ago';
function render(s){
  S=s;
  $('fw').textContent=s.fw+' '+s.version;
  const b=$('banner'),a=s.alarm;
  b.className=a.active?'alarm':a.unacked?'was':'ok';
  $('btext').textContent=a.active?'ALARM: '+a.text:a.unacked?'There was an alarm. See the event log.':'All clear';
  $('bmute').hidden=!(a.sounding||a.unacked);
  $('bmute').textContent=a.active?'Mute':'Acknowledge';
  $('inputs').innerHTML=s.inputs.map(i=>`<tr><td><span class="big">${esc(i.label)}</span><div class="${i.err?'warn':'mut'}">${i.name}, ${i.valid?i.v.toFixed(2)+' V':'no reading'}${i.err?', '+i.err+' failed reads':''}</div></td>
    <td>${pill(i.on,i.pump?'RUNNING':'HIGH','off',i.pump?'run':'on')}${i.on?'<div class="mut">for '+dur(i.on_for)+'</div>':''}</td>
    <td class="n big">${i.count}${i.pump?'<div class="mut">'+i.starts_hour+' in the last hour</div>':''}</td>
    <td class="n">${dur(i.total_s)}<div class="mut">longest ${dur(Math.max(i.longest_s,i.on_for))}</div></td>
    <td class="n">${i.count?(i.on?'now':dur(i.last_s))+'<div class="mut">'+ago(s.pon,i.last_on)+'</div>':'never'}</td></tr>`).join('');
  $('innote').textContent='Counted since the board was powered on, '+dur(s.pon)+' ago. An input is on at '+s.on_v+' V or more and off below '+s.off_v+' V.';
  $('alarms').innerHTML=s.alarms.map(x=>`<tr><td>${esc(x.text)}</td>
    <td>${!x.enabled?'<span class="mut">off in settings</span>':x.active?pill(1,x.muted?'muted':'ALARM','')+'<div class="mut">for '+dur(x.for)+'</div>':pill(0,'','ok')}</td>
    <td class="n big">${x.count}</td><td class="n">${x.count?ago(s.pon,x.last_on):'never'}</td></tr>`).join('');
  $('sens').innerHTML=row('Supply voltage',`<span class="big">${s.vin.valid?s.vin.v.toFixed(2)+' V':'no reading'}</span>`)
    +(s.vin.max?row('Lowest / highest since restart',s.vin.min.toFixed(2)+' / '+s.vin.max.toFixed(2)+' V'):'')
    +row('Barometric pressure',s.baro.ok?s.baro.hpa.toFixed(1)+' hPa':'<span class="warn">sensor not answering</span>')
    +row('Board temperature',s.baro.ok?s.baro.temp.toFixed(1)+' &deg;C':'-')
    +row('Sensor calibration check',s.baro.present?pill(s.baro.crc,'pass','FAIL','ok'):'-')
    +row('CPU temperature',s.cpu_temp.toFixed(1)+' &deg;C')
    +row('MUTE button',pill(s.mute.pressed,'pressed','released'));
  $('outs').innerHTML=row('Light',pill(s.out.light,'ON','off'))+row('Buzzer',pill(s.out.buzzer,'ON','off'))
    +row('Alarm LED',pill(s.out.led_alm,'on','off'))+row('Status LED',s.wifi.mode=='STA'?'short flash: on the network':s.wifi.mode=='AP'?'slow blink: setup hotspot':'fast blink: joining');
  $('outnote').textContent=(s.out.lamp_test?'MUTE held: lamp and buzzer test running. ':'')
    +(s.hw_bypass?'The float is up: the hard-wired bypass holds the light and buzzer on. Mute cannot silence that.':'A test lasts 5 seconds. Holding MUTE on the board tests both; holding it for 10 seconds resets Wi-Fi.');
  const e=s.ble;
  $('ble').innerHTML=row('State',!e.enabled?'off in settings':e.failed?'<span class="warn">could not start</span>':e.advertising?pill(1,'advertising','','ok'):'paused while the setup hotspot is in use')
    +row('Address',e.mac||'-')+row('Updates sent',e.frames);
  const c=s.can;
  $('can').innerHTML=row('State',!c.running?'<span class="warn">driver not running</span>':!c.n2k?'listening only (output off in settings)':c.on_bus?pill(1,'on the network','','ok')+' address '+c.addr:'<span class="warn">no other device answers</span>')
    +(c.n2k?row('Messages sent / dropped',c.sent+' / '+c.send_fail)+row('Alarms to displays',c.alerts?'on':'off in settings'):'')
    +row('Frames received',c.rx)+row('Last frame',c.rx?'0x'+c.last_id.toString(16).toUpperCase().padStart(8,'0')+', '+c.last_age+' s ago':'-')
    +row('Bus errors / missed / restarts',c.bus_err+' / '+c.missed+' / '+c.bus_offs)
    +row('Self-test','<span class="'+(c.selftest.startsWith('FAIL')?'warn':'')+'">'+esc(c.selftest)+'</span>');
  $('pgns').innerHTML=c.pgns.length?'<tr><th>PGN</th><th class="n">Source</th><th class="n">Frames</th></tr>'+c.pgns.map(p=>`<tr><td>${p.pgn}</td><td class="n">${p.src}</td><td class="n">${p.n}</td></tr>`).join(''):'';
  const w=s.wifi;
  $('wifi').innerHTML=row('Mode',w.mode=='AP'?pill(1,'setup hotspot','')+' '+esc(w.ap_ssid):w.mode=='STA'?pill(1,'connected','','ok')+' '+esc(w.ssid):'joining '+esc(w.ssid))
    +(w.mode=='STA'?row('Signal',w.rssi+' dBm')+row('Channel',w.channel):row('Hotspot clients',w.clients))
    +row('Address',w.ip)+row('Name','http://'+w.host+'.local')+row('MAC',w.mac)+row('Saved network',w.saved?esc(w.saved):'none');
  $('sys').innerHTML=row('Powered on for',dur(s.pon))+row('Running since last restart',dur(s.up))
    +row('Restarts since power-on',s.restarts)+row('Last restart',s.reset)+row('Chip',esc(s.chip)+', '+s.cores+' cores')
    +row('Flash / PSRAM',s.flash_mb+' MB / '+(s.psram_kb?Math.round(s.psram_kb/1024)+' MB':'not used'))
    +row('Free memory',Math.round(s.heap/1024)+' kB (lowest '+Math.round(s.heap_min/1024)+' kB)')
    +row('Firmware size',Math.round(s.sketch_kb)+' kB of '+Math.round(s.sketch_max_kb)+' kB')+row('Built',esc(s.build))
    +row('Login for changes',s.login?'on (name: admin)':'off');
}
async function tick(){
  try{const r=await fetch('/api/status',{cache:'no-store'});render(await r.json());$('link').textContent='live';$('link').className=''}
  catch(e){$('link').textContent='no answer from the board';$('link').className='bad'}
}
async function loadLog(){
  try{const j=await (await fetch('/api/log',{cache:'no-store'})).json();
    $('logt').innerHTML=j.events.length?j.events.map(e=>`<tr><td class="mut" style="white-space:nowrap">${ago(j.pon,e.t)}</td><td${e.alarm?' class="warn"':''}>${esc(e.text)}</td></tr>`).join(''):'<tr><td class="mut">Nothing yet</td></tr>'}
  catch(e){}
}
function muteNow(){post('/api/mute').then(tick)}
function test(n){post('/api/test','name='+n).then(tick)}
function selftest(){post('/api/can/selftest').then(tick)}
const CHK=['light_on_pump','buzz_long','buzz_cyc','buzz_low','ble','n2k','n2k_alerts'],NUM=['float_delay','long_run','cycles','low_volt','vin_cal','n2k_bank','n2k_batt','n2k_temp','n2k_src'];
async function loadSettings(){
  const j=await (await fetch('/api/settings',{cache:'no-store'})).json();
  j.names.forEach((n,i)=>$('n'+i).value=n);
  NUM.forEach(k=>$(k).value=j[k]);CHK.forEach(k=>$(k).checked=j[k]);
}
async function saveSettings(){
  let b='flags=1';
  for(let i=0;i<4;i++)b+='&n'+i+'='+encodeURIComponent($('n'+i).value);
  NUM.forEach(k=>b+='&'+k+'='+encodeURIComponent($(k).value));
  CHK.forEach(k=>b+='&'+k+'='+($(k).checked?1:0));
  if($('apass_clear').checked)b+='&pass_set=1&pass=';
  else if($('apass').value)b+='&pass_set=1&pass='+encodeURIComponent($('apass').value);
  const j=await post('/api/settings',b);
  $('smsg').textContent=j.msg||'No answer';
  $('apass').value='';$('apass_clear').checked=false;
  loadSettings();tick();
}
async function scan(start){
  $('wmsg').textContent='Scanning...';
  const j=await (await fetch('/api/wifi/scan'+(start?'?start=1':''))).json();
  if(j.scanning){setTimeout(()=>scan(0),1500);return}
  $('nets').innerHTML='<option value="">'+j.nets.length+' networks found</option>'+j.nets.map(n=>`<option value="${esc(n.ssid)}">${esc(n.ssid)} (${n.rssi} dBm${n.open?', open':''})</option>`).join('');
  $('wmsg').textContent='';
}
async function saveWifi(){
  const j=await post('/api/wifi','ssid='+encodeURIComponent($('ssid').value)+'&pass='+encodeURIComponent($('pass').value));
  $('wmsg').textContent=j.msg||'No answer';
}
async function forget(){if(confirm('Forget the saved network and open the setup hotspot?'))$('wmsg').textContent=(await post('/api/wifi/forget')).msg||''}
async function reboot(){if(confirm('Restart the board? Counters and the log are kept.'))post('/api/reboot')}
async function resetCounters(){if(confirm('Set all counters to zero and clear the event log?')){await post('/api/counters/reset');tick();loadLog()}}
async function upload(){
  const f=$('fwfile').files[0];if(!f){$('umsg').textContent='Choose a .bin file first.';return}
  $('umsg').textContent='Uploading '+Math.round(f.size/1024)+' kB...';
  const d=new FormData();d.append('firmware',f,f.name);
  try{const r=await fetch('/update',{method:'POST',body:d});$('umsg').textContent=r.status==401?'Login needed.':await r.text()}catch(e){$('umsg').textContent='Upload failed: '+e}
}
tick();loadLog();loadSettings();setInterval(tick,1000);setInterval(loadLog,5000);
</script></body></html>
)HTML";
