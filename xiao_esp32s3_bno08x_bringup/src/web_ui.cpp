#include "web_ui.h"

#include <WiFi.h>

#include "app_config.h"

namespace {
const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO BNO08X Monitor</title><style>
body{font-family:system-ui,sans-serif;background:#10151c;color:#e6edf3;margin:16px}h1{font-size:1.25rem}.muted{color:#9ba7b4}.bad{color:#ff7b72}.ok{color:#56d364}table{border-collapse:collapse;width:100%;max-width:820px;margin:12px 0}th,td{border:1px solid #30363d;padding:7px;text-align:right}th:first-child,td:first-child{text-align:left}code{color:#79c0ff}#status{font-weight:bold}.chart{max-width:820px;margin:16px 0}.chart h2{font-size:1rem;margin:0 0 5px}canvas{display:block;width:100%;height:220px;background:#161b22;border:1px solid #30363d}
</style></head><body><h1>XIAO ESP32S3 / BNO08X</h1><p id="status">接続中...</p><p class="muted">20 msごとに画面を更新。値は各イベント種別の最後に受信した値で、ageが鮮度です。</p>
<table><thead><tr><th>値</th><th>X</th><th>Y</th><th>Z</th><th>精度</th><th>age [us]</th><th>seq</th></tr></thead><tbody>
<tr><td>加速度 [m/s²]</td><td id="ax"></td><td id="ay"></td><td id="az"></td><td id="aa"></td><td id="aage"></td><td id="aseq"></td></tr>
<tr><td>角速度 [rad/s]</td><td id="gx"></td><td id="gy"></td><td id="gz"></td><td id="ga"></td><td id="gage"></td><td id="gseq"></td></tr></tbody></table>
<table><thead><tr><th>Rotation Vector / Euler</th><th>X / roll</th><th>Y / pitch</th><th>Z / yaw</th><th>W</th><th>精度</th><th>age [us]</th></tr></thead><tbody>
<tr><td>Quaternion [-]</td><td id="qx"></td><td id="qy"></td><td id="qz"></td><td id="qw"></td><td id="qa"></td><td id="qage"></td></tr>
<tr><td>Euler [deg]</td><td id="roll"></td><td id="pitch"></td><td id="yaw"></td><td>-</td><td id="qseq"></td><td id="qsensor"></td></tr></tbody></table>
<div class="chart"><h2>加速度 [m/s²]（直近約6秒）</h2><canvas id="accelChart"></canvas></div>
<div class="chart"><h2>角速度 [rad/s]（直近約6秒）</h2><canvas id="gyroChart"></canvas></div>
<div class="chart"><h2>姿勢角 [deg]（直近約6秒）</h2><canvas id="attitudeChart"></canvas></div>
<p class="muted">API: <code>/api/latest</code> &middot; 精度はBNO08X/SH-2のstatus生値（下位2ビット: 0=unreliable, 1=low, 2=medium, 3=high）。</p>
<script>
const f=(v,n=4)=>Number(v).toFixed(n),set=(id,v)=>document.getElementById(id).textContent=v,colors=['#58a6ff','#3fb950','#f85149'];
const hist={accel:[],gyro:[],attitude:[]},last={accel:-1,gyro:-1,attitude:-1};let inFlight=false;
function push(name,seq,row){if(seq===last[name])return;last[name]=seq;hist[name].push(row);if(hist[name].length>300)hist[name].shift()}
function graph(id,rows,labels,unit,minScale){const c=document.getElementById(id),dpr=window.devicePixelRatio||1,w=Math.max(1,c.clientWidth*dpr),h=220*dpr;if(c.width!==w||c.height!==h){c.width=w;c.height=h}const x=c.getContext('2d');x.clearRect(0,0,w,h);let scale=minScale;for(const r of rows)for(const v of r)scale=Math.max(scale,Math.abs(v)*1.1);const left=52*dpr,right=8*dpr,top=16*dpr,bottom=22*dpr,pw=w-left-right,ph=h-top-bottom;x.strokeStyle='#30363d';x.lineWidth=dpr;for(let i=0;i<5;i++){const y=top+ph*i/4;x.beginPath();x.moveTo(left,y);x.lineTo(left+pw,y);x.stroke();const val=(scale-(2*scale*i/4)).toFixed(2);x.fillStyle='#9ba7b4';x.font=(10*dpr)+'px sans-serif';x.fillText(val,2*dpr,y+3*dpr)}x.fillStyle='#9ba7b4';x.fillText(unit,left+4*dpr,11*dpr);if(rows.length<2)return;for(let n=0;n<3;n++){x.strokeStyle=colors[n];x.lineWidth=1.5*dpr;x.beginPath();rows.forEach((r,i)=>{const px=left+pw*i/(rows.length-1),py=top+ph*(scale-r[n])/(2*scale);if(i)x.lineTo(px,py);else x.moveTo(px,py)});x.stroke()}x.font=(11*dpr)+'px sans-serif';labels.forEach((l,i)=>{x.fillStyle=colors[i];x.fillText(l,left+(45+i*40)*dpr,h-5*dpr)})}
function draw(){graph('accelChart',hist.accel,['X','Y','Z'],'m/s²',10);graph('gyroChart',hist.gyro,['X','Y','Z'],'rad/s',0.2);graph('attitudeChart',hist.attitude,['roll','pitch','yaw'],'deg',15)}
async function tick(){if(inFlight)return;inFlight=true;try{const d=await fetch('/api/latest',{cache:'no-store'}).then(r=>r.json()),a=d.accel,g=d.gyro,q=d.rotation;['x','y','z'].forEach(k=>set('a'+k,f(a[k])));['x','y','z'].forEach(k=>set('g'+k,f(g[k])));['x','y','z','w'].forEach(k=>set('q'+k,f(q[k],6)));set('aa',a.accuracy);set('ga',g.accuracy);set('qa',q.accuracy);set('aage',a.age_us);set('gage',g.age_us);set('qage',q.age_us);set('aseq',a.sequence);set('gseq',g.sequence);set('qseq','seq '+q.sequence);set('qsensor','sensor '+q.sensor_us+' us');set('roll',f(q.roll_deg,2));set('pitch',f(q.pitch_deg,2));set('yaw',f(q.yaw_deg,2));push('accel',a.sequence,[a.x,a.y,a.z]);push('gyro',g.sequence,[g.x,g.y,g.z]);push('attitude',q.sequence,[q.roll_deg,q.pitch_deg,q.yaw_deg]);draw();const ok=a.valid&&g.valid&&q.valid;const s=document.getElementById('status');s.textContent=ok?'取得中':'一部データ未受信';s.className=ok?'ok':'bad'}catch(e){const s=document.getElementById('status');s.textContent='Web APIへ再接続中';s.className='bad'}finally{inFlight=false}}
tick();setInterval(tick,20);window.addEventListener('resize',draw);
</script></body></html>)HTML";
}  // namespace

