#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <SparkFun_VL53L5CX_Library.h>
#include "app_config.h"
#include "bno_reader.h"
#include "sd_logger.h"

using namespace app_config;

namespace {
WebServer web(kHttpPort);
bno::Reader imu;
btf::Logger logger;
TwoWire tofWire(1);
SparkFun_VL53L5CX tof;
VL53L5CX_ResultsData tofData{};

char tofScan[80] = "not scanned";
bool tofReady = false;
uint32_t tofFrames = 0;
uint64_t tofLastUs = 0;
uint16_t tofMin = 0;
uint16_t tofMedian = 0;
uint16_t tofMax = 0;
uint8_t tofValid = 0;

bool summaryPending = false;
uint32_t lastSystem = 0;
uint32_t lastStatus = 0;
uint32_t lastSummaryTry = 0;
uint64_t lastLogUs[4]{};

void put32(uint8_t*& p, uint32_t v) {
  for (int i = 0; i < 4; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
}

void onSample(const bno::Sample& s) {
  btf::Event e{};
  e.type = s.type == bno::EventType::Accel ? btf::Type::BnoAccel :
           s.type == bno::EventType::Gyro ? btf::Type::BnoGyro :
                                               btf::Type::BnoGameRotation;
  e.accuracy = s.accuracy;
  e.sequence = s.sequence;
  e.rxUs = s.rxUs;
  e.sensorUs = s.sensorUs;
  const uint8_t index = static_cast<uint8_t>(s.type);
  e.deltaUs = lastLogUs[index] ? static_cast<uint32_t>(s.rxUs - lastLogUs[index]) : 0;
  lastLogUs[index] = s.rxUs;
  memcpy(e.payload, s.v, sizeof(s.v));
  logger.enqueue(e);
}

void scanTofBus() {
  size_t used = 0;
  tofScan[0] = '\0';
  for (uint8_t address = 1; address < 0x78; ++address) {
    tofWire.beginTransmission(address);
    if (tofWire.endTransmission() == 0) {
      used += snprintf(tofScan + used, sizeof(tofScan) - used, "%s0x%02X",
                       used ? " " : "", address);
      if (used >= sizeof(tofScan) - 6) break;
    }
  }
  if (!used) snprintf(tofScan, sizeof(tofScan), "none");
  Serial.printf("TOF I2C scan SDA=GPIO%d SCL=GPIO%d: %s\n",
                kTofSdaPin, kTofSclPin, tofScan);
}

void pollTof() {
  if (!tofReady || !tof.isDataReady()) return;

  const uint64_t receiveUs = esp_timer_get_time();
  if (!tof.getRangingData(&tofData)) return;

  ++tofFrames;
  tofLastUs = receiveUs;
  uint16_t validDistances[64];
  tofValid = 0;
  tofMin = UINT16_MAX;
  tofMax = 0;

  for (uint8_t i = 0; i < 64; ++i) {
    const uint16_t distance = tofData.distance_mm[i];
    const uint8_t status = tofData.target_status[i];
    if ((status == 5 || status == 6 || status == 9) && distance > 0) {
      validDistances[tofValid++] = distance;
      if (distance < tofMin) tofMin = distance;
      if (distance > tofMax) tofMax = distance;
    }
  }

  for (uint8_t i = 1; i < tofValid; ++i) {
    const uint16_t value = validDistances[i];
    int j = i - 1;
    while (j >= 0 && validDistances[j] > value) {
      validDistances[j + 1] = validDistances[j];
      --j;
    }
    validDistances[j + 1] = value;
  }
  tofMedian = tofValid ? validDistances[tofValid / 2] : 0;
  if (!tofValid) tofMin = 0;

  btf::Event header{};
  header.type = btf::Type::TofFrameHeader;
  header.rxUs = receiveUs;
  uint8_t* p = header.payload;
  put32(p, tofFrames);
  put32(p, 0);
  logger.enqueue(header);

  for (uint8_t block = 0; block < 2; ++block) {
    btf::Event event{};
    event.type = block ? btf::Type::TofBlock1 : btf::Type::TofBlock0;
    event.rxUs = receiveUs;
    p = event.payload;
    put32(p, tofFrames);
    *p++ = block * 32;
    memcpy(p, tofData.distance_mm + block * 32, 64);
    p += 64;
    memcpy(p, tofData.target_status + block * 32, 32);
    logger.enqueue(event);
  }
}

void systemLog() {
  if (millis() - lastSystem < kSystemIntervalMs) return;
  lastSystem = millis();
  btf::Event event{};
  event.type = btf::Type::SystemStatus;
  event.rxUs = esp_timer_get_time();
  uint8_t* p = event.payload;
  put32(p, imu.accelRate().count);
  put32(p, imu.gyroRate().count);
  put32(p, imu.rotationRate().count);
  put32(p, imu.accelRate().missing);
  put32(p, imu.gyroRate().missing);
  put32(p, imu.rotationRate().missing);
  put32(p, logger.stats().dropped);
  put32(p, logger.stats().writeErrors);
  put32(p, imu.reinitCount());
  logger.enqueue(event);
}

uint32_t ageMs(uint64_t timestampUs, uint64_t nowUs) {
  return timestampUs ? static_cast<uint32_t>((nowUs - timestampUs) / 1000) : UINT32_MAX;
}

void api() {
  const auto& sample = imu.latest();
  const auto& stats = logger.stats();
  const uint64_t nowUs = esp_timer_get_time();
  char json[1600];
  snprintf(json, sizeof(json),
    "{\"stage\":2,\"bno\":\"%s\",\"addr\":\"0x%02X\",\"i2c_hz\":%lu,"
    "\"accel_hz\":%.2f,\"gyro_hz\":%.2f,\"rotation_hz\":%.2f,"
    "\"accel_age_ms\":%lu,\"gyro_age_ms\":%lu,\"rotation_age_ms\":%lu,"
    "\"ax\":%.5f,\"ay\":%.5f,\"az\":%.5f,\"gx\":%.5f,\"gy\":%.5f,\"gz\":%.5f,"
    "\"roll\":%.3f,\"pitch\":%.3f,\"yaw\":%.3f,\"sd\":\"%s\",\"run\":\"%s\","
    "\"queue\":%lu,\"qmax\":%lu,\"drops\":%lu,\"sd_errors\":%lu,\"reinit\":%lu,\"fault\":\"%s\","
    "\"tof_enabled\":%s,\"tof_scan\":\"%s\",\"tof_i2c_hz\":%lu,\"tof_sda_gpio\":%d,\"tof_scl_gpio\":%d,"
    "\"tof_frames\":%lu,\"tof_age_ms\":%lu,\"tof_valid_zones\":%u,\"tof_min_mm\":%u,\"tof_median_mm\":%u,\"tof_max_mm\":%u}",
    imu.ready() ? "RUNNING" : "ERROR", imu.address(), static_cast<unsigned long>(kBnoI2cHz),
    imu.accelRate().hz(), imu.gyroRate().hz(), imu.rotationRate().hz(),
    static_cast<unsigned long>(ageMs(sample.accelUs, nowUs)),
    static_cast<unsigned long>(ageMs(sample.gyroUs, nowUs)),
    static_cast<unsigned long>(ageMs(sample.rotationUs, nowUs)),
    sample.ax, sample.ay, sample.az, sample.gx, sample.gy, sample.gz,
    sample.roll, sample.pitch, sample.yaw, logger.stateName(), stats.runName,
    static_cast<unsigned long>(stats.queueUsed), static_cast<unsigned long>(stats.queueHighWater),
    static_cast<unsigned long>(stats.dropped), static_cast<unsigned long>(stats.writeErrors),
    static_cast<unsigned long>(imu.reinitCount()), imu.fault(),
    tofReady ? "true" : "false", tofScan, static_cast<unsigned long>(kTofI2cHz),
    kTofSdaPin, kTofSclPin, static_cast<unsigned long>(tofFrames),
    static_cast<unsigned long>(ageMs(tofLastUs, nowUs)), tofValid, tofMin, tofMedian, tofMax);
  web.send(200, "application/json", json);
}

const char page[] = R"HTML(<!doctype html><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;background:#10151c;color:#e7edf3;margin:12px}.c{background:#1b2430;padding:10px;margin:8px 0;border-radius:8px}button{padding:10px;margin:3px;min-width:88px}button:disabled{opacity:.5}canvas{width:100%;height:150px;background:#080c11}.pending{color:#ffd479}</style><h2>BNO08X + VL53L5CX SD logger</h2><div class=c id=s>loading</div><div class=c><button id=startBtn onclick="cmd('start',this)">Start log</button><button id=stopBtn onclick="cmd('stop',this)">Stop log</button><span class=pending id=commandState>Ready</span></div><div class=c id=d></div><canvas id=plot></canvas><div class=c>ToF: VL53L5CX 8x8 continuous ranging, requested 10 Hz. Graph: valid-zone median distance [mm].</div><script>const plot=document.getElementById('plot'),startBtn=document.getElementById('startBtn'),stopBtn=document.getElementById('stopBtn'),commandState=document.getElementById('commandState');let h=[],polling=false,commandBusy=false;function buttons(v){startBtn.disabled=v;stopBtn.disabled=v}async function cmd(x,b){if(commandBusy)return;commandBusy=true;buttons(true);commandState.textContent=`${x==='start'?'Start':'Stop'} request sending...`;try{let r=await fetch('/api/log/'+x,{method:'POST',cache:'no-store'}),t=await r.text();if(!r.ok)throw Error(t);commandState.textContent=`${x==='start'?'Start':'Stop'} accepted; waiting for device state...`;await u()}catch(e){commandState.textContent=`Command error: ${e.message}`}finally{commandBusy=false;buttons(false)}}function gr(){let w=plot.width=plot.clientWidth*devicePixelRatio,H=plot.height=plot.clientHeight*devicePixelRatio,g=plot.getContext('2d');g.clearRect(0,0,w,H);if(!h.length)return;let lo=Math.min(...h),hi=Math.max(...h),span=Math.max(1,hi-lo);g.strokeStyle='#77e49a';g.beginPath();h.forEach((v,i)=>{let X=i*w/159,Y=H-(v-lo)/span*H*.85-H*.075;i?g.lineTo(X,Y):g.moveTo(X,Y)});g.stroke();g.fillStyle='#b9c8d8';g.fillText(`${Math.round(lo)}..${Math.round(hi)} mm`,8,16)}async function u(){if(polling)return;polling=true;try{let j=await(await fetch('/api/latest',{cache:'no-store'})).json();s.textContent=`BNO ${j.bno} ${j.addr} I2C ${j.i2c_hz}Hz | SD ${j.sd} ${j.run} queue ${j.queue}/${j.qmax} drop ${j.drops} SDerr ${j.sd_errors} reinit ${j.reinit} ${j.fault}`;d.textContent=`Accel ${j.ax.toFixed(3)}, ${j.ay.toFixed(3)}, ${j.az.toFixed(3)} m/s2 age ${j.accel_age_ms}ms | Gyro ${j.gx.toFixed(3)}, ${j.gy.toFixed(3)}, ${j.gz.toFixed(3)} rad/s age ${j.gyro_age_ms}ms | Euler ${j.roll.toFixed(1)}, ${j.pitch.toFixed(1)}, ${j.yaw.toFixed(1)} deg | Hz A/G/R ${j.accel_hz.toFixed(1)}/${j.gyro_hz.toFixed(1)}/${j.rotation_hz.toFixed(1)} | ToF ${j.tof_enabled?'RUNNING':'ERROR'} bus ${j.tof_scan} GPIO${j.tof_sda_gpio}/GPIO${j.tof_scl_gpio} ${j.tof_i2c_hz}Hz frames ${j.tof_frames} age ${j.tof_age_ms}ms valid ${j.tof_valid_zones}/64 min/med/max ${j.tof_min_mm}/${j.tof_median_mm}/${j.tof_max_mm} mm`;if(j.tof_enabled&&j.tof_valid_zones)h.push(j.tof_median_mm);if(h.length>160)h.shift();gr();if(!commandBusy&&j.sd==='LOGGING')commandState.textContent='Recording';if(!commandBusy&&j.sd==='READY')commandState.textContent='Ready'}catch(e){s.textContent='web error: '+e.message}finally{polling=false}}setInterval(u,250);u()</script>)HTML";

void startLog() {
  if (logger.state() != btf::State::Ready) {
    web.send(409, "application/json", "{\"error\":\"logger is not ready\"}");
    return;
  }
  imu.resetRunStats();
  memset(lastLogUs, 0, sizeof(lastLogUs));
  summaryPending = false;
  logger.requestStart();
  web.send(202, "application/json", "{\"requested\":\"start\"}");
}

void stopLog() {
  summaryPending = true;
  logger.requestStop();
  web.send(202, "application/json", "{\"requested\":\"stop\"}");
}

void webBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword);
  web.on("/", HTTP_GET, [] { web.send(200, "text/html; charset=utf-8", page); });
  web.on("/api/latest", HTTP_GET, api);
  web.on("/api/log/start", HTTP_POST, startLog);
  web.on("/api/log/stop", HTTP_POST, stopLog);
  web.on("/favicon.ico", HTTP_GET, [] { web.send(204); });
  web.begin();
}

