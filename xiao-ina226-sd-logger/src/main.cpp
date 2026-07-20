#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <cmath>
#include <cstring>
#include "app_config.h"
#include "ina226_reader.h"
#include "sd_logger.h"

using namespace cfg;
namespace {
WebServer web(80);
Ina226 ina;
vlog::Logger logger;
InaSample latest;
uint64_t nextSampleUs = 0;
uint64_t lastGoodUs = 0;
uint64_t lastLoggedSampleUs = 0;
uint64_t lastReinitAttemptUs = 0;
uint64_t lastStatusUs = 0;
uint32_t sampleSequence = 0;
uint32_t invalidSamples = 0;
uint32_t maxGapUs = 0;
uint32_t maxI2cUs = 0;
uint32_t i2cErrorsAtStart = 0;
uint32_t reinitializationsAtStart = 0;
bool sensorOnline = false;
bool configLogged = false;
bool summaryPending = false;

void putU32(uint8_t*& p, uint32_t value) { for (int i = 3; i >= 0; --i) *p++ = static_cast<uint8_t>(value >> (i * 8)); }
void putFloat(uint8_t*& p, float value) { memcpy(p, &value, sizeof(value)); p += sizeof(value); }

void scanI2c() {
  Serial.println("I2C scan:");
  bool any = false;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X%s\n", address, address == kAddr ? " (INA226 candidate)" : "");
      any = true;
    }
  }
  if (!any) Serial.println("  no I2C device found");
}

void enqueueConfig() {
  if (!logger.isLogging() || configLogged) return;
  vlog::Event event{};
  event.type = vlog::Type::InaConfig;
  event.flags = sensorOnline ? 1 : 0;
  event.rxUs = esp_timer_get_time();
  uint8_t* p = event.payload;
  putU32(p, kAddr);
  putU32(p, kI2cHz);
  putU32(p, ina.manufacturer);
  putU32(p, ina.die);
  putU32(p, ina.configuration);
  putU32(p, ina.calibration);
  putFloat(p, kShuntOhm);
  putFloat(p, kCurrentLsbA);
  putFloat(p, kPowerLsbW);
  logger.enqueue(event);
  configLogged = true;
}

void enqueueMeasurement() {
  if (!logger.isLogging() || !latest.valid) return;
  vlog::Event event{};
  event.type = vlog::Type::InaMeasurement;
  event.flags = 1;
  event.rxUs = latest.us;
  event.packetSequence = sampleSequence;
  uint8_t* p = event.payload;
  putU32(p, static_cast<uint32_t>(static_cast<int32_t>(latest.rawShunt)));
  putU32(p, latest.rawBus);
  putU32(p, static_cast<uint32_t>(static_cast<int32_t>(latest.rawCurrent)));
  putU32(p, latest.rawPower);
  putU32(p, latest.mask);
  putFloat(p, latest.shuntV);
  putFloat(p, latest.busV);
  putFloat(p, latest.currentShuntA);
  putFloat(p, latest.currentRegA);
  putFloat(p, latest.powerRegW);
  putFloat(p, latest.powerCalcW);
  putU32(p, latest.i2cUs);
  logger.enqueue(event);
}

void enqueueStatus() {
  const uint64_t nowUs = esp_timer_get_time();
  if (!logger.isLogging() || nowUs - lastStatusUs < 1000000ULL) return;
  lastStatusUs = nowUs;
  vlog::Event event{};
  event.type = vlog::Type::InaStatus;
  event.flags = sensorOnline ? 1 : 0;
  event.rxUs = nowUs;
  uint8_t* p = event.payload;
  putU32(p, sampleSequence);
  putU32(p, invalidSamples);
  putU32(p, ina.errors - i2cErrorsAtStart);
  putU32(p, ina.reinit - reinitializationsAtStart);
  putU32(p, maxI2cUs);
  putU32(p, maxGapUs);
  putU32(p, logger.stats().queueUsed);
  putU32(p, logger.stats().dropped);
  putU32(p, logger.stats().writeErrors);
  putU32(p, lastGoodUs ? static_cast<uint32_t>((nowUs >= lastGoodUs ? nowUs - lastGoodUs : 0) / 1000ULL) : UINT32_MAX);
  logger.enqueue(event);
}