void WebUi::begin(const Diagnostics& diagnostics) {
  diagnostics_ = &diagnostics;
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  delay(100);
  if (!WiFi.softAP(app_config::kWebApSsid, app_config::kWebApPassword,
                   app_config::kWebApChannel)) {
    Serial.println("Web UI SoftAP start failed.");
    return;
  }
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/latest", HTTP_GET, [this]() { handleLatest(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  started_ = true;
  Serial.printf("Web UI: connect SSID '%s', then open http://%s/\n", app_config::kWebApSsid,
                WiFi.softAPIP().toString().c_str());
}

void WebUi::handleClient() {
  if (started_) {
    server_.handleClient();
  }
}

void WebUi::printStatus() const {
  if (started_) {
    Serial.printf("# webui: started=1 ssid=%s ip=%s port=%u\n", app_config::kWebApSsid,
                  WiFi.softAPIP().toString().c_str(), app_config::kWebHttpPort);
  } else {
    Serial.println("# webui: started=0");
  }
}

void WebUi::handleRoot() { server_.send_P(200, "text/html; charset=utf-8", kIndexHtml); }

uint32_t WebUi::ageUs(bool valid, uint32_t last_rx_us, uint32_t now_us) {
  return valid ? now_us - last_rx_us : UINT32_MAX;
}

void WebUi::handleLatest() {
  if (diagnostics_ == nullptr) {
    server_.send(503, "application/json", "{\"error\":\"not ready\"}");
    return;
  }
  const uint32_t now_us = micros();
  const VectorSample& accel = diagnostics_->accel();
  const VectorSample& gyro = diagnostics_->gyro();
  const RotationSample& rotation = diagnostics_->rotation();
  char json[1024];
  const int length = snprintf(
      json, sizeof(json),
      "{\"timestamp_us\":%lu,\"refresh_ms\":%lu,\"reinitializations\":%lu,"
      "\"accel\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"accuracy\":%u,\"valid\":%s,\"age_us\":%lu,\"sequence\":%lu,\"sensor_us\":%llu},"
      "\"gyro\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"accuracy\":%u,\"valid\":%s,\"age_us\":%lu,\"sequence\":%lu,\"sensor_us\":%llu},"
      "\"rotation\":{\"x\":%.7f,\"y\":%.7f,\"z\":%.7f,\"w\":%.7f,\"roll_deg\":%.3f,\"pitch_deg\":%.3f,\"yaw_deg\":%.3f,\"accuracy\":%u,\"valid\":%s,\"age_us\":%lu,\"sequence\":%lu,\"sensor_us\":%llu}}",
      static_cast<unsigned long>(now_us), static_cast<unsigned long>(app_config::kWebRefreshMs),
      static_cast<unsigned long>(diagnostics_->reinitializationCount()), accel.x, accel.y, accel.z,
      accel.accuracy, accel.valid ? "true" : "false",
      static_cast<unsigned long>(ageUs(accel.valid, accel.last_rx_us, now_us)),
      static_cast<unsigned long>(accel.sequence), static_cast<unsigned long long>(accel.sensor_timestamp_us),
      gyro.x, gyro.y, gyro.z, gyro.accuracy, gyro.valid ? "true" : "false",
      static_cast<unsigned long>(ageUs(gyro.valid, gyro.last_rx_us, now_us)),
      static_cast<unsigned long>(gyro.sequence), static_cast<unsigned long long>(gyro.sensor_timestamp_us),
      rotation.quaternion.x, rotation.quaternion.y, rotation.quaternion.z, rotation.quaternion.w,
      rotation.euler.roll_deg, rotation.euler.pitch_deg, rotation.euler.yaw_deg, rotation.accuracy,
      rotation.valid ? "true" : "false",
      static_cast<unsigned long>(ageUs(rotation.valid, rotation.last_rx_us, now_us)),
      static_cast<unsigned long>(rotation.sequence),
      static_cast<unsigned long long>(rotation.sensor_timestamp_us));
  if (length <= 0 || length >= static_cast<int>(sizeof(json))) {
    server_.send(500, "application/json", "{\"error\":\"response too large\"}");
    return;
  }
  server_.sendHeader("Cache-Control", "no-store");
  server_.setContentLength(length);
  server_.send(200, "application/json", "");
  server_.client().write(reinterpret_cast<const uint8_t*>(json), length);
}

void WebUi::handleNotFound() { server_.send(404, "text/plain", "Not found. Use / or /api/latest\n"); }