#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <esp_timer.h>
#include <stddef.h>
#include <math.h>
#include <freertos/semphr.h>

#include "app_config.h"
#include "gnss_receiver.h"
#include <boat_protocol.h>

using namespace app_config;
constexpr uint32_t kLogMagic = 0x424C4F47UL;  // Bytes on SD: GOLB.

HardwareSerial controlUart(1);
HardwareSerial gnssUart(2);
TwoWire bnoWire(1);
Adafruit_BNO08x bno08x(-1);
WebServer web(kHttpPort);
gnss::Receiver gnssRx;
sh2_SensorValue_t bnoValue{};
boat::Decoder controlDecoder;

struct BnoLatest {
  bool ready = false, accelValid = false, gyroValid = false, quatValid = false;
  uint8_t address = 0, accelAccuracy = 0, gyroAccuracy = 0, quatAccuracy = 0;
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  float qi = 0, qj = 0, qk = 0, qw = 1, roll = 0, pitch = 0, yaw = 0;
  uint64_t accelUs = 0, gyroUs = 0, quatUs = 0, lastUs = 0;
  uint32_t reinits = 0;
  char fault[48] = "starting";
} bno;

struct LogStats {
  bool sdReady = false, logging = false;
  uint32_t records = 0, writeErrors = 0, queueDrops = 0, controlFrames = 0;
  uint32_t localFrames = 0, controlCrc = 0, controlCobs = 0, controlLength = 0;
  uint16_t queueHighWater = 0;
  char runName[16] = "none";
} logStats;

boat::Frame frameQueue[kFrameQueueDepth];
uint16_t queueHead = 0, queueTail = 0, queueUsed = 0;
portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;
File logFile;
uint8_t sdBuffer[kSdWriteBufferBytes];
size_t sdUsed = 0;
uint32_t commBootId = 0, localSequence = 0, controlTxSequence = 0;
uint32_t navSequence = 0, gnssFixSequence = 0, commandSequence = 0, timeSyncSequence = 0;
uint64_t lastControlFrameUs = 0;
uint32_t lastFlushMs = 0, lastHeartbeatMs = 0, lastBnoRetryMs = 0, lastGnssStatusMs = 0, lastDiagMs = 0, lastNavMs = 0, lastTimeSyncMs = 0;
bool pendingNewFix = false;
struct LinkLatest {
  uint32_t navTx = 0, resultRx = 0, resultBadCrc = 0, commandAckRx = 0, lastCommandId = 0;
  uint64_t lastHeartbeatUs = 0, lastResultUs = 0, lastAckUs = 0, lastRttUs = 0;
  boat::GnssProcessResultPayload result{};
  boat::CommandAckPayload ack{};
} linkLatest;
volatile uint8_t pendingCommand = 0;
SemaphoreHandle_t controlTxMutex = nullptr;
enum PendingCommand : uint8_t { CommandNone, CommandStartLog, CommandStopLog, CommandStop, CommandEstop };

uint64_t nowUs() { return static_cast<uint64_t>(esp_timer_get_time()); }
uint32_t ageMs(uint64_t then, uint64_t now) {
  return then ? static_cast<uint32_t>((now - then) / 1000ULL) : UINT32_MAX;
}

bool enqueueFrame(const boat::Frame& frame) {
  portENTER_CRITICAL(&queueMux);
  if (queueUsed >= kFrameQueueDepth) {
    ++logStats.queueDrops;
    portEXIT_CRITICAL(&queueMux);
    return false;
  }
  frameQueue[queueHead] = frame;
  queueHead = (queueHead + 1) % kFrameQueueDepth;
  ++queueUsed;
  if (queueUsed > logStats.queueHighWater) logStats.queueHighWater = queueUsed;
  portEXIT_CRITICAL(&queueMux);
  return true;
}

bool dequeueFrame(boat::Frame& frame) {
  portENTER_CRITICAL(&queueMux);
  if (!queueUsed) { portEXIT_CRITICAL(&queueMux); return false; }
  frame = frameQueue[queueTail];
  queueTail = (queueTail + 1) % kFrameQueueDepth;
  --queueUsed;
  portEXIT_CRITICAL(&queueMux);
  return true;
}

void emitLocal(boat::Type type, const void* payload, uint16_t length, uint16_t flags = 0) {
  if (!logStats.logging || length > boat::kMaxPayload) return;
  boat::Frame frame{};
  frame.header = {boat::kVersion, static_cast<uint8_t>(type), length, ++localSequence,
                  commBootId, nowUs(), flags};
  if (length) memcpy(frame.payload, payload, length);
  if (enqueueFrame(frame)) ++logStats.localFrames;
}

bool appendBytes(const void* data, size_t length) {
  const uint8_t* source = static_cast<const uint8_t*>(data);
  while (length) {
    const size_t space = sizeof(sdBuffer) - sdUsed;
    const size_t part = length < space ? length : space;
    memcpy(sdBuffer + sdUsed, source, part);
    sdUsed += part; source += part; length -= part;
    if (sdUsed == sizeof(sdBuffer)) {
      if (logFile.write(sdBuffer, sdUsed) != sdUsed) { ++logStats.writeErrors; return false; }
      sdUsed = 0;
    }
  }
  return true;
}

