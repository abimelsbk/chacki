#pragma once
#include <pgmspace.h>

// Chacki control panel. Self-contained (no CDN) so it works on any LAN.
static const char CHACKI_PAGE[] PROGMEM = R"chackipage(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chacki Control</title>
<style>
:root{--bg:#05060f;--panel:#0d1124;--line:#22284a;--txt:#dbe7ff;--dim:#7d88b0;--eye:#00e6ff;--acc:#aa46ff;--ok:#28ff8c;--warn:#ffb428;--bad:#ff2840}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
body:before{content:"";position:fixed;inset:0;pointer-events:none;background:repeating-linear-gradient(0deg,transparent 0 2px,rgba(0,0,0,.18) 2px 3px)}
header{position:sticky;top:0;z-index:5;background:rgba(5,6,15,.92);backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:12px 16px;display:flex;gap:14px;align-items:center}
.face{width:64px;height:44px;border-radius:10px;background:#000;border:1px solid var(--line);display:flex;gap:8px;justify-content:center;align-items:center;flex:none}
.face i{width:18px;height:22px;border-radius:6px;background:var(--eye);box-shadow:0 0 10px var(--eye);transition:all .3s}
.face.sleeping i{height:3px}.face.happy i{border-radius:20px 20px 4px 4px;height:12px}
h1{margin:0;font-size:20px;letter-spacing:3px;color:var(--eye);text-shadow:0 0 8px var(--eye)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:4px}.chip{font-size:12px;color:var(--dim);border:1px solid var(--line);border-radius:20px;padding:1px 8px}
main{max-width:860px;margin:auto;padding:12px 16px 60px;display:grid;gap:14px}
@media(min-width:760px){main{grid-template-columns:1fr 1fr}.wide{grid-column:1/-1}}
section{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:14px 16px}
h2{margin:0 0 10px;font-size:13px;letter-spacing:2px;text-transform:uppercase;color:var(--acc)}
.row{display:flex;align-items:center;gap:10px;margin:8px 0;flex-wrap:wrap}.row label{flex:1;min-width:120px;color:var(--dim)}
button{background:transparent;color:var(--txt);border:1px solid var(--eye);border-radius:10px;padding:8px 14px;font:inherit;cursor:pointer}
button:hover{background:rgba(0,230,255,.12)}button.alt{border-color:var(--acc)}button.danger{border-color:var(--bad);color:#ffc0c8}
button.sel{background:var(--eye);color:#000;font-weight:600}
input,select{background:#070a18;color:var(--txt);border:1px solid var(--line);border-radius:8px;padding:7px 9px;font:inherit}
input[type=range]{flex:1.4;accent-color:var(--eye)}input[type=color]{width:46px;height:34px;padding:2px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--eye)}
.bar{flex:1.4;height:12px;border-radius:8px;background:#070a18;border:1px solid var(--line);overflow:hidden}.bar b{display:block;height:100%;background:var(--ok);transition:width .5s}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:8px}
.skin{border:1px solid var(--line);border-radius:10px;padding:8px;cursor:pointer;text-align:center;font-size:13px}
.skin span{display:block;height:20px;border-radius:6px;margin-bottom:6px}
.skin.sel{border-color:var(--eye);box-shadow:0 0 8px var(--eye)}
.big{font-size:34px;font-variant-numeric:tabular-nums;letter-spacing:2px;color:var(--eye)}
.note{font-size:12px;color:var(--dim)}.wifi input{flex:1;min-width:120px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#111830;border:1px solid var(--acc);border-radius:10px;padding:10px 16px;opacity:0;transition:opacity .3s;pointer-events:none;max-width:90%}
#toast.on{opacity:1}progress{width:100%;height:14px;accent-color:var(--eye)}
</style></head><body>
<header><div class="face" id="face"><i></i><i></i></div>
<div><h1 id="title">CHACKI</h1><div class="chips"><span class="chip" id="cMood">-</span><span class="chip" id="cBat">-</span><span class="chip" id="cWifi">-</span><span class="chip" id="cTime">-</span></div></div></header>
<main>

<section><h2>Pet</h2>
<div class="row"><label>Food</label><div class="bar"><b id="bFood"></b></div></div>
<div class="row"><label>Energy</label><div class="bar"><b id="bEnergy"></b></div></div>
<div class="row"><label>Fun</label><div class="bar"><b id="bFun"></b></div></div>
<div class="row"><button onclick="act('feed')">Feed</button><button onclick="act('play')">Play</button><button onclick="act('pet')">Pet</button><button class="alt" onclick="act('sleep')">Sleep</button><button class="alt" onclick="act('wake')">Wake</button></div>
<div class="row" id="moods"></div></section>

<section><h2>Focus buddy</h2>
<div class="big" id="pomoT">--:--</div><div class="note" id="pomoS">Pomodoro is off</div>
<div class="row"><button onclick="act('focus_start')">Start focus</button><button class="alt" onclick="act('focus_stop')">Stop</button></div>
<div class="row"><label>Focus (min)</label><input type="number" min="1" max="120" data-k="pomo_focus" style="width:80px"></div>
<div class="row"><label>Break (min)</label><input type="number" min="1" max="60" data-k="pomo_break" style="width:80px"></div>
<div class="row"><label>Long break (min)</label><input type="number" min="1" max="60" data-k="pomo_long" style="width:80px"></div></section>

<section class="wide"><h2>Personality</h2><div class="row" id="personas"></div>
<div class="row"><label>Name</label><input data-k="name" maxlength="12"></div></section>

<section class="wide"><h2>Skin</h2><div class="grid" id="skins"></div>
<div class="row"><label>Eye colour</label><input type="color" data-k="eye_color"></div>
<div class="row"><label>Accent</label><input type="color" data-k="accent"></div>
<div class="row"><label>Background</label><input type="color" data-k="bg"></div>
<div class="row"><label>Eye shape</label><select data-k="eye_shape"><option>round</option><option>square</option><option>pill</option></select></div>
<div class="row"><label>Scanlines</label><input type="checkbox" data-k="scanlines"></div>
<div class="row"><label>Brightness</label><input type="range" min="10" max="255" data-k="brightness"></div>
<div class="note">Custom skins: put .txt files in /chacki/skins/ on the SD card (see README).</div></section>

<section><h2>Sound &amp; ears</h2>
<div class="row"><label>Volume</label><input type="range" min="0" max="255" data-k="volume"></div>
<div class="row"><label>Sound effects</label><input type="checkbox" data-k="sfx"><button onclick="act('test_sound')">Test</button></div>
<div class="row"><label>Clap / voice reaction</label><input type="checkbox" data-k="clap_on"></div>
<div class="row"><label>Sensitivity</label><input type="range" min="1" max="10" data-k="clap_sens"></div>
<div class="row"><label>Room level</label><div class="bar"><b id="bMic" style="background:var(--eye)"></b></div></div>
<div class="note">Clap once: startled. Clap twice: wake up / happy. Talk nearby: it gets curious.</div></section>

<section><h2>Schedule</h2>
<div class="row"><label>Night mode</label><input type="checkbox" data-k="night_on"></div>
<div class="row"><label>Sleep at / wake at</label><input type="number" min="0" max="23" data-k="night_start" style="width:64px"><input type="number" min="0" max="23" data-k="night_end" style="width:64px"></div>
<div class="row"><label>Alarm</label><input type="checkbox" data-k="alarm_on"><input type="time" data-k="alarm_time"><button onclick="act('test_alarm')">Test</button></div>
<div class="row"><label>Water every (min, 0=off)</label><input type="number" min="0" max="600" data-k="water_min" style="width:80px"></div>
<div class="row"><label>Stand up every (min)</label><input type="number" min="0" max="600" data-k="stand_min" style="width:80px"></div>
<div class="row"><label>Custom reminder</label><input data-k="custom_text" maxlength="40" placeholder="e.g. Check Burp scan"></div>
<div class="row"><label>...every (min)</label><input type="number" min="0" max="1440" data-k="custom_min" style="width:80px"><button onclick="act('test_reminder')">Test</button></div></section>

<section class="wide"><h2>Network</h2><div class="note">Up to 5 networks (2.4&nbsp;GHz). Leave a password empty to keep the saved one. Saving reboots Chacki.</div>
<div id="wifi"></div>
<div class="row"><label>Backend URL</label><input id="nBackend" placeholder="http://192.168.1.50:8000" style="flex:2"></div>
<div class="row"><label>Device token</label><input id="nToken" type="password" placeholder="unchanged" style="flex:2"></div>
<div class="row"><label>Timezone (POSIX)</label><input id="nTz" style="flex:2"></div>
<div class="row"><button onclick="saveNet()">Save &amp; reboot</button></div></section>

<section><h2>Security</h2>
<div class="row"><label>New web password</label><input id="pw1" type="password" minlength="6"></div>
<div class="row"><label>Repeat</label><input id="pw2" type="password"></div>
<div class="row"><button onclick="savePw()">Change password</button></div>
<div class="note">Login user is <b>chacki</b>. The same password protects wireless updates. Plain HTTP: avoid using this on untrusted Wi-Fi.</div></section>

<section><h2>Firmware</h2>
<div class="row"><label>Version</label><span id="fVer">-</span></div>
<div class="row"><label>Update mode</label><span id="fMode">-</span></div>
<div class="note" id="fHelp"></div>
<div class="row"><input type="file" id="fFile" accept=".bin" style="flex:1"><button onclick="upload()">Upload</button></div>
<progress id="fProg" max="100" value="0" hidden></progress>
<div class="row"><button class="alt" onclick="post('/api/reboot').then(()=>toast('Rebooting...'))">Reboot</button><button class="danger" onclick="if(confirm('Return to M5 Launcher?'))post('/api/launcher')">Exit to Launcher</button></div></section>

</main><div id="toast"></div>
<script>
const $=s=>document.querySelector(s),$$=s=>document.querySelectorAll(s);let S={},busy=0;
const MOODS=['happy','sad','angry','curious','thinking','excited','focused','sleeping','error'];
function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('on');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('on'),3500)}
async function api(p,o){const r=await fetch(p,o);const j=await r.json().catch(()=>({}));if(!r.ok)throw new Error(j.error||r.statusText);return j}
const post=(p,b)=>api(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})});
function act(a,extra){post('/api/action',Object.assign({action:a},extra||{})).catch(e=>toast(e.message))}
let tm={};function save(k,v){clearTimeout(tm[k]);tm[k]=setTimeout(()=>post('/api/settings',{[k]:v}).then(r=>{S[k]=v;theme()}).catch(e=>toast(e.message)),250)}
function theme(){const r=document.documentElement.style;if(S.eye_color)r.setProperty('--eye',S.eye_color);if(S.accent)r.setProperty('--acc',S.accent);$('#title').textContent=(S.name||'CHACKI').toUpperCase()}
function fill(){$$('[data-k]').forEach(el=>{const k=el.dataset.k,v=S[k];if(v===undefined)return;if(el.type=='checkbox')el.checked=v;else el.value=v;
 el.onchange=el.oninput=()=>{let x=el.type=='checkbox'?el.checked:(el.type=='number'||el.type=='range')?+el.value:el.value;save(k,x)}});
 $('#personas').innerHTML=S.personalities.map(p=>`<button class="${p==S.personality?'sel':''}" onclick="setP('${p}')">${p}</button>`).join('');
 $('#skins').innerHTML=S.skins.map(n=>`<div class="skin ${n==S.skin?'sel':''}" onclick="setSkin(this.dataset.n)" data-n="${n.replace(/"/g,'&quot;')}"><span style="background:linear-gradient(90deg,var(--eye),var(--acc))"></span>${n.replace(/</g,'&lt;')}</div>`).join('');
 $('#wifi').innerHTML=S.wifi.map((w,i)=>`<div class="row wifi"><input id="ws${i}" placeholder="Network ${i+1} name" value="${(w.ssid||'').replace(/"/g,'&quot;')}"><input id="wp${i}" type="password" placeholder="${w.has_pass?'saved - unchanged':'password'}"></div>`).join('');
 $('#nBackend').value=S.backend_url||'';$('#nTz').value=S.tz||'';theme()}
async function load(){S=await api('/api/settings');fill()}
function setP(p){post('/api/settings',{personality:p}).then(load).catch(e=>toast(e.message))}
function setSkin(n){post('/api/settings',{skin:n}).then(load).catch(e=>toast(e.message))}
function saveNet(){const wifi=S.wifi.map((w,i)=>{const o={ssid:$('#ws'+i).value.trim()};const p=$('#wp'+i).value;if(p)o.pass=p;return o});
 const b={wifi,backend_url:$('#nBackend').value.trim(),tz:$('#nTz').value.trim()};if($('#nToken').value)b.device_token=$('#nToken').value;
 post('/api/settings',b).then(()=>post('/api/reboot')).then(()=>toast('Saved - rebooting. Reconnect in ~20 s')).catch(e=>toast(e.message))}
function savePw(){const a=$('#pw1').value;if(a.length<6)return toast('At least 6 characters');if(a!=$('#pw2').value)return toast('Passwords differ');
 post('/api/settings',{web_pass:a}).then(()=>post('/api/reboot')).then(()=>toast('Password changed - log in again after reboot')).catch(e=>toast(e.message))}
function upload(){const f=$('#fFile').files[0];if(!f)return toast('Choose Chacki-CoreS3SE-launcher.bin first');
 const x=new XMLHttpRequest(),fd=new FormData(),pr=$('#fProg');fd.append('firmware',f,f.name);pr.hidden=false;pr.value=0;busy=1;
 x.upload.onprogress=e=>{if(e.lengthComputable)pr.value=e.loaded*100/e.total};
 x.onload=()=>{busy=0;let j={};try{j=JSON.parse(x.responseText)}catch(e){}toast(x.status==200?(j.message||'Done'):('Failed: '+(j.error||x.status)));if(x.status==200)setTimeout(()=>location.reload(),20000)};
 x.onerror=()=>{busy=0;toast('Upload failed (connection lost)')};x.open('POST','/update');x.send(fd)}
const fmt=s=>String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');
function bar(id,v){const b=$(id);b.style.width=v+'%';b.style.background=v<25?'var(--bad)':v<50?'var(--warn)':'var(--ok)'}
async function poll(){if(busy)return;try{const s=await api('/api/status');
 $('#cMood').textContent=s.mood+' · '+s.personality;$('#cBat').textContent=s.battery>=0?(s.charging?'⚡':'')+s.battery+'%':'no battery';
 $('#cWifi').textContent=s.ssid+' '+s.rssi+'dBm';$('#cTime').textContent=s.time;
 bar('#bFood',s.food);bar('#bEnergy',s.energy);bar('#bFun',s.fun);$('#bMic').style.width=Math.min(100,s.ambient*100)+'%';
 const f=$('#face');f.className='face '+(s.sleeping?'sleeping':s.mood);
 $('#pomoT').textContent=s.pomo=='off'?'--:--':fmt(s.pomo_remain);
 $('#pomoS').textContent=s.pomo=='off'?`Pomodoro is off · ${s.sessions} done today`:(s.pomo=='focus'?'Focusing':'On a break')+` · ${s.sessions} done`;
 $('#fVer').textContent=s.version+' ('+s.device_id+')';
 $('#fMode').textContent=s.ota_mode=='flash'?'Direct (wireless flash)':'Via SD + Launcher';
 $('#fHelp').innerHTML=s.ota_mode=='flash'?'Upload <b>Chacki-CoreS3SE-launcher.bin</b>; Chacki flashes it and reboots. From VS Code: <code>pio run -e cores3se-ota -t upload</code>.':
  'Running under M5 Launcher (single app slot), so Chacki cannot overwrite itself safely. Uploading saves the file to the SD card and reboots into Launcher: then tap <b>SD &gt; chacki &gt; update.bin &gt; Install</b>. No card swapping needed.';
}catch(e){}}
$('#moods').innerHTML=MOODS.map(m=>`<button class="alt" onclick="act('mood',{mood:'${m}'})">${m}</button>`).join('');
load().catch(e=>toast(e.message));poll();setInterval(poll,1500);
</script></body></html>)chackipage";