void acquire() {
  const uint64_t nowUs = esp_timer_get_time();
  if (nowUs < nextSampleUs) return;
  nextSampleUs += kSampleUs;
  if (nowUs - nextSampleUs > kSampleUs) nextSampleUs = nowUs + kSampleUs;

  InaSample sample{};
  if (ina.read(sample)) {
    latest = sample;
    lastGoodUs = sample.us;
    sensorOnline = true;
    if (logger.isLogging()) {
      if (lastLoggedSampleUs) {
        const uint32_t gap = static_cast<uint32_t>(sample.us - lastLoggedSampleUs);
        if (gap > maxGapUs) maxGapUs = gap;
      }
      lastLoggedSampleUs = sample.us;
      ++sampleSequence;
      if (sample.i2cUs > maxI2cUs) maxI2cUs = sample.i2cUs;
      enqueueMeasurement();
    }
  } else {
    if (logger.isLogging()) ++invalidSamples;
    if (lastGoodUs && nowUs - lastGoodUs > static_cast<uint64_t>(kReinitMs) * 1000ULL &&
        nowUs - lastReinitAttemptUs > static_cast<uint64_t>(kReinitMs) * 1000ULL) {
      lastReinitAttemptUs = nowUs;
      sensorOnline = ina.begin();
      Serial.printf("INA226 reinitialize: %s\n", sensorOnline ? "ok" : "failed");
    }
  }
  enqueueStatus();
}

void jsonLatest() {
  const uint64_t nowUs = esp_timer_get_time();
  const bool fresh = latest.valid && lastGoodUs && (nowUs - lastGoodUs <= static_cast<uint64_t>(kTimeoutMs) * 1000ULL);
  const uint32_t ageMs = lastGoodUs ? static_cast<uint32_t>((nowUs - lastGoodUs) / 1000ULL) : UINT32_MAX;
  const float bus = fresh ? latest.busV : 0.0f;
  const float shuntUv = fresh ? latest.shuntV * 1000000.0f : 0.0f;
  const float currentShunt = fresh ? latest.currentShuntA : 0.0f;
  const float currentReg = fresh ? latest.currentRegA : 0.0f;
  const float powerReg = fresh ? latest.powerRegW : 0.0f;
  const float powerCalc = fresh ? latest.powerCalcW : 0.0f;
  char json[1200];
  snprintf(json, sizeof(json),
    "{\"detected\":%s,\"online\":%s,\"fresh\":%s,\"valid\":%s,\"address\":\"0x44\",\"manufacturer\":\"0x%04X\",\"die\":\"0x%04X\",\"config\":\"0x%04X\",\"calibration\":\"0x%04X\",\"age_ms\":%lu,\"bus_v\":%.6f,\"shunt_uv\":%.3f,\"current_shunt_a\":%.6f,\"current_reg_a\":%.6f,\"power_reg_w\":%.6f,\"power_calc_w\":%.6f,\"sequence\":%lu,\"i2c_errors\":%lu,\"reinitializations\":%lu,\"invalid\":%lu,\"i2c_max_us\":%lu,\"gap_max_us\":%lu,\"sd_state\":\"%s\",\"run\":\"%s\",\"queue\":%lu,\"queue_highwater\":%lu,\"log_drops\":%lu,\"sd_errors\":%lu}",
    (ina.manufacturer == 0x5449 && ina.die == 0x2260) ? "true" : "false", sensorOnline ? "true" : "false", fresh ? "true" : "false", latest.valid ? "true" : "false",
    ina.manufacturer, ina.die, ina.configuration, ina.calibration, static_cast<unsigned long>(ageMs), bus, shuntUv, currentShunt, currentReg, powerReg, powerCalc,
    static_cast<unsigned long>(sampleSequence), static_cast<unsigned long>(ina.errors), static_cast<unsigned long>(ina.reinit), static_cast<unsigned long>(invalidSamples),
    static_cast<unsigned long>(maxI2cUs), static_cast<unsigned long>(maxGapUs), logger.stateName(), logger.stats().runName,
    static_cast<unsigned long>(logger.stats().queueUsed), static_cast<unsigned long>(logger.stats().queueHighWater), static_cast<unsigned long>(logger.stats().dropped), static_cast<unsigned long>(logger.stats().writeErrors));
  web.send(200, "application/json", json);
}