void summary() {
  if (!summaryPending || logger.isLogging() || logger.state() != btf::State::Ready ||
      millis() - lastSummaryTry < 500) return;
  lastSummaryTry = millis();
  const auto& s = logger.stats();
  char text[1400];
  snprintf(text, sizeof(text),
    "firmware=%s %s\nboard=%s\nstage=2_bno_tof\n"
    "BNO08X=0x%02X SDA=%d SCL=%d INT=%d(passive) RST=%d I2C=%lu\n"
    "VL53L5CX=0x%02X SDA=%d SCL=%d I2C=%lu requested_hz=%lu ready=%d scan=%s frames=%lu valid_zones=%u min_mm=%u median_mm=%u max_mm=%u\n"
    "requested_accel_gyro_hz=200\nrequested_rotation_hz=50\n"
    "accel_count=%lu missing=%lu hz=%.3f max_gap_us=%llu\n"
    "gyro_count=%lu missing=%lu hz=%.3f max_gap_us=%llu\n"
    "rotation_count=%lu missing=%lu hz=%.3f max_gap_us=%llu\n"
    "saved=%lu drop=%lu sd_write_errors=%lu queue_high_water=%lu reinit=%lu fault=%s\nnormal_stop=1\n",
    kFirmwareName, kFirmwareVersion, kBoardName, imu.address(), kBnoSdaPin, kBnoSclPin,
    kBnoIntPin, kBnoRstPin, static_cast<unsigned long>(kBnoI2cHz), kTofAddress,
    kTofSdaPin, kTofSclPin, static_cast<unsigned long>(kTofI2cHz),
    static_cast<unsigned long>(kTofFrequencyHz), tofReady, tofScan,
    static_cast<unsigned long>(tofFrames), tofValid, tofMin, tofMedian, tofMax,
    static_cast<unsigned long>(imu.accelRate().count), static_cast<unsigned long>(imu.accelRate().missing),
    imu.accelRate().hz(), static_cast<unsigned long long>(imu.accelRate().maxGapUs),
    static_cast<unsigned long>(imu.gyroRate().count), static_cast<unsigned long>(imu.gyroRate().missing),
    imu.gyroRate().hz(), static_cast<unsigned long long>(imu.gyroRate().maxGapUs),
    static_cast<unsigned long>(imu.rotationRate().count), static_cast<unsigned long>(imu.rotationRate().missing),
    imu.rotationRate().hz(), static_cast<unsigned long long>(imu.rotationRate().maxGapUs),
    static_cast<unsigned long>(s.saved), static_cast<unsigned long>(s.dropped),
    static_cast<unsigned long>(s.writeErrors), static_cast<unsigned long>(s.queueHighWater),
    static_cast<unsigned long>(imu.reinitCount()), imu.fault());
  if (logger.writeSummary(text)) summaryPending = false;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  logger.begin();
  imu.begin();

  tofWire.begin(kTofSdaPin, kTofSclPin, kTofI2cHz);
  Serial.printf("TOF boot wait %lu ms\n", static_cast<unsigned long>(kTofBootWaitMs));
  delay(kTofBootWaitMs);
  scanTofBus();
  tofReady = tof.begin(kTofAddress, tofWire) &&
             tof.setResolution(static_cast<uint8_t>(SF_VL53L5CX_RANGING_RESOLUTION::RES_8X8)) &&
             tof.setRangingFrequency(kTofFrequencyHz) &&
             tof.setRangingMode(SF_VL53L5CX_RANGING_MODE::CONTINUOUS) &&
             tof.startRanging();

  webBegin();
  Serial.printf("%s %s BNO=%d TOF=%d AP=%s http://%s/\n", kFirmwareName,
                kFirmwareVersion, imu.ready(), tofReady, kApSsid,
                WiFi.softAPIP().toString().c_str());
}

void loop() {
  imu.poll(onSample);
  imu.recover();
  pollTof();
  systemLog();
  logger.service();
  web.handleClient();
  logger.service();
  summary();

  if (millis() - lastStatus >= kStatusIntervalMs) {
    lastStatus = millis();
    Serial.printf("BNO=%d TOF=%d bus=%s frames=%lu A/G/R=%.1f/%.1f/%.1f SD=%s q=%lu drop=%lu err=%lu fault=%s\n",
      imu.ready(), tofReady, tofScan, static_cast<unsigned long>(tofFrames),
      imu.accelRate().hz(), imu.gyroRate().hz(), imu.rotationRate().hz(),
      logger.stateName(), static_cast<unsigned long>(logger.stats().queueUsed),
      static_cast<unsigned long>(logger.stats().dropped),
      static_cast<unsigned long>(logger.stats().writeErrors), imu.fault());
  }
  delay(1);
}