void flushLog() {
  if (!logFile || !sdUsed) return;
  if (logFile.write(sdBuffer, sdUsed) != sdUsed) ++logStats.writeErrors;
  sdUsed = 0;
  logFile.flush();
  lastFlushMs = millis();
}

bool startLog() {
  if (!logStats.sdReady || logStats.logging) return logStats.logging;
  SD.mkdir(kLogDirectory);
  char path[32];
  for (uint16_t index = 1; index < 10000; ++index) {
    snprintf(logStats.runName, sizeof(logStats.runName), "RUN%04u.BIN", index);
    snprintf(path, sizeof(path), "%s/%s", kLogDirectory, logStats.runName);
    if (!SD.exists(path)) { logFile = SD.open(path, FILE_WRITE); break; }
  }
  if (!logFile) { ++logStats.writeErrors; snprintf(logStats.runName, sizeof(logStats.runName), "open-error"); return false; }
  logStats.logging = true;
  Serial.printf("LOG started: %s/%s\n", kLogDirectory, logStats.runName);
  return true;
}

void writeRunSummary() {
  if (!logStats.sdReady || !strcmp(logStats.runName, "none")) return;
  char path[32]{}; snprintf(path,sizeof(path),"%s/%s",kLogDirectory,logStats.runName);
  char* extension=strstr(path,".BIN"); if (!extension) return; memcpy(extension,".TXT",5);
  File summary=SD.open(path,FILE_WRITE); if (!summary) { ++logStats.writeErrors; return; }
  summary.printf("firmware=%s\nversion=%s\nprotocol=%u\nnormal_stop=1\nrecords=%lu\nqueue_drops=%lu\nsd_write_errors=%lu\ngnss_nav_tx=%lu\ngnss_result_rx=%lu\nresult_bad_crc=%lu\nlast_rtt_us=%llu\ncommand_ack_rx=%lu\n",kFirmwareName,kFirmwareVersion,boat::kVersion,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)linkLatest.resultBadCrc,(unsigned long long)linkLatest.lastRttUs,(unsigned long)linkLatest.commandAckRx);
  summary.close();
}

void stopLog() {
  if (!logStats.logging) return;
  boat::Frame frame{};
  while (dequeueFrame(frame)) {
    appendBytes(&kLogMagic, sizeof(kLogMagic));
    const uint64_t receivedUs = nowUs();
    appendBytes(&receivedUs, sizeof(receivedUs));
    appendBytes(&frame.header, sizeof(frame.header));
    appendBytes(frame.payload, frame.header.length);
    ++logStats.records;
  }
  flushLog();
  logFile.close(); logStats.logging = false; writeRunSummary();
  Serial.printf("LOG stopped: records=%lu errors=%lu\n", (unsigned long)logStats.records, (unsigned long)logStats.writeErrors);
}

void serviceLog() {
  if (!logStats.logging) return;
  boat::Frame frame{};
  for (uint8_t i = 0; i < 32 && dequeueFrame(frame); ++i) {
    const uint64_t receivedUs = nowUs();
    if (!appendBytes(&kLogMagic, sizeof(kLogMagic)) || !appendBytes(&receivedUs, sizeof(receivedUs)) ||
        !appendBytes(&frame.header, sizeof(frame.header)) || !appendBytes(frame.payload, frame.header.length)) {
      continue;
    }
    ++logStats.records;
  }
  if (millis() - lastFlushMs >= kSdFlushIntervalMs) flushLog();
}

void controlRxTask(void*) {
  for (;;) {
    while (controlUart.available()) {
      boat::Frame frame{};
      if (controlDecoder.feed(static_cast<uint8_t>(controlUart.read()), frame)) {
        ++logStats.controlFrames;
        lastControlFrameUs = nowUs();
        const boat::Type type = static_cast<boat::Type>(frame.header.type);
        if (type == boat::Type::Heartbeat) linkLatest.lastHeartbeatUs = lastControlFrameUs;
        if (type == boat::Type::GnssProcessResult && frame.header.length == sizeof(boat::GnssProcessResultPayload)) {
          boat::GnssProcessResultPayload result{}; memcpy(&result, frame.payload, sizeof(result));
          if (result.canonicalCrc == boat::canonicalCrc(&result, offsetof(boat::GnssProcessResultPayload, canonicalCrc))) { linkLatest.result=result; ++linkLatest.resultRx; linkLatest.lastResultUs=lastControlFrameUs; }
          else ++linkLatest.resultBadCrc;
        }
        if (type == boat::Type::CommandAck && frame.header.length == sizeof(boat::CommandAckPayload)) { memcpy(&linkLatest.ack,frame.payload,sizeof(linkLatest.ack)); linkLatest.lastCommandId=linkLatest.ack.commandId; ++linkLatest.commandAckRx; linkLatest.lastAckUs=lastControlFrameUs; }
        if (type == boat::Type::TimeSyncReply && frame.header.length == sizeof(boat::TimeSyncReplyPayload)) { boat::TimeSyncReplyPayload reply{}; memcpy(&reply,frame.payload,sizeof(reply)); if (reply.t1Us) linkLatest.lastRttUs=lastControlFrameUs-reply.t1Us; }
        if (logStats.logging) enqueueFrame(frame);
      }
    }
    logStats.controlCrc = controlDecoder.crcErrors;
    logStats.controlCobs = controlDecoder.cobsErrors;
    logStats.controlLength = controlDecoder.lengthErrors;
    vTaskDelay(1);
  }
}

void setBnoFault(const char* text) { snprintf(bno.fault, sizeof(bno.fault), "%s", text); }
bool bnoReports() {
  return bno08x.enableReport(SH2_ACCELEROMETER, kAccelGyroIntervalUs) &&
         bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, kAccelGyroIntervalUs) &&
         bno08x.enableReport(SH2_GAME_ROTATION_VECTOR, kRotationIntervalUs);
}
bool startBno() {
  bno.ready = false; bnoWire.begin(kBnoSdaPin, kBnoSclPin, kBnoI2cHz);
  for (uint8_t address : {kBnoAddressPrimary, kBnoAddressAlternate}) {
    if (bno08x.begin_I2C(address, &bnoWire) && bnoReports()) {
      bno.ready = true; bno.address = address; bno.lastUs = nowUs(); setBnoFault("none");
      return true;
    }
  }
  setBnoFault("BNO08X init/report failed");
  return false;
}

struct __attribute__((packed)) BnoPayload {
  uint8_t kind, accuracy, sequence, reserved;
  uint64_t sensorUs;
  float v[7];
};

void updateEuler() {
  const float norm = sqrtf(bno.qi*bno.qi + bno.qj*bno.qj + bno.qk*bno.qk + bno.qw*bno.qw);
  if (norm < 0.00001f) return;
  const float x=bno.qi/norm, y=bno.qj/norm, z=bno.qk/norm, w=bno.qw/norm;
  bno.roll = atan2f(2*(w*x+y*z), 1-2*(x*x+y*y))*180.0f/PI;
  bno.pitch = asinf(constrain(2*(w*y-z*x), -1.0f, 1.0f))*180.0f/PI;
  bno.yaw = atan2f(2*(w*z+x*y), 1-2*(y*y+z*z))*180.0f/PI;
}

void serviceBno() {
  if (!bno.ready) {
    if (millis() - lastBnoRetryMs >= kBnoReinitIntervalMs) { lastBnoRetryMs = millis(); ++bno.reinits; startBno(); }
    return;
  }
  if (bno08x.wasReset() && !bnoReports()) { bno.ready = false; setBnoFault("BNO08X reset"); return; }
  for (uint8_t i=0; i<16 && bno08x.getSensorEvent(&bnoValue); ++i) {
    BnoPayload payload{};
    payload.accuracy = bnoValue.status & 3; payload.sequence = bnoValue.sequence; payload.sensorUs = bnoValue.timestamp;
    const uint64_t receivedUs = nowUs();
    if (bnoValue.sensorId == SH2_ACCELEROMETER) {
      payload.kind=1; payload.v[0]=bnoValue.un.accelerometer.x; payload.v[1]=bnoValue.un.accelerometer.y; payload.v[2]=bnoValue.un.accelerometer.z;
      bno.ax=payload.v[0]; bno.ay=payload.v[1]; bno.az=payload.v[2]; bno.accelAccuracy=payload.accuracy; bno.accelUs=receivedUs; bno.accelValid=true;
      emitLocal(boat::Type::BnoAccel, &payload, sizeof(payload));
    } else if (bnoValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
      payload.kind=2; payload.v[0]=bnoValue.un.gyroscope.x; payload.v[1]=bnoValue.un.gyroscope.y; payload.v[2]=bnoValue.un.gyroscope.z;
      bno.gx=payload.v[0]; bno.gy=payload.v[1]; bno.gz=payload.v[2]; bno.gyroAccuracy=payload.accuracy; bno.gyroUs=receivedUs; bno.gyroValid=true;
      emitLocal(boat::Type::BnoGyro, &payload, sizeof(payload));
    } else if (bnoValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
      payload.kind=3; payload.v[0]=bnoValue.un.gameRotationVector.i; payload.v[1]=bnoValue.un.gameRotationVector.j; payload.v[2]=bnoValue.un.gameRotationVector.k; payload.v[3]=bnoValue.un.gameRotationVector.real;
      bno.qi=payload.v[0]; bno.qj=payload.v[1]; bno.qk=payload.v[2]; bno.qw=payload.v[3]; updateEuler(); payload.v[4]=bno.roll; payload.v[5]=bno.pitch; payload.v[6]=bno.yaw;
      bno.quatAccuracy=payload.accuracy; bno.quatUs=receivedUs; bno.quatValid=true;
      emitLocal(boat::Type::BnoQuaternion, &payload, sizeof(payload));
    } else continue;
    bno.lastUs = receivedUs;
  }
  if (ageMs(bno.lastUs, nowUs()) > kBnoNoDataTimeoutMs) { bno.ready=false; setBnoFault("BNO08X no data"); }
}

struct __attribute__((packed)) GnssRawPayload { uint8_t rawLength, checksumValid, parsed, reserved; char type[6]; char raw[kGnssMaxSentenceChars]; };
struct __attribute__((packed)) GnssFixPayload { uint32_t flags; double latitude, longitude; float altitudeM, speedMps, courseDeg, hdop; uint8_t satellites, fixType; char utcTime[12], utcDate[8], lastType[7]; uint64_t endUs, parseUs; };
struct __attribute__((packed)) GnssStatusPayload { uint32_t bytes, sentences, valid, checksumErrors, parseErrors, logDrops, sentenceAgeMs, fixAgeMs; };

void logGnssSentence(const gnss::Sentence& sentence) {
  GnssRawPayload raw{}; raw.rawLength = min<uint8_t>(sentence.rawLength, kGnssMaxSentenceChars);
  raw.checksumValid = sentence.checksumValid; raw.parsed = sentence.parsed;
  strncpy(raw.type, sentence.type, sizeof(raw.type)-1); memcpy(raw.raw, sentence.raw, raw.rawLength);
  emitLocal(boat::Type::GnssRaw, &raw, sizeof(raw));
  if (!sentence.parsed) return;
  const auto& latest = gnssRx.latest();
  GnssFixPayload fix{}; fix.flags=latest.flags; fix.latitude=latest.latitude; fix.longitude=latest.longitude; fix.altitudeM=latest.altitudeM; fix.speedMps=latest.speedMps; fix.courseDeg=latest.courseDeg; fix.hdop=latest.hdop; fix.satellites=latest.satellites; fix.fixType=latest.fixType; strncpy(fix.utcTime,latest.utcTime,sizeof(fix.utcTime)-1); strncpy(fix.utcDate,latest.utcDate,sizeof(fix.utcDate)-1); strncpy(fix.lastType,latest.lastType,sizeof(fix.lastType)-1); fix.endUs=latest.lastSentenceEndUs; fix.parseUs=latest.lastParseUs;
  emitLocal(boat::Type::GnssFix, &fix, sizeof(fix));
  ++gnssFixSequence;
  pendingNewFix = true;
}

boat::GnssNavPayload makeNavPayload() {
  const auto& latest = gnssRx.latest();
  boat::GnssNavPayload nav{};
  nav.navSequence = ++navSequence;
  nav.fixSequence = gnssFixSequence;
  if (latest.flags & gnss::FixValid) nav.flags |= boat::NavFixValid;
  if (pendingNewFix) nav.flags |= boat::NavNewFix;
  if (latest.flags & gnss::LatitudeValid) nav.flags |= boat::NavLatValid;
  if (latest.flags & gnss::LongitudeValid) nav.flags |= boat::NavLonValid;
  if (latest.flags & gnss::AltitudeValid) nav.flags |= boat::NavAltitudeValid;
  if (latest.flags & gnss::SpeedValid) nav.flags |= boat::NavSpeedValid;
  if (latest.flags & gnss::CourseValid) nav.flags |= boat::NavCourseValid;
  if (latest.flags & gnss::HdopValid) nav.flags |= boat::NavHdopValid;
  nav.latitudeE7 = static_cast<int32_t>(lround(latest.latitude * 1e7));
  nav.longitudeE7 = static_cast<int32_t>(lround(latest.longitude * 1e7));
  nav.altitudeMm = static_cast<int32_t>(lround(latest.altitudeM * 1000));
  nav.speedMmPerSec = static_cast<int32_t>(lround(latest.speedMps * 1000));
  nav.courseE5Deg = static_cast<int32_t>(lround(latest.courseDeg * 100000));
  nav.hdopCenti = static_cast<uint16_t>(constrain(lround(latest.hdop * 100), 0L, 65535L));
  nav.satellites = latest.satellites; nav.fixType = latest.fixType; nav.generatedUs = nowUs();
  nav.canonicalCrc = boat::canonicalCrc(&nav, offsetof(boat::GnssNavPayload, canonicalCrc));
  return nav;
}

void serviceGnss() {
  for (uint16_t count=0; count<kGnssReadBudgetBytes && gnssUart.available(); ++count) {
    const int value=gnssUart.read(); if (value < 0) break;
    gnss::Sentence sentence{}; if (gnssRx.feed(static_cast<char>(value), nowUs(), sentence)) logGnssSentence(sentence);
  }
  gnssRx.expire(nowUs());
  if (millis() - lastGnssStatusMs >= kGnssStatusIntervalMs) {
    lastGnssStatusMs = millis(); const auto window=gnssRx.takeWindow(); const auto& latest=gnssRx.latest();
    GnssStatusPayload status{window.bytes,window.sentences,window.validSentences,window.checksumErrors,window.parseErrors,window.logDrops,ageMs(latest.lastSentenceEndUs,nowUs()),ageMs(latest.lastValidFixUs,nowUs())};
    emitLocal(boat::Type::GnssStatus,&status,sizeof(status));
  }
}

void sendControl(boat::Type type, const void* payload = nullptr, uint16_t payloadLength = 0) {
  // This is a separate direction of the link. Keep SD-log sequence numbers
  // contiguous so an offline parser can use gaps as loss evidence.
  boat::Header header{boat::kVersion,static_cast<uint8_t>(type),payloadLength,++controlTxSequence,commBootId,nowUs(),0};
  uint8_t encoded[boat::kMaxEncoded]; const size_t length=boat::encode(header,static_cast<const uint8_t*>(payload),encoded,sizeof(encoded));
  if (!length) return;
  if (controlTxMutex && xSemaphoreTake(controlTxMutex,pdMS_TO_TICKS(20)) != pdTRUE) return;
  controlUart.write(encoded,length);
  if (controlTxMutex) xSemaphoreGive(controlTxMutex);
}

void sendGnssNav() {
  const boat::GnssNavPayload nav = makeNavPayload();
  sendControl(boat::Type::GnssNav, &nav, sizeof(nav));
  emitLocal(boat::Type::GnssNav, &nav, sizeof(nav), 1); ++linkLatest.navTx; pendingNewFix = false;
}
void gnssNavTask(void*) { TickType_t last=xTaskGetTickCount(); for(;;) { sendGnssNav(); vTaskDelayUntil(&last,pdMS_TO_TICKS(kGnssNavIntervalMs)); } }
void serviceTimeSync() {
  if (millis() - lastTimeSyncMs < kTimeSyncIntervalMs) return;
  lastTimeSyncMs = millis(); boat::TimeSyncRequestPayload request{++timeSyncSequence, nowUs()};
  sendControl(boat::Type::TimeSyncRequest, &request, sizeof(request));
  emitLocal(boat::Type::TimeSyncRequest, &request, sizeof(request), 1);
}
void serviceCommands() {
  const uint8_t command=pendingCommand; pendingCommand=CommandNone;
  if (command==CommandStartLog) startLog(); else if (command==CommandStopLog) stopLog();
  else if (command==CommandStop || command==CommandEstop) { boat::CommandPayload request{++commandSequence, static_cast<uint8_t>(command==CommandStop ? boat::Type::Stop : boat::Type::Estop), {0,0,0}}; const boat::Type type=command==CommandStop?boat::Type::Stop:boat::Type::Estop; sendControl(type,&request,sizeof(request)); emitLocal(type,&request,sizeof(request),1); }
  if (millis()-lastHeartbeatMs >= kControlHeartbeatIntervalMs) { lastHeartbeatMs=millis(); boat::HeartbeatPayload heartbeat{millis(),controlTxSequence,0,1,0}; sendControl(boat::Type::Heartbeat,&heartbeat,sizeof(heartbeat)); emitLocal(boat::Type::Heartbeat,&heartbeat,sizeof(heartbeat),1); }
}

const char page[] PROGMEM = R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;margin:12px;background:#101720;color:#edf3fa}.card{background:#1d2a38;border-radius:9px;padding:10px;margin:9px 0;white-space:pre-wrap}button{padding:9px;margin:2px}canvas{width:100%;height:130px;background:#080d13}</style><h2>ボート統合テレメトリ</h2><div class=card id=status>読み込み中</div><div class=card id=detail></div><div class=card><button onclick="post('/api/log/start')">記録開始</button><button onclick="post('/api/log/stop')">記録停止</button><button onclick="post('/api/control/stop')">STOP</button><button onclick="post('/api/control/estop')">E-STOP</button></div><canvas id=graph></canvas><script>let h=[];async function post(u){await fetch(u,{method:'POST'})}function draw(){let c=graph,w=c.width=c.clientWidth*devicePixelRatio,H=c.height=c.clientHeight*devicePixelRatio,x=c.getContext('2d');x.clearRect(0,0,w,H);x.strokeStyle='#79e5a0';x.beginPath();let m=Math.max(1,...h.map(Math.abs));h.forEach((v,i)=>{let X=i*w/159,Y=H/2-v/m*H*.42;i?x.lineTo(X,Y):x.moveTo(X,Y)});x.stroke()}async function update(){try{let j=await(await fetch('/api/latest',{cache:'no-store'})).json();status.textContent=`SD: ${j.sd}  記録: ${j.logging} (${j.run})\n制御リンク: ${j.control_age_ms} ms / frames ${j.control_frames} / error ${j.control_errors}\nBNO: ${j.bno} ${j.bno_fault}  age A/G/Q: ${j.accel_age_ms}/${j.gyro_age_ms}/${j.quat_age_ms} ms`;detail.textContent=`GNSS: ${j.gnss_receiving?'受信中':'未受信'}  fix: ${j.gnss_fix}  age: ${j.gnss_age_ms} ms\n緯度: ${j.lat_valid?j.lat.toFixed(7):'無効'}  経度: ${j.lon_valid?j.lon.toFixed(7):'無効'}\n高度: ${j.alt_valid?j.alt_m.toFixed(1)+' m':'無効'}  速度: ${j.speed_valid?j.speed_mps.toFixed(2)+' m/s':'無効'}  衛星: ${j.sats_valid?j.sats:'無効'}\n比較BNO Accel Z: ${j.az.toFixed(3)} m/s²  queue/drop: ${j.queue}/${j.drops}`;h.push(j.az);if(h.length>160)h.shift();draw()}catch(e){status.textContent='更新エラー'}}setInterval(update,50);update()</script></html>)HTML";

const char linkPage[] PROGMEM = R"HTML(<!doctype html><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;background:#101720;color:#edf3fa;margin:12px}.card{background:#1d2a38;border-radius:8px;padding:10px;margin:8px 0;white-space:pre-wrap}button{padding:8px;margin:2px}canvas{width:100%;height:120px;background:#080d13}</style><h2>Boat GNSS round-trip / DRY RUN</h2><div class=card id=s>loading</div><div class=card><button onclick="post('/api/log/start')">Start log</button><button onclick="post('/api/log/stop')">Stop log</button><button onclick="post('/api/control/stop')">STOP</button><button onclick="post('/api/control/estop')">E-STOP</button></div><canvas id=g></canvas><script>let a=[];async function post(u){await fetch(u,{method:'POST'})}function draw(){let w=g.width=g.clientWidth*devicePixelRatio,h=g.height=g.clientHeight*devicePixelRatio,c=g.getContext('2d');c.clearRect(0,0,w,h);let m=Math.max(1,...a.map(Math.abs));c.strokeStyle='#79e5a0';c.beginPath();a.forEach((v,i)=>{let x=i*w/159,y=h/2-v/m*h*.42;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke()}async function load(){try{let j=await(await fetch('/api/link',{cache:'no-store'})).json();s.textContent=`SD ${j.sd}, logging ${j.logging}, run ${j.run}, records ${j.records}\nGNSS ${j.gnss_fix?'valid fix':'no fix'} age ${j.gnss_age_ms} ms: ${j.lat.toFixed(7)}, ${j.lon.toFixed(7)}, HDOP ${j.hdop.toFixed(2)}\nGNSS_NAV TX ${j.nav_tx}; result RX ${j.result_rx}; result age ${j.result_age_ms} ms; RTT ${j.rtt_ms} ms\nControl link ${j.link_healthy?'healthy':'STALE'}; CRC bad ${j.result_bad_crc}; ACK id ${j.ack_id}, age ${j.ack_age_ms} ms\nDRY_RUN ${j.result_dry_run}; control state ${j.control_state}`;a.push(j.north_m);if(a.length>160)a.shift();draw()}catch(e){s.textContent='update failed'}}setInterval(load,250);load()</script>)HTML";

const char clearPage[] PROGMEM = R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.card{background:#1d2a38;border-radius:10px;padding:12px;margin:10px 0}.pill{display:inline-block;padding:4px 8px;border-radius:99px;margin:2px;background:#46576a}.ok{background:#176b42}.bad{background:#8d3039}.wait{background:#8b641b}button{font:inherit;padding:12px 10px;margin:3px;border:0;border-radius:8px;background:#2f8fce;color:white}button:disabled{opacity:.45}.danger{background:#b32d3b}pre{margin:0;white-space:pre-wrap;font:14px ui-monospace,monospace}canvas{width:100%;height:110px;background:#090d12}</style><h2>ボート統合テレメトリ</h2><div class=card><span id=sd class=pill>SD確認中</span><span id=link class=pill>制御リンク確認中</span><span id=dry class=pill>DRY_RUN確認中</span><div id=feedback class=wait style="margin-top:8px;padding:8px;border-radius:6px">状態を取得中です。</div></div><div class=card><b>記録</b><br><button id=start onclick="act('/api/log/start','記録開始','start')">記録を開始</button><button id=stop onclick="act('/api/log/stop','記録停止','stop')">記録を停止</button><div id=loghint></div></div><div class=card><b>安全操作（制御側へ送信）</b><br><button id=bstop class=danger onclick="act('/api/control/stop','STOP','ack')">STOP</button><button id=bestop class=danger onclick="act('/api/control/estop','E-STOP','ack')">E-STOP</button><div>操作を押すと「送信要求」→「制御側ACK受信」を表示します。</div></div><div class=card><b>現在の状態</b><pre id=data>読み込み中</pre></div><canvas id=graph></canvas><script>let h=[],last={},pending=null;const state=['BOOT','DISARMED','ARMED_IDLE','RUNNING','E_STOP','FAULT'];function pill(e,ok,text){e.className='pill '+(ok?'ok':'bad');e.textContent=text}function say(t,c='wait'){feedback.className=c;feedback.textContent=t}function buttons(x){for(let e of document.querySelectorAll('button'))e.disabled=x}async function act(url,label,kind){buttons(true);pending={label,kind,ack:last.ack_id||0,at:Date.now()};say(label+'：通信側へ送信要求を登録しました。','wait');try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(r.status)}catch(e){pending=null;say(label+'：送信要求に失敗しました。','bad');buttons(false)}}function draw(){let w=graph.width=graph.clientWidth*devicePixelRatio,H=graph.height=graph.clientHeight*devicePixelRatio,c=graph.getContext('2d');c.clearRect(0,0,w,H);let m=Math.max(1,...h.map(Math.abs));c.strokeStyle='#78e39d';c.beginPath();h.forEach((v,i)=>{let x=i*w/159,y=H/2-v/m*H*.42;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke()}function confirm(j){if(!pending)return;if(pending.kind==='start'&&j.logging){say('記録開始：反映済みです。','ok');pending=null;buttons(false)}else if(pending.kind==='stop'&&!j.logging){say('記録停止：SDを閉じ、TXT概要を作成しました。','ok');pending=null;buttons(false)}else if(pending.kind==='ack'&&j.ack_id>pending.ack){let s=state[j.ack_state]||j.ack_state;say(pending.label+'：制御側ACK受信。状態は '+s+' です。','ok');pending=null;buttons(false)}else if(Date.now()-pending.at>600){say(pending.label+'：制御側の応答を待っています…','wait')}}async function load(){try{let j=await(await fetch('/api/ui',{cache:'no-store'})).json();last=j;pill(sd,j.sd==='ready','SD: '+j.sd);pill(link,j.link_healthy,'制御リンク: '+(j.link_healthy?'正常':'未接続/停止'));pill(dry,j.dry_run,'DRY_RUN: '+(j.dry_run?'有効':'無効'));loghint.textContent='現在: '+(j.logging?'記録中 '+j.run:'停止中')+' / '+j.records+' 件';data.textContent=`GNSS: ${j.gnss_fix?'有効fix':'fixなし'}  age ${j.gnss_age_ms} ms\n緯度 ${j.lat.toFixed(7)}  経度 ${j.lon.toFixed(7)}  HDOP ${j.hdop.toFixed(2)}\nGNSS_NAV送信 ${j.nav_tx} / 結果返信 ${j.result_rx} / RTT ${j.rtt_ms} ms\n制御状態 ${state[j.control_state]||j.control_state} / 結果age ${j.result_age_ms} ms\nACK id ${j.ack_id} / ACK age ${j.ack_age_ms} ms / SD drop ${j.queue_drops} / SD error ${j.sd_errors}`;h.push(j.north_m);if(h.length>160)h.shift();draw();confirm(j)}catch(e){say('画面の状態取得に失敗しました。接続を確認してください。','bad')}}setInterval(load,250);load()</script></html>)HTML";

void apiUi() { const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); char json[1600]; snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"queue_drops\":%lu,\"sd_errors\":%lu,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"hdop\":%.2f,\"nav_tx\":%lu,\"result_rx\":%lu,\"result_age_ms\":%lu,\"rtt_ms\":%lu,\"link_healthy\":%s,\"dry_run\":%s,\"control_state\":%u,\"north_m\":%.3f,\"ack_id\":%lu,\"ack_state\":%u,\"ack_age_ms\":%lu}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.hdop,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)ageMs(linkLatest.lastResultUs,now),(unsigned long)(linkLatest.lastRttUs/1000ULL),ageMs(linkLatest.lastHeartbeatUs,now)<=kControlLinkTimeoutMs?"true":"false",linkLatest.result.dryRun?"true":"false",linkLatest.result.safetyState,linkLatest.result.northMm/1000.0f,(unsigned long)linkLatest.lastCommandId,linkLatest.ack.safetyState,(unsigned long)ageMs(linkLatest.lastAckUs,now)); web.send(200,"application/json",json); }

void apiLink() { const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); char json[1500]; snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"hdop\":%.2f,\"nav_tx\":%lu,\"result_rx\":%lu,\"result_age_ms\":%lu,\"rtt_ms\":%lu,\"link_healthy\":%s,\"result_bad_crc\":%lu,\"result_dry_run\":%u,\"control_state\":%u,\"north_m\":%.3f,\"ack_id\":%lu,\"ack_age_ms\":%lu}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.hdop,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)ageMs(linkLatest.lastResultUs,now),(unsigned long)(linkLatest.lastRttUs/1000ULL),ageMs(linkLatest.lastHeartbeatUs,now)<=kControlLinkTimeoutMs?"true":"false",(unsigned long)linkLatest.resultBadCrc,linkLatest.result.dryRun,linkLatest.result.safetyState,linkLatest.result.northMm/1000.0f,(unsigned long)linkLatest.lastCommandId,(unsigned long)ageMs(linkLatest.lastAckUs,now)); web.send(200,"application/json",json); }

void apiLatest() {
  const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); uint16_t used; portENTER_CRITICAL(&queueMux); used=queueUsed; portEXIT_CRITICAL(&queueMux);
  char json[1800];
  snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"queue\":%u,\"drops\":%lu,\"control_frames\":%lu,\"control_errors\":%lu,\"control_age_ms\":%lu,\"bno\":%s,\"bno_fault\":\"%s\",\"accel_age_ms\":%lu,\"gyro_age_ms\":%lu,\"quat_age_ms\":%lu,\"ax\":%.5f,\"ay\":%.5f,\"az\":%.5f,\"gnss_receiving\":%s,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"alt_m\":%.2f,\"speed_mps\":%.3f,\"course_deg\":%.2f,\"sats\":%u,\"hdop\":%.2f,\"lat_valid\":%s,\"lon_valid\":%s,\"alt_valid\":%s,\"speed_valid\":%s,\"sats_valid\":%s}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,used,(unsigned long)logStats.queueDrops,(unsigned long)logStats.controlFrames,(unsigned long)(logStats.controlCrc+logStats.controlCobs+logStats.controlLength),(unsigned long)ageMs(lastControlFrameUs,now),bno.ready?"true":"false",bno.fault,(unsigned long)ageMs(bno.accelUs,now),(unsigned long)ageMs(bno.gyroUs,now),(unsigned long)ageMs(bno.quatUs,now),bno.ax,bno.ay,bno.az,gnssRx.receiving(now)?"true":"false",(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.altitudeM,g.speedMps,g.courseDeg,g.satellites,g.hdop,(g.flags&gnss::LatitudeValid)?"true":"false",(g.flags&gnss::LongitudeValid)?"true":"false",(g.flags&gnss::AltitudeValid)?"true":"false",(g.flags&gnss::SpeedValid)?"true":"false",(g.flags&gnss::SatellitesValid)?"true":"false");
  web.send(200,"application/json",json);
}
void requestStart() { pendingCommand=CommandStartLog; web.send(202,"application/json","{\"requested\":\"start\"}"); }
void requestStop() { pendingCommand=CommandStopLog; web.send(202,"application/json","{\"requested\":\"stop\"}"); }
void requestControlStop() { pendingCommand=CommandStop; web.send(202,"application/json","{\"requested\":\"stop\"}"); }
void requestControlEstop() { pendingCommand=CommandEstop; web.send(202,"application/json","{\"requested\":\"estop\"}"); }

void beginWeb() {
  WiFi.persistent(false); WiFi.mode(WIFI_AP); WiFi.softAP(kApSsid,kApPassword);
  web.on("/",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",clearPage); }); web.on("/api/latest",HTTP_GET,apiLatest); web.on("/api/link",HTTP_GET,apiLink); web.on("/api/ui",HTTP_GET,apiUi);
  web.on("/api/log/start",HTTP_POST,requestStart); web.on("/api/log/stop",HTTP_POST,requestStop);
  web.on("/api/control/stop",HTTP_POST,requestControlStop); web.on("/api/control/estop",HTTP_POST,requestControlEstop); web.begin();
}

void setup() {
  Serial.begin(115200); delay(300); commBootId=esp_random();
  controlUart.setRxBufferSize(kControlUartRxBufferBytes); controlUart.begin(kControlUartBaud,SERIAL_8N1,kControlUartRxPin,kControlUartTxPin); controlTxMutex=xSemaphoreCreateMutex();
  gnssUart.setRxBufferSize(kGnssUartRxBufferBytes); gnssRx.begin(gnssUart);
  SPI.begin(kSdSckPin,kSdMisoPin,kSdMosiPin,kSdCsPin); logStats.sdReady=SD.begin(kSdCsPin,SPI);
  startBno(); xTaskCreatePinnedToCore(controlRxTask,"ControlRx",4096,nullptr,2,nullptr,0); xTaskCreatePinnedToCore(gnssNavTask,"GnssNavTx",4096,nullptr,2,nullptr,1); beginWeb(); startLog();
  Serial.printf("%s %s boot=%lu SD=%d AP=%s URL=http://%s/\n",kFirmwareName,kFirmwareVersion,(unsigned long)commBootId,logStats.sdReady,kApSsid,WiFi.softAPIP().toString().c_str());
}
void loop() {
  serviceBno(); serviceGnss(); serviceTimeSync(); serviceCommands(); serviceLog(); web.handleClient(); serviceLog();
  if (millis()-lastDiagMs >= kDiagnosticIntervalMs) { lastDiagMs=millis(); Serial.printf("SD=%d log=%d rec=%lu q=%u drop=%lu control=%lu GNSS=%d BNO=%d\n",logStats.sdReady,logStats.logging,(unsigned long)logStats.records,queueUsed,(unsigned long)logStats.queueDrops,(unsigned long)logStats.controlFrames,gnssRx.receiving(nowUs()),bno.ready); }
  delay(1);
}