const char kPage[] = R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;margin:0;background:#111820;color:#e9eef5}.c{background:#1c2733;margin:9px;padding:11px;border-radius:8px;white-space:pre-wrap}button{padding:10px 16px;margin-right:8px;font-size:15px}canvas{width:100%;height:180px;background:#0b1118}</style><h2 class=c>INA226 SD logger</h2><div class=c id=s>loading...</div><div class=c><button onclick="cmd('/api/start')">Start log</button><button onclick="cmd('/api/stop')">Stop log</button> Web refresh 4 Hz</div><canvas id=g width=800 height=180></canvas><div class=c id=v></div><script>const h=[];async function cmd(u){try{await fetch(u,{method:'POST'});setTimeout(load,100)}catch(e){s.textContent='request failed'}}function graph(){let c=g.getContext('2d'),w=g.width,H=g.height;c.clearRect(0,0,w,H);c.strokeStyle='#344454';c.beginPath();c.moveTo(0,H/2);c.lineTo(w,H/2);c.stroke();if(h.length<2)return;let mn=Math.min(...h),mx=Math.max(...h);if(mx-mn<.02){mn-=.01;mx+=.01}c.strokeStyle='#55d890';c.beginPath();h.forEach((x,i)=>{let X=i*w/(h.length-1),Y=H-(x-mn)/(mx-mn)*H;i?c.lineTo(X,Y):c.moveTo(X,Y)});c.stroke();c.fillStyle='#e9eef5';c.fillText('Bus voltage '+mn.toFixed(3)+' - '+mx.toFixed(3)+' V',8,16)}async function load(){try{let j=await(await fetch('/api/latest',{cache:'no-store'})).json();s.textContent='INA226 '+(j.detected?'DETECTED':'NOT DETECTED')+' '+j.address+' ID '+j.manufacturer+'/'+j.die+' config '+j.config+' cal '+j.calibration+' | '+j.sd_state+' '+j.run+' queue '+j.queue+' drop '+j.log_drops+' SDerr '+j.sd_errors;v.textContent='valid='+j.valid+' fresh='+j.fresh+' age='+j.age_ms+' ms\nBus '+j.bus_v+' V | Shunt '+j.shunt_uv+' uV\nI(shunt) '+j.current_shunt_a+' A | I(register) '+j.current_reg_a+' A\nP(register) '+j.power_reg_w+' W | P(bus x shunt) '+j.power_calc_w+' W\nseq '+j.sequence+' invalid '+j.invalid+' I2Cerr '+j.i2c_errors+' reinit '+j.reinitializations+' maxI2C '+j.i2c_max_us+' us maxGap '+j.gap_max_us+' us';if(j.fresh){h.push(j.bus_v);if(h.length>120)h.shift();graph()}}catch(e){s.textContent='Web update failed'}}setInterval(load,250);load()</script></html>)HTML";

void startLog() { configLogged = false; lastStatusUs = 0; lastLoggedSampleUs = 0; sampleSequence = 0; invalidSamples = 0; maxGapUs = 0; maxI2cUs = 0; i2cErrorsAtStart = ina.errors; reinitializationsAtStart = ina.reinit; summaryPending = false; logger.requestStart(); web.send(202, "application/json", "{\"accepted\":true}"); }
void stopLog() { logger.requestStop(); summaryPending = true; web.send(202, "application/json", "{\"accepted\":true}"); }

void writeSummaryIfReady() {
  if (!summaryPending || logger.isLogging() || logger.state() != vlog::State::Ready) return;
  char text[768];
  snprintf(text, sizeof(text),
    "firmware=%s\nversion=%s\naddress=0x44\nmanufacturer=0x%04X\ndie=0x%04X\nconfig=0x%04X\ncalibration=0x%04X\ni2c_hz=%lu\nrequested_hz=%lu\nmeasurements=%lu\ninvalid=%lu\ni2c_errors=%lu\nreinitializations=%lu\nmax_i2c_us=%lu\nmax_gap_us=%lu\nqueue_drop=%lu\nsd_write_errors=%lu\nnormal_stop=1\n",
    kFirmwareName, kFirmwareVersion, ina.manufacturer, ina.die, ina.configuration, ina.calibration,
    static_cast<unsigned long>(kI2cHz), static_cast<unsigned long>(kSampleHz), static_cast<unsigned long>(sampleSequence), static_cast<unsigned long>(invalidSamples),
    static_cast<unsigned long>(ina.errors - i2cErrorsAtStart), static_cast<unsigned long>(ina.reinit - reinitializationsAtStart), static_cast<unsigned long>(maxI2cUs), static_cast<unsigned long>(maxGapUs),
    static_cast<unsigned long>(logger.stats().dropped), static_cast<unsigned long>(logger.stats().writeErrors));
  if (logger.writeSummary(text)) summaryPending = false;
}
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.printf("%s %s\nI2C SDA=D1/GPIO%d SCL=D0/GPIO%d %lu Hz, INA226=0x%02X\n", kFirmwareName, kFirmwareVersion, kSda, kScl, static_cast<unsigned long>(kI2cHz), kAddr);
  Wire.begin(kSda, kScl, kI2cHz);
  Wire.setTimeOut(20);
  scanI2c();
  sensorOnline = ina.begin();
  Serial.printf("INA226 init=%s manufacturer=0x%04X die=0x%04X config=0x%04X cal=0x%04X\n", sensorOnline ? "ok" : "failed", ina.manufacturer, ina.die, ina.configuration, ina.calibration);
  logger.begin();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPass);
  Serial.printf("Web AP=%s IP=%s\n", kApSsid, WiFi.softAPIP().toString().c_str());
  web.on("/", HTTP_GET, [] { web.send(200, "text/html; charset=utf-8", kPage); });
  web.on("/api/latest", HTTP_GET, jsonLatest);
  web.on("/api/start", HTTP_POST, startLog);
  web.on("/api/stop", HTTP_POST, stopLog);
  web.begin();
  nextSampleUs = esp_timer_get_time() + kSampleUs;
}

void loop() {
  logger.service();
  enqueueConfig();
  acquire();
  web.handleClient();
  writeSummaryIfReady();
  delay(1);
}