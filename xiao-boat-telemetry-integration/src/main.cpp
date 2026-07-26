#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <stddef.h>
#include <math.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

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

boat::Decoder controlDecoder;

struct BnoLatest {
  bool ready = false, accelValid = false, gyroValid = false, quatValid = false, magneticValid = false;
  uint8_t address = 0, accelAccuracy = 0, gyroAccuracy = 0, quatAccuracy = 0, magneticAccuracy = 0;
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0, mx = 0, my = 0, mz = 0;
  float qi = 0, qj = 0, qk = 0, qw = 1, roll = 0, pitch = 0, yaw = 0;
  uint64_t accelUs = 0, gyroUs = 0, quatUs = 0, magneticUs = 0, lastUs = 0;
  uint32_t reinits = 0;
  char fault[48] = "starting";
} bno;

struct LogStats {
  bool sdReady = false, logging = false;
  bool normalStop = true, faultSummaryWritten = false;
  uint32_t records = 0, writeErrors = 0, queueDrops = 0, controlFrames = 0, timingDiagnostics = 0, maxLogQueueWaitUs = 0;
  uint32_t sdWriteCalls = 0, sdLastRequest = 0, sdLastWritten = 0, sdLastWriteUs = 0, sdMaxWriteUs = 0;
  uint32_t localFrames = 0, controlCrc = 0, controlCobs = 0, controlLength = 0;
  uint16_t queueHighWater = 0;
  char runName[16] = "none";
  char directory[12] = "/BOATLOG";
  char fault[48] = "none";
  char sdOperation[16] = "none";
} logStats;

boat::Frame frameQueue[kFrameQueueDepth];
uint16_t queueHead = 0, queueTail = 0, queueUsed = 0;
portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;
File logFile;
uint8_t sdBuffer[kSdWriteBufferBytes];
size_t sdUsed = 0;
uint32_t commBootId = 0, localSequence = 0, controlTxSequence = 0;
uint32_t navSequence = 0, gnssFixSequence = 0, commandSequence = 0, timeSyncSequence = 0;
uint64_t lastControlFrameUs = 0, lastSdWriteStartUs = 0, lastSdWriteEndUs = 0;
uint32_t lastHeartbeatMs = 0, lastBnoRetryMs = 0, lastGnssStatusMs = 0, lastDiagMs = 0, lastNavMs = 0, lastTimeSyncMs = 0;
bool pendingNewFix = false, p1CaptureActive = false;
uint32_t p1CaptureId = 0;
struct CalibrationSession { bool active=false; uint32_t sessionId=0; boat::CalibrationKind kind=boat::CalibrationKind::Static6Face; char runName[16]="none"; } calibration;
esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
bool p1RecoveryDetected = false;
char p1RecoveredRun[16] = "none";
constexpr char kP1JournalPath[] = "/P1/ACTIVE.TXT";
struct LinkLatest {
  uint32_t navTx = 0, resultRx = 0, resultBadCrc = 0, commandAckRx = 0, lastCommandId = 0;
  uint64_t lastHeartbeatUs = 0, lastResultUs = 0, lastAckUs = 0, lastRttUs = 0, estimatedStateUs = 0;
  boat::GnssProcessResultPayload result{};
  boat::CommandAckPayload ack{};
  boat::EstimatedStatePayload estimatedState{};
} linkLatest;
struct PrimaryImuLatest { boat::PrimaryImuSnapshotPayload sample{}; uint64_t receivedUs=0; } primaryImu;
struct TofLatest { uint32_t frame=0; uint16_t centerMm=0; uint8_t centerStatus=0; uint64_t receivedUs=0; } tofLatest;
enum ProvisionalFlag : uint8_t { ProvisionalAttitudeBaseline=1u<<0, ProvisionalDualImuAccepted=1u<<1, ProvisionalGnssAccepted=1u<<2, ProvisionalTofAccepted=1u<<3, ProvisionalOutputBlocked=1u<<4, ProvisionalCalibrationPending=1u<<5 };
struct ProvisionalSystem {
  uint64_t lastServiceUs=0, lastLogUs=0;
  bool attitudeAvailable=false, dualAccepted=false, gnssAccepted=false, tofAccepted=false;
  float rollRad=0, pitchRad=0, yawRad=0, rollRateRadS=0, pitchRateRadS=0, yawRateRadS=0;
  float attitudeCorrection[3]{};
  float accelDeltaMps2=NAN, gyroDeltaRadS=NAN, waterHeightM=0;
  double latitudeDeg=0, longitudeDeg=0;
  float groundSpeedMps=0, courseRad=0;
  uint8_t flags=ProvisionalOutputBlocked|ProvisionalCalibrationPending;
} provisional;
volatile uint8_t pendingCommand = 0;
SemaphoreHandle_t controlTxMutex = nullptr;
SemaphoreHandle_t logMutex = nullptr;
TaskHandle_t bnoTaskHandle = nullptr;
TaskHandle_t logTaskHandle = nullptr;
struct BnoQueuedEvent {
  sh2_SensorValue_t value{};
  uint64_t receivedUs = 0, queuePushUs = 0;
};
struct BnoMetrics {
  uint32_t intEdges = 0, taskFallbacks = 0, serviceCalls = 0, callbackEvents = 0;
  uint32_t decodeErrors = 0, eventQueueDrops = 0, accelEvents = 0, gyroEvents = 0;
  uint32_t magneticEvents = 0, ignoredEvents = 0, maxServiceUs = 0;
  uint16_t eventQueueHighWater = 0;
} bnoMetrics;
QueueHandle_t bnoEventQueue = nullptr;
portMUX_TYPE bnoTaskMux = portMUX_INITIALIZER_UNLOCKED;
enum PendingCommand : uint8_t { CommandNone, CommandStartLog, CommandStopLog, CommandStop, CommandEstop, CommandStartP1, CommandStopP1, CommandStartCalibration, CommandStopCalibration };
boat::CalibrationKind pendingCalibrationKind=boat::CalibrationKind::Static6Face;
enum class BenchState : uint8_t { Idle, Preflight, PreparePhase, WaitControlReady, Warmup, Measuring, FinalizingPhase, NextPhase, FinalizingCampaign, Completed, Aborted, Estop };
enum class BenchPreset : uint8_t { Quick, Standard, Endurance, Custom };
struct BenchPhasePlan { boat::BenchmarkPhase phase; uint32_t quickMs; uint8_t inaProfile,tofProfile,uartProfile; uint32_t i2cHz; };
constexpr BenchPhasePlan kBenchPhases[] = {
  {static_cast<boat::BenchmarkPhase>(experiment_config::kPhase), experiment_config::kDurationMs, experiment_config::kInaProfile, experiment_config::kTofProfile, experiment_config::kUartProfile, experiment_config::kPeripheralI2cHz},
};
struct BenchmarkCampaign {
  BenchState state=BenchState::Idle; BenchPreset preset=BenchPreset::Quick; uint32_t id=0,startMs=0,stateMs=0,phaseStartMs=0,nextSyntheticUs=0,syntheticSeq=0;
  uint16_t phaseIndex=0, failedPhases=0; uint32_t customPhaseMs=0,stopCommandId=0; bool controlReady=false,resultReady=false,userStop=false; boat::BenchmarkReadyPayload ready{}; boat::BenchmarkResultPayload result{};
  uint32_t runNavStart=0,runResultStart=0,runHeartbeatStart=0,runBytesStart=0; char cableProfile[24]="CABLE_10CM",wiringType[20]="direct",pullup[16]="unknown",target[16]="unknown",note[48]=""; uint16_t cableLengthCm=10;
} benchmark;

uint64_t nowUs() { return static_cast<uint64_t>(esp_timer_get_time()); }
uint32_t ageMs(uint64_t then, uint64_t now) {
  return then ? static_cast<uint32_t>((now - then) / 1000ULL) : UINT32_MAX;
}

bool loggingActive() {
  portENTER_CRITICAL(&queueMux);
  const bool active = logStats.logging;
  portEXIT_CRITICAL(&queueMux);
  return active;
}

const char* calibrationKindName(boat::CalibrationKind kind) {
  switch(kind) {
    case boat::CalibrationKind::Static6Face: return "STATIC_6FACE";
    case boat::CalibrationKind::RotationX: return "ROTATION_X";
    case boat::CalibrationKind::RotationY: return "ROTATION_Y";
    case boat::CalibrationKind::RotationZ: return "ROTATION_Z";
    case boat::CalibrationKind::GyroBias: return "GYRO_BIAS";
    case boat::CalibrationKind::Magnetic: return "MAG";
    case boat::CalibrationKind::TimeOffset: return "TIME_OFFSET";
    case boat::CalibrationKind::Tof: return "TOF";
    case boat::CalibrationKind::ServoGeometry: return "SERVO_GEOMETRY";
    case boat::CalibrationKind::VescTelemetry: return "VESC_TELEMETRY";
  }
  return "UNKNOWN";
}

bool writeP1Journal() {
  if (!logStats.sdReady) return false;
  SD.mkdir("/P1");
  File journal = SD.open(kP1JournalPath, FILE_WRITE);
  if (!journal) return false;
  journal.printf("capture_id=%lu\nrun=%s\nstarted_us=%llu\n", (unsigned long)p1CaptureId,
                 logStats.runName, (unsigned long long)nowUs());
  journal.flush();
  journal.close();
  return true;
}

void clearP1Journal() {
  if (logStats.sdReady && SD.exists(kP1JournalPath)) SD.remove(kP1JournalPath);
}

void recoverP1Journal() {
  if (!logStats.sdReady || !SD.exists(kP1JournalPath)) return;
  File journal = SD.open(kP1JournalPath, FILE_READ);
  if (journal) {
    char line[64]{};
    while (journal.available()) {
      const size_t count = journal.readBytesUntil('\n', line, sizeof(line) - 1);
      line[count] = '\0';
      if (!strncmp(line, "run=", 4)) snprintf(p1RecoveredRun, sizeof(p1RecoveredRun), "%s", line + 4);
    }
    journal.close();
  }
  SD.mkdir("/P1");
  File recovery = SD.open("/P1/RECOVERY.TXT", FILE_APPEND);
  if (recovery) {
    recovery.printf("event=unexpected_p1_end\nrun=%s\nreset_reason=%d\nboot_id=%lu\n", p1RecoveredRun,
                    static_cast<int>(bootResetReason), (unsigned long)commBootId);
    recovery.close();
  }
  p1RecoveryDetected = true;
  clearP1Journal();
}

void setLoggingActive(bool active) {
  portENTER_CRITICAL(&queueMux);
  logStats.logging = active;
  portEXIT_CRITICAL(&queueMux);
}

bool enqueueFrame(boat::Frame frame) { frame.logQueueUs = nowUs();
  portENTER_CRITICAL(&queueMux);
  if (!logStats.logging) {
    portEXIT_CRITICAL(&queueMux);
    return false;
  }
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
  if (logTaskHandle) xTaskNotifyGive(logTaskHandle);
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

void clearLogQueue() { portENTER_CRITICAL(&queueMux); queueHead=queueTail=queueUsed=0; portEXIT_CRITICAL(&queueMux); }
void writeAbortSummary(const char* reason) { if(!logStats.sdReady||!strcmp(logStats.runName,"none"))return;char path[32]{};snprintf(path,sizeof(path),"%s/%s",logStats.directory,logStats.runName);char* e=strstr(path,".BIN");if(!e)return;memcpy(e,".TXT",5);File f=SD.open(path,FILE_WRITE);if(!f){Serial.printf("FAULT SUMMARY OPEN FAILED: %s\n",reason);return;}f.printf("firmware=%s\nversion=%s\nprotocol=%u\nnormal_stop=0\nabort_reason=%s\nrecords_before_abort=%lu\nqueue_drops=%lu\nsd_write_errors=%lu\nlog_fault=%s\nsd_operation=%s\nsd_write_chunk_bytes=%u\nsd_write_calls=%lu\nsd_last_request_bytes=%lu\nsd_last_written_bytes=%lu\nsd_last_write_us=%lu\nsd_max_write_us=%lu\nsd_card_type=%u\nsd_card_bytes=%llu\nsd_total_bytes=%llu\nsd_used_bytes=%llu\ncampaign_id=%lu\nphase=%u\ndirectory=%s\n",kFirmwareName,kFirmwareVersion,boat::kVersion,reason,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,logStats.fault,logStats.sdOperation,(unsigned)kSdWriteChunkBytes,(unsigned long)logStats.sdWriteCalls,(unsigned long)logStats.sdLastRequest,(unsigned long)logStats.sdLastWritten,(unsigned long)logStats.sdLastWriteUs,(unsigned long)logStats.sdMaxWriteUs,(unsigned)SD.cardType(),(unsigned long long)SD.cardSize(),(unsigned long long)SD.totalBytes(),(unsigned long long)SD.usedBytes(),(unsigned long)benchmark.id,benchmark.phaseIndex,logStats.directory);f.close();logStats.faultSummaryWritten=true;}
void abortLog(const char* reason) { snprintf(logStats.fault,sizeof(logStats.fault),"%s",reason); logStats.normalStop=false; setLoggingActive(false); sdUsed=0; if(logFile) logFile.close(); writeAbortSummary(reason); clearLogQueue(); Serial.printf("LOG aborted: %s summary=%d\n",reason,logStats.faultSummaryWritten); }

void emitLocal(boat::Type type, const void* payload, uint16_t length, uint16_t flags = 0) {
  if (!logStats.logging || length > boat::kMaxPayload) return;
  boat::Frame frame{};
  frame.header = {boat::kVersion, static_cast<uint8_t>(type), length, ++localSequence,
                  commBootId, nowUs(), flags};
  if (length) memcpy(frame.payload, payload, length);
  if (enqueueFrame(frame)) ++logStats.localFrames;
}

bool isBnoFrame(const boat::Frame& frame) {
  const uint8_t type = frame.header.type;
  return frame.header.length == sizeof(boat::BnoPayload) &&
         (type == static_cast<uint8_t>(boat::Type::BnoAccel) || type == static_cast<uint8_t>(boat::Type::BnoGyro) || type == static_cast<uint8_t>(boat::Type::BnoMagnetic));
}

void emitTimingDiagnostic(const boat::Frame& frame, uint32_t queueWaitUs) {
  if (!isBnoFrame(frame) || queueWaitUs < kTimingDiagnosticThresholdUs) return;
  boat::BnoPayload source{}; memcpy(&source, frame.payload, sizeof(source));
  boat::TimingDiagnosticPayload diagnostic{};
  diagnostic.originBootId = frame.header.bootId; diagnostic.originSequence = frame.header.sequence;
  diagnostic.sourceType = frame.header.type; diagnostic.bnoSequence = source.sequence;
  diagnostic.sensorTimestamp = source.sensorUs; diagnostic.callbackUs = source.callbackUs; diagnostic.queuePushUs = source.queuePushUs;
  diagnostic.frameUs = frame.header.sourceUs; diagnostic.uartRxUs = frame.uartRxUs; diagnostic.logQueueUs = frame.logQueueUs; diagnostic.sdTaskUs = frame.sdTaskUs;
  diagnostic.lastSdWriteStartUs = lastSdWriteStartUs; diagnostic.lastSdWriteEndUs = lastSdWriteEndUs; diagnostic.queueWaitUs = queueWaitUs;
  emitLocal(boat::Type::TimingDiagnostic, &diagnostic, sizeof(diagnostic), 1); ++logStats.timingDiagnostics;
}

bool commitSdBuffer(const char* operation) {
  if (!logFile || !sdUsed) return true;
  const size_t total = sdUsed;
  size_t offset = 0;
  while (offset < total) {
    const size_t requested = min(kSdWriteChunkBytes, total - offset);
    const uint64_t startedUs = nowUs();
    const size_t written = logFile.write(sdBuffer + offset, requested);
    const uint64_t finishedUs = nowUs();
    const uint32_t elapsedUs = (uint32_t)(finishedUs - startedUs);
    lastSdWriteStartUs = startedUs; lastSdWriteEndUs = finishedUs;
    ++logStats.sdWriteCalls;
    logStats.sdLastRequest = requested;
    logStats.sdLastWritten = written;
    logStats.sdLastWriteUs = elapsedUs;
    logStats.sdMaxWriteUs = max(logStats.sdMaxWriteUs, elapsedUs);
    if (written != requested) {
      ++logStats.writeErrors;
      snprintf(logStats.sdOperation, sizeof(logStats.sdOperation), "%s", operation);
      sdUsed = 0;  // Do not repeatedly retry a buffer after an SD write failure.
      return false;
    }
    offset += written;
  }
  sdUsed = 0;
  return true;
}

bool appendBytes(const void* data, size_t length) {
  const uint8_t* source = static_cast<const uint8_t*>(data);
  while (length) {
    const size_t space = sizeof(sdBuffer) - sdUsed;
    const size_t part = length < space ? length : space;
    memcpy(sdBuffer + sdUsed, source, part);
    sdUsed += part; source += part; length -= part;
    if (sdUsed == sizeof(sdBuffer) && !commitSdBuffer("buffer_full")) return false;
  }
  return true;
}

bool finalizeLog() {
  if (!logFile) return true;
  if (!commitSdBuffer("final_flush")) return false;
  logFile.flush();
  return true;
}

bool startLog(const char* directory = kLogDirectory) {
  if (!logMutex || xSemaphoreTake(logMutex, portMAX_DELAY) != pdTRUE) return false;
  if (!logStats.sdReady || loggingActive()) { const bool active=loggingActive(); xSemaphoreGive(logMutex); return active; }
  portENTER_CRITICAL(&bnoTaskMux); bnoMetrics = {}; portEXIT_CRITICAL(&bnoTaskMux);
  logStats.records=0; logStats.writeErrors=0; logStats.queueDrops=0; logStats.localFrames=0; logStats.queueHighWater=0; logStats.timingDiagnostics=0; logStats.maxLogQueueWaitUs=0; logStats.sdWriteCalls=0; logStats.sdLastRequest=0; logStats.sdLastWritten=0; logStats.sdLastWriteUs=0; logStats.sdMaxWriteUs=0; logStats.normalStop=true; logStats.faultSummaryWritten=false; snprintf(logStats.fault,sizeof(logStats.fault),"none"); snprintf(logStats.sdOperation,sizeof(logStats.sdOperation),"none"); snprintf(logStats.directory,sizeof(logStats.directory),"%s",directory); clearLogQueue();
  SD.mkdir(logStats.directory);
  char path[32];
  for (uint16_t index = 1; index < 10000; ++index) {
    snprintf(logStats.runName, sizeof(logStats.runName), "RUN%04u.BIN", index);
    snprintf(path, sizeof(path), "%s/%s", logStats.directory, logStats.runName);
    if (!SD.exists(path)) { logFile = SD.open(path, FILE_WRITE); break; }
  }
  if (!logFile) { ++logStats.writeErrors; snprintf(logStats.runName, sizeof(logStats.runName), "open-error"); xSemaphoreGive(logMutex); return false; }
  setLoggingActive(true);
  xSemaphoreGive(logMutex);
  Serial.printf("LOG started: %s/%s\n", logStats.directory, logStats.runName);
  return true;
}

void writeRunSummary() {
  if (!logStats.sdReady || !strcmp(logStats.runName, "none")) return;
  char path[32]{}; snprintf(path,sizeof(path),"%s/%s",logStats.directory,logStats.runName);
  char* extension=strstr(path,".BIN"); if (!extension) return; memcpy(extension,".TXT",5);
  File summary=SD.open(path,FILE_WRITE); if (!summary) { ++logStats.writeErrors; return; }
  BnoMetrics metrics{}; portENTER_CRITICAL(&bnoTaskMux); metrics=bnoMetrics; portEXIT_CRITICAL(&bnoTaskMux);
  summary.printf("firmware=%s\nversion=%s\nprotocol=%u\nnormal_stop=%u\nrecords=%lu\nqueue_drops=%lu\nqueue_high_water=%u\nsd_write_errors=%lu\nlog_fault=%s\nsd_write_chunk_bytes=%u\nsd_write_calls=%lu\nsd_max_write_us=%lu\ngnss_nav_tx=%lu\ngnss_result_rx=%lu\nresult_bad_crc=%lu\nlast_rtt_us=%llu\ncommand_ack_rx=%lu\nbno_int_edges=%lu\nbno_task_fallbacks=%lu\nbno_service_calls=%lu\nbno_callback_events=%lu\nbno_decode_errors=%lu\nbno_event_queue_drops=%lu\nbno_event_queue_high_water=%u\nbno_accel_events=%lu\nbno_gyro_events=%lu\nbno_magnetic_events=%lu\nbno_ignored_events=%lu\nbno_max_service_us=%lu\n",kFirmwareName,kFirmwareVersion,boat::kVersion,logStats.normalStop?1:0,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned)logStats.queueHighWater,(unsigned long)logStats.writeErrors,logStats.fault,(unsigned)kSdWriteChunkBytes,(unsigned long)logStats.sdWriteCalls,(unsigned long)logStats.sdMaxWriteUs,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)linkLatest.resultBadCrc,(unsigned long long)linkLatest.lastRttUs,(unsigned long)linkLatest.commandAckRx,(unsigned long)metrics.intEdges,(unsigned long)metrics.taskFallbacks,(unsigned long)metrics.serviceCalls,(unsigned long)metrics.callbackEvents,(unsigned long)metrics.decodeErrors,(unsigned long)metrics.eventQueueDrops,(unsigned)metrics.eventQueueHighWater,(unsigned long)metrics.accelEvents,(unsigned long)metrics.gyroEvents,(unsigned long)metrics.magneticEvents,(unsigned long)metrics.ignoredEvents,(unsigned long)metrics.maxServiceUs);
  summary.printf("timing_diagnostics=%lu\nmax_log_queue_wait_us=%lu\n",(unsigned long)logStats.timingDiagnostics,(unsigned long)logStats.maxLogQueueWaitUs); summary.close();
}
void stopLog() {
  if (!logMutex || xSemaphoreTake(logMutex, portMAX_DELAY) != pdTRUE) return;
  if (!loggingActive()) { xSemaphoreGive(logMutex); return; }
  // Closing admission before draining guarantees that no producer can append a
  // frame after the final drain begins.
  setLoggingActive(false);
  boat::Frame frame{};
  while (dequeueFrame(frame)) {
    const uint64_t receivedUs = frame.logQueueUs ? frame.logQueueUs : nowUs();
    if (!appendBytes(&kLogMagic, sizeof(kLogMagic)) || !appendBytes(&receivedUs, sizeof(receivedUs)) ||
        !appendBytes(&frame.header, sizeof(frame.header)) || !appendBytes(frame.payload, frame.header.length)) { abortLog("SD write failed while stopping"); xSemaphoreGive(logMutex); return; }
    ++logStats.records;
  }
  if (!finalizeLog()) { abortLog("SD flush failed while stopping"); xSemaphoreGive(logMutex); return; }
  logFile.close(); writeRunSummary();
  xSemaphoreGive(logMutex);
  Serial.printf("LOG stopped: records=%lu errors=%lu\n", (unsigned long)logStats.records, (unsigned long)logStats.writeErrors);
}

void serviceLog() {
  if (!logMutex || xSemaphoreTake(logMutex, 0) != pdTRUE) return;
  if (!loggingActive()) { xSemaphoreGive(logMutex); return; }
  boat::Frame frame{};
  for (uint8_t i = 0; i < 32 && dequeueFrame(frame); ++i) {
    // The outer record timestamp is the bounded acquisition-to-recorder
    // ingress time, not the later SD writer service time.  The latter remains
    // in sdTaskUs and TimingDiagnostic so storage latency is never hidden.
    const uint64_t receivedUs = frame.logQueueUs ? frame.logQueueUs : nowUs();
    frame.sdTaskUs = nowUs();
    const uint32_t queueWaitUs = frame.logQueueUs ? static_cast<uint32_t>(frame.sdTaskUs - frame.logQueueUs) : 0;
    if (queueWaitUs > logStats.maxLogQueueWaitUs) logStats.maxLogQueueWaitUs = queueWaitUs;
    if (!appendBytes(&kLogMagic, sizeof(kLogMagic)) || !appendBytes(&receivedUs, sizeof(receivedUs)) ||
        !appendBytes(&frame.header, sizeof(frame.header)) || !appendBytes(frame.payload, frame.header.length)) {
      abortLog("SD write failed"); xSemaphoreGive(logMutex); return;
    }
    ++logStats.records; emitTimingDiagnostic(frame, queueWaitUs);
  }
  xSemaphoreGive(logMutex);
}

void logTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kLogTaskWakeMs));
    serviceLog();
    // Leave a scheduling point for the equally-prioritized GNSS task.
    vTaskDelay(1);
  }
}

void controlRxTask(void*) {
  for (;;) {
    while (controlUart.available()) {
      boat::Frame frame{};
      if (controlDecoder.feed(static_cast<uint8_t>(controlUart.read()), frame)) {
        ++logStats.controlFrames;
        frame.uartRxUs = nowUs(); lastControlFrameUs = frame.uartRxUs;
        const boat::Type type = static_cast<boat::Type>(frame.header.type);
        if (type == boat::Type::Heartbeat) linkLatest.lastHeartbeatUs = lastControlFrameUs;
        if (type == boat::Type::GnssProcessResult && frame.header.length == sizeof(boat::GnssProcessResultPayload)) {
          boat::GnssProcessResultPayload result{}; memcpy(&result, frame.payload, sizeof(result));
          if (result.canonicalCrc == boat::canonicalCrc(&result, offsetof(boat::GnssProcessResultPayload, canonicalCrc))) { linkLatest.result=result; ++linkLatest.resultRx; linkLatest.lastResultUs=lastControlFrameUs; }
          else ++linkLatest.resultBadCrc;
        }
        if (type == boat::Type::CommandAck && frame.header.length == sizeof(boat::CommandAckPayload)) { memcpy(&linkLatest.ack,frame.payload,sizeof(linkLatest.ack)); linkLatest.lastCommandId=linkLatest.ack.commandId; ++linkLatest.commandAckRx; linkLatest.lastAckUs=lastControlFrameUs; }
        if (type == boat::Type::EstimatedState && frame.header.length == sizeof(boat::EstimatedStatePayload)) { memcpy(&linkLatest.estimatedState,frame.payload,sizeof(linkLatest.estimatedState)); linkLatest.estimatedStateUs=lastControlFrameUs; }
        if (type == boat::Type::PrimaryImuSnapshot && frame.header.length == sizeof(boat::PrimaryImuSnapshotPayload)) { memcpy(&primaryImu.sample,frame.payload,sizeof(primaryImu.sample)); primaryImu.receivedUs=lastControlFrameUs; }
        if (type == boat::Type::TofFrame && frame.header.length == 196) {
          memcpy(&tofLatest.frame, frame.payload, sizeof(tofLatest.frame));
          constexpr uint8_t kCenterZone = 32;  // VL53L5CX 8x8 centre cell in the transmitted 64-zone frame.
          memcpy(&tofLatest.centerMm, frame.payload + 4 + kCenterZone * sizeof(uint16_t), sizeof(tofLatest.centerMm));
          tofLatest.centerStatus = frame.payload[132 + kCenterZone];
          tofLatest.receivedUs = lastControlFrameUs;
        }
        if (type == boat::Type::BenchmarkReady && frame.header.length == sizeof(boat::BenchmarkReadyPayload)) { memcpy(&benchmark.ready,frame.payload,sizeof(benchmark.ready)); if (benchmark.ready.canonicalCrc==boat::canonicalCrc(&benchmark.ready,offsetof(boat::BenchmarkReadyPayload,canonicalCrc)) && benchmark.ready.campaignId==benchmark.id) benchmark.controlReady=true; }
        if (type == boat::Type::BenchmarkResult && frame.header.length == sizeof(boat::BenchmarkResultPayload)) { memcpy(&benchmark.result,frame.payload,sizeof(benchmark.result)); if (benchmark.result.canonicalCrc==boat::canonicalCrc(&benchmark.result,offsetof(boat::BenchmarkResultPayload,canonicalCrc)) && benchmark.result.campaignId==benchmark.id) benchmark.resultReady=true; }
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
         bno08x.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, kMagneticIntervalUs);
}

void bnoSensorCallback(void*, sh2_SensorEvent_t* event) {
  BnoQueuedEvent queued{};
  if (sh2_decodeSensorEvent(&queued.value, event) != SH2_OK) {
    portENTER_CRITICAL(&bnoTaskMux); ++bnoMetrics.decodeErrors; portEXIT_CRITICAL(&bnoTaskMux);
    return;
  }
  queued.receivedUs = nowUs();
  portENTER_CRITICAL(&bnoTaskMux);
  ++bnoMetrics.callbackEvents;
  if (queued.value.sensorId == SH2_ACCELEROMETER) ++bnoMetrics.accelEvents;
  else if (queued.value.sensorId == SH2_GYROSCOPE_CALIBRATED) ++bnoMetrics.gyroEvents;
  else if (queued.value.sensorId == SH2_MAGNETIC_FIELD_CALIBRATED) ++bnoMetrics.magneticEvents;
  else ++bnoMetrics.ignoredEvents;
  portEXIT_CRITICAL(&bnoTaskMux);
  queued.queuePushUs = nowUs();
  if (!bnoEventQueue || xQueueSend(bnoEventQueue, &queued, 0) != pdPASS) {
    portENTER_CRITICAL(&bnoTaskMux); ++bnoMetrics.eventQueueDrops; portEXIT_CRITICAL(&bnoTaskMux);
    return;
  }
  const UBaseType_t used = uxQueueMessagesWaiting(bnoEventQueue);
  portENTER_CRITICAL(&bnoTaskMux);
  if (used > bnoMetrics.eventQueueHighWater) bnoMetrics.eventQueueHighWater = used;
  portEXIT_CRITICAL(&bnoTaskMux);
}

bool startBno() {
  bno.ready = false;
  if (!bnoEventQueue) { setBnoFault("BNO event queue unavailable"); return false; }
  xQueueReset(bnoEventQueue);
  bnoWire.begin(kBnoSdaPin, kBnoSclPin, kBnoI2cHz);
  for (uint8_t address : {kBnoAddressPrimary, kBnoAddressAlternate}) {
    if (bno08x.begin_I2C(address, &bnoWire) &&
        sh2_setSensorCallback(bnoSensorCallback, nullptr) == SH2_OK && bnoReports()) {
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

void emitBnoEvent(const BnoQueuedEvent& queued) {
  const sh2_SensorValue_t& value = queued.value;
  boat::BnoPayload payload{};
  payload.accuracy = value.status & 3; payload.sequence = value.sequence; payload.sensorUs = value.timestamp; payload.callbackUs = queued.receivedUs; payload.queuePushUs = queued.queuePushUs;
  if (value.sensorId == SH2_ACCELEROMETER) {
    payload.kind=1; payload.v[0]=value.un.accelerometer.x; payload.v[1]=value.un.accelerometer.y; payload.v[2]=value.un.accelerometer.z;
    bno.ax=payload.v[0]; bno.ay=payload.v[1]; bno.az=payload.v[2]; bno.accelAccuracy=payload.accuracy; bno.accelUs=queued.receivedUs; bno.accelValid=true;
    emitLocal(boat::Type::BnoAccel, &payload, sizeof(payload));
  } else if (value.sensorId == SH2_GYROSCOPE_CALIBRATED) {
    payload.kind=2; payload.v[0]=value.un.gyroscope.x; payload.v[1]=value.un.gyroscope.y; payload.v[2]=value.un.gyroscope.z;
    bno.gx=payload.v[0]; bno.gy=payload.v[1]; bno.gz=payload.v[2]; bno.gyroAccuracy=payload.accuracy; bno.gyroUs=queued.receivedUs; bno.gyroValid=true;
    emitLocal(boat::Type::BnoGyro, &payload, sizeof(payload));
  } else if (value.sensorId == SH2_MAGNETIC_FIELD_CALIBRATED) {
    payload.kind=4; payload.v[0]=value.un.magneticField.x; payload.v[1]=value.un.magneticField.y; payload.v[2]=value.un.magneticField.z;
    bno.mx=payload.v[0]; bno.my=payload.v[1]; bno.mz=payload.v[2]; bno.magneticAccuracy=payload.accuracy; bno.magneticUs=queued.receivedUs; bno.magneticValid=true;
    emitLocal(boat::Type::BnoMagnetic, &payload, sizeof(payload));
  } else return;
  bno.lastUs = queued.receivedUs;
}

void drainBnoEvents() {
  BnoQueuedEvent queued{};
  while (xQueueReceive(bnoEventQueue, &queued, 0) == pdTRUE) emitBnoEvent(queued);
}

void serviceBno(bool processEvents) {
  if (!bno.ready) {
    if (millis() - lastBnoRetryMs >= kBnoReinitIntervalMs) { lastBnoRetryMs = millis(); ++bno.reinits; startBno(); }
    return;
  }
  if (bno08x.wasReset()) {
    xQueueReset(bnoEventQueue);
    if (sh2_setSensorCallback(bnoSensorCallback, nullptr) != SH2_OK || !bnoReports()) { bno.ready = false; setBnoFault("BNO08X reset"); return; }
  }
  if (processEvents) {
    const uint64_t startedUs = nowUs();
    uint8_t calls = 0;
    do { sh2_service(); ++calls; } while (digitalRead(kBnoIntPin) == LOW && calls < kBnoServiceCallBudget);
    const uint32_t elapsedUs = static_cast<uint32_t>(nowUs() - startedUs);
    portENTER_CRITICAL(&bnoTaskMux);
    bnoMetrics.serviceCalls += calls;
    if (elapsedUs > bnoMetrics.maxServiceUs) bnoMetrics.maxServiceUs = elapsedUs;
    portEXIT_CRITICAL(&bnoTaskMux);
  }
  drainBnoEvents();
  if (ageMs(bno.lastUs, nowUs()) > kBnoNoDataTimeoutMs) { bno.ready=false; setBnoFault("BNO08X no data"); }
}

void IRAM_ATTR bnoIntIsr() {
  BaseType_t woke = pdFALSE;
  portENTER_CRITICAL_ISR(&bnoTaskMux); ++bnoMetrics.intEdges; portEXIT_CRITICAL_ISR(&bnoTaskMux);
  if (bnoTaskHandle) vTaskNotifyGiveFromISR(bnoTaskHandle, &woke);
  if (woke) portYIELD_FROM_ISR();
}

void bnoTask(void*) {
  for (;;) {
    const bool intLow = digitalRead(kBnoIntPin) == LOW;
    const uint32_t notified = intLow ? 0 : ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kBnoTaskFallbackMs));
    const bool processEvents = intLow || notified;
    if (!processEvents) { portENTER_CRITICAL(&bnoTaskMux); ++bnoMetrics.taskFallbacks; portEXIT_CRITICAL(&bnoTaskMux); }
    serviceBno(processEvents);
  }
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

bool sendControl(boat::Type type, const void* payload = nullptr, uint16_t payloadLength = 0) {
  // This is a separate direction of the link. Keep SD-log sequence numbers
  // contiguous so an offline parser can use gaps as loss evidence.
  boat::Header header{boat::kVersion,static_cast<uint8_t>(type),payloadLength,++controlTxSequence,commBootId,nowUs(),0};
  uint8_t encoded[boat::kMaxEncoded]; const size_t length=boat::encode(header,static_cast<const uint8_t*>(payload),encoded,sizeof(encoded));
  if (!length) return false;
  // A GNSS_NAV sequence must never be skipped merely because heartbeat or a
  // command is using the same UART. The wire is fast enough that this wait is bounded in practice.
  if (controlTxMutex && xSemaphoreTake(controlTxMutex,portMAX_DELAY) != pdTRUE) return false;
  const size_t written=controlUart.write(encoded,length);
  if (controlTxMutex) xSemaphoreGive(controlTxMutex);
  return written == length;
}

bool sendP1Capture(boat::P1CaptureAction action) {
  boat::P1CapturePayload capture{};
  capture.captureId = p1CaptureId;
  capture.action = static_cast<uint8_t>(action);
  if (!sendControl(boat::Type::P1Capture, &capture, sizeof(capture))) return false;
  emitLocal(boat::Type::P1Capture, &capture, sizeof(capture), 1);
  return true;
}

void emitCalibrationMarker(boat::CalibrationAction action) {
  boat::CalibrationMarkerPayload marker{};
  marker.sessionId=calibration.sessionId;
  marker.kind=static_cast<uint8_t>(calibration.kind);
  marker.action=static_cast<uint8_t>(action);
  emitLocal(boat::Type::CalibrationMarker,&marker,sizeof(marker),1);
}

bool startCalibration(boat::CalibrationKind kind) {
  if (loggingActive() || !logStats.sdReady) return false;
  calibration={}; calibration.kind=kind; calibration.sessionId=esp_random(); if(!calibration.sessionId) calibration.sessionId=1;
  if (!startLog("/CAL")) return false;
  snprintf(calibration.runName,sizeof(calibration.runName),"%s",logStats.runName);
  p1CaptureId=calibration.sessionId;
  calibration.active=sendP1Capture(boat::P1CaptureAction::Start);
  if (!calibration.active) { stopLog(); return false; }
  emitCalibrationMarker(boat::CalibrationAction::Start);
  return true;
}

void stopCalibration() {
  if (!calibration.active) return;
  emitCalibrationMarker(boat::CalibrationAction::Stop);
  sendP1Capture(boat::P1CaptureAction::Stop);
  calibration.active=false;
  if (loggingActive()) stopLog();
}

const BenchPhasePlan& currentBenchPhase() { return kBenchPhases[benchmark.phaseIndex]; }
uint32_t benchPhaseDurationMs(const BenchPhasePlan& p) { return p.quickMs; }
uint32_t benchWarmupMs(const BenchPhasePlan& p) {
  if (p.i2cHz==100000) return kBenchWarmupI2cMs;
  if (p.tofProfile) return kBenchWarmupTofMs;
  if (p.inaProfile) return kBenchWarmupInaMs;
  if (p.uartProfile) return kBenchWarmupUartMs;
  return kBenchWarmupUartMs;
}
const char* benchStateName(BenchState s) { static const char* n[]={"IDLE","PREFLIGHT","PREPARE_PHASE","WAIT_CONTROL_READY","WARMUP","MEASURING","FINALIZING_PHASE","NEXT_PHASE","FINALIZING_CAMPAIGN","COMPLETED","ABORTED","E_STOP"}; return n[(uint8_t)s]; }
const char* benchPresetName(BenchPreset p) { return p==BenchPreset::Quick?"QUICK":p==BenchPreset::Standard?"STANDARD":p==BenchPreset::Endurance?"ENDURANCE":"CUSTOM"; }
void emitBenchEvent(uint8_t code, boat::BenchmarkStatus status, uint32_t value=0) { boat::BenchmarkEventPayload e{}; e.campaignId=benchmark.id; e.phaseId=benchmark.phaseIndex; e.code=code; e.status=(uint8_t)status; e.value=value; e.flags=boat::BenchmarkDryRun; e.timestampUs=nowUs(); e.canonicalCrc=boat::canonicalCrc(&e,offsetof(boat::BenchmarkEventPayload,canonicalCrc)); emitLocal(boat::Type::BenchmarkEvent,&e,sizeof(e),1); }
bool sendBenchCommand(boat::BenchmarkAction action) { const auto& p=currentBenchPhase(); boat::BenchmarkCommandPayload c{}; c.campaignId=benchmark.id;c.phaseId=benchmark.phaseIndex;c.phase=(uint8_t)p.phase;c.action=(uint8_t)action;c.durationMs=benchPhaseDurationMs(p);c.i2cClockHz=p.i2cHz;c.inaProfile=p.inaProfile;c.tofProfile=p.tofProfile;c.uartProfile=p.uartProfile;c.canonicalCrc=boat::canonicalCrc(&c,offsetof(boat::BenchmarkCommandPayload,canonicalCrc)); const boat::Type type=action==boat::BenchmarkAction::Prepare?boat::Type::BenchmarkPrepare:action==boat::BenchmarkAction::Start?boat::Type::BenchmarkStart:action==boat::BenchmarkAction::Stop?boat::Type::BenchmarkStop:boat::Type::BenchmarkAbort; if(!sendControl(type,&c,sizeof(c)))return false; emitLocal(type,&c,sizeof(c),1); return true; }
void appendBenchmarkSummary(const char* outcome) { if (!logStats.sdReady||!strcmp(logStats.runName,"none"))return;char path[32]{};snprintf(path,sizeof(path),"%s/%s",logStats.directory,logStats.runName);char* e=strstr(path,".BIN");if(!e)return;memcpy(e,".TXT",5);File f=SD.open(path,FILE_APPEND);if(!f)return;f.printf("experiment=%s\nbenchmark_outcome=%s\ncampaign_id=%lu\npreset=%s\ncable_profile=%s\ncable_length_cm=%u\nwiring_type=%s\npullup_resistance_ohm=%s\ntarget_distance_mm=%s\nnote=%s\nrun_gnss_nav_tx=%lu\nrun_gnss_result_rx=%lu\nphase_count=%u\nlast_phase=%u\nfailed_phases=%u\n",experiment_config::kName,outcome,(unsigned long)benchmark.id,benchPresetName(benchmark.preset),benchmark.cableProfile,benchmark.cableLengthCm,benchmark.wiringType,benchmark.pullup,benchmark.target,benchmark.note,(unsigned long)(linkLatest.navTx-benchmark.runNavStart),(unsigned long)(linkLatest.resultRx-benchmark.runResultStart),(unsigned)(sizeof(kBenchPhases)/sizeof(kBenchPhases[0])),benchmark.phaseIndex,benchmark.failedPhases);f.close(); }
void finishBenchmark(const char* outcome, bool emergency=false) { if (benchmark.state==BenchState::Idle||benchmark.state==BenchState::Completed||benchmark.state==BenchState::Aborted)return;emitBenchEvent(emergency?9:8,emergency?boat::BenchmarkStatus::Aborted:boat::BenchmarkStatus::Pass);if(strcmp(outcome,"completed")&&strcmp(outcome,"user_stop")){logStats.normalStop=false;snprintf(logStats.fault,sizeof(logStats.fault),"%s",outcome);}benchmark.state=emergency?BenchState::Estop:BenchState::FinalizingCampaign;if(logStats.logging){stopLog();appendBenchmarkSummary(outcome);} }
void serviceBenchSynthetic() { if (benchmark.state!=BenchState::Measuring)return;const uint8_t profile=currentBenchPhase().uartProfile;if(!profile)return;const uint32_t hz=profile==1?250:profile==2?500:630;const uint64_t period=1000000ULL/hz;const uint64_t n=nowUs();if(n<benchmark.nextSyntheticUs)return;benchmark.nextSyntheticUs=n+period;boat::SyntheticDataPayload p{};p.campaignId=benchmark.id;p.sequence=++benchmark.syntheticSeq;p.stream=p.sequence%8;p.bytes=sizeof(p);p.generatedUs=n;for(uint8_t i=0;i<sizeof(p.pattern);++i)p.pattern[i]=(uint8_t)((p.sequence+i*17u)&0xffu);p.payloadCrc=boat::crc32((const uint8_t*)&p,offsetof(boat::SyntheticDataPayload,payloadCrc));if(sendControl(boat::Type::SyntheticData,&p,sizeof(p)))emitLocal(boat::Type::SyntheticData,&p,sizeof(p),1); }
void serviceBenchmark() {
  if (benchmark.state==BenchState::Idle||benchmark.state==BenchState::Completed||benchmark.state==BenchState::Aborted||benchmark.state==BenchState::Estop)return;
  const uint32_t elapsed=millis()-benchmark.stateMs;
  if (logStats.writeErrors) { benchmark.state=BenchState::Aborted; return; }
  if (!logStats.logging) { if(benchmark.state==BenchState::FinalizingCampaign)benchmark.state=BenchState::Completed; else benchmark.state=BenchState::Aborted; return; }
  // PHASE_PREPARE deliberately reinitializes the shared I2C devices on the
  // control XIAO. It can block its normal heartbeat longer than 500 ms, so
  // enforce the fast link-loss rule only while an actual measurement is live.
  if (benchmark.state==BenchState::Measuring&&ageMs(linkLatest.lastHeartbeatUs,nowUs())>kBenchLinkAbortMs) { finishBenchmark("link_timeout",false); benchmark.state=BenchState::Aborted; return; }
  if (benchmark.state==BenchState::Preflight) { if (!logStats.sdReady||!bno.ready||!gnssRx.receiving(nowUs())) { finishBenchmark("preflight_failed",false); benchmark.state=BenchState::Aborted; return; } if(linkLatest.lastCommandId<benchmark.stopCommandId){if(elapsed>kBenchCommandTimeoutMs){finishBenchmark("stop_ack_timeout",false);benchmark.state=BenchState::Aborted;}return;} if(linkLatest.ack.safetyState!=1||linkLatest.ack.disposition!=0){finishBenchmark("stop_not_disarmed",false);benchmark.state=BenchState::Aborted;return;} benchmark.state=BenchState::PreparePhase;benchmark.stateMs=millis();return; }
  if (benchmark.state==BenchState::PreparePhase) { benchmark.controlReady=false; if(!sendBenchCommand(boat::BenchmarkAction::Prepare)){finishBenchmark("prepare_send_failed",false);benchmark.state=BenchState::Aborted;return;} emitBenchEvent(2,boat::BenchmarkStatus::Pass);benchmark.state=BenchState::WaitControlReady;benchmark.stateMs=millis();return; }
  if (benchmark.state==BenchState::WaitControlReady) { if(benchmark.controlReady){if(benchmark.ready.status!=(uint8_t)boat::BenchmarkStatus::Pass||!(benchmark.ready.flags&boat::BenchmarkDryRun)||!(benchmark.ready.flags&boat::BenchmarkPcaOff)||!(benchmark.ready.flags&boat::BenchmarkVescZero)){finishBenchmark("control_preflight_failed",false);benchmark.state=BenchState::Aborted;return;} benchmark.state=BenchState::Warmup;benchmark.stateMs=millis();emitBenchEvent(3,boat::BenchmarkStatus::Pass);return;} if(elapsed>kBenchPrepareTimeoutMs){finishBenchmark("prepare_timeout",false);benchmark.state=BenchState::Aborted;} return; }
  if (benchmark.state==BenchState::Warmup) { if(elapsed<benchWarmupMs(currentBenchPhase()))return; if(!sendBenchCommand(boat::BenchmarkAction::Start)){finishBenchmark("start_send_failed",false);benchmark.state=BenchState::Aborted;return;} benchmark.phaseStartMs=millis();benchmark.nextSyntheticUs=nowUs();benchmark.state=BenchState::Measuring;benchmark.stateMs=millis();emitBenchEvent(4,boat::BenchmarkStatus::Pass);return; }
  if (benchmark.state==BenchState::Measuring) { serviceBenchSynthetic();if(millis()-benchmark.phaseStartMs<benchPhaseDurationMs(currentBenchPhase()))return;benchmark.resultReady=false;if(!sendBenchCommand(boat::BenchmarkAction::Stop)){finishBenchmark("stop_send_failed",false);benchmark.state=BenchState::Aborted;return;}benchmark.state=BenchState::FinalizingPhase;benchmark.stateMs=millis();return; }
  if (benchmark.state==BenchState::FinalizingPhase) { if(benchmark.resultReady){if(benchmark.result.status!=(uint8_t)boat::BenchmarkStatus::Pass)++benchmark.failedPhases;emitBenchEvent(5,(boat::BenchmarkStatus)benchmark.result.status,benchmark.result.tofFrames);benchmark.state=BenchState::NextPhase;benchmark.stateMs=millis();return;}if(elapsed>kBenchResultTimeoutMs){++benchmark.failedPhases;emitBenchEvent(6,boat::BenchmarkStatus::Fail);benchmark.state=BenchState::NextPhase;benchmark.stateMs=millis();}return; }
  if (benchmark.state==BenchState::NextPhase) { if(++benchmark.phaseIndex>=sizeof(kBenchPhases)/sizeof(kBenchPhases[0])){benchmark.state=BenchState::FinalizingCampaign;benchmark.stateMs=millis();return;}benchmark.state=BenchState::PreparePhase;benchmark.stateMs=millis();return; }
  if (benchmark.state==BenchState::FinalizingCampaign) { const bool passed=benchmark.failedPhases==0;emitBenchEvent(7,passed?boat::BenchmarkStatus::Pass:boat::BenchmarkStatus::Fail,benchmark.failedPhases);if(logStats.logging){stopLog();appendBenchmarkSummary(passed?"completed":"completed_with_failures");}benchmark.state=BenchState::Completed; }
}

void sendGnssNav() {
  const boat::GnssNavPayload nav = makeNavPayload();
  if (!sendControl(boat::Type::GnssNav, &nav, sizeof(nav))) return;
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
  else if (command==CommandStartP1) {
    if (!loggingActive() && startLog("/P1")) {
      p1CaptureId=esp_random(); if(!p1CaptureId)p1CaptureId=1;
      if (!writeP1Journal()) { stopLog(); return; }
      p1CaptureActive=sendP1Capture(boat::P1CaptureAction::Start);
      if(!p1CaptureActive) { stopLog(); clearP1Journal(); }
    }
  } else if (command==CommandStopP1) {
    if (p1CaptureActive) sendP1Capture(boat::P1CaptureAction::Stop);
    p1CaptureActive=false;
    if(loggingActive()) stopLog();
    if (logStats.normalStop && !loggingActive()) clearP1Journal();
  }
  else if (command==CommandStartCalibration) startCalibration(pendingCalibrationKind);
  else if (command==CommandStopCalibration) stopCalibration();
  else if (command==CommandStop || command==CommandEstop) { if(benchmark.state!=BenchState::Idle&&benchmark.state!=BenchState::Completed&&benchmark.state!=BenchState::Aborted)finishBenchmark(command==CommandEstop?"emergency_stop":"user_stop",command==CommandEstop); boat::CommandPayload request{++commandSequence, static_cast<uint8_t>(command==CommandStop ? boat::Type::Stop : boat::Type::Estop), {0,0,0}}; const boat::Type type=command==CommandStop?boat::Type::Stop:boat::Type::Estop; sendControl(type,&request,sizeof(request)); emitLocal(type,&request,sizeof(request),1); }
  if (millis()-lastHeartbeatMs >= kControlHeartbeatIntervalMs) { lastHeartbeatMs=millis(); boat::HeartbeatPayload heartbeat{millis(),controlTxSequence,0,1,0}; sendControl(boat::Type::Heartbeat,&heartbeat,sizeof(heartbeat)); emitLocal(boat::Type::Heartbeat,&heartbeat,sizeof(heartbeat),1); }
}

const char page[] PROGMEM = R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;margin:12px;background:#101720;color:#edf3fa}.card{background:#1d2a38;border-radius:9px;padding:10px;margin:9px 0;white-space:pre-wrap}button{padding:9px;margin:2px}canvas{width:100%;height:130px;background:#080d13}</style><h2>ボート統合テレメトリ</h2><div class=card id=status>読み込み中</div><div class=card id=detail></div><div class=card><button onclick="post('/api/log/start')">記録開始</button><button onclick="post('/api/log/stop')">記録停止</button><button onclick="post('/api/control/stop')">STOP</button><button onclick="post('/api/control/estop')">E-STOP</button></div><canvas id=graph></canvas><script>let h=[];async function post(u){await fetch(u,{method:'POST'})}function draw(){let c=graph,w=c.width=c.clientWidth*devicePixelRatio,H=c.height=c.clientHeight*devicePixelRatio,x=c.getContext('2d');x.clearRect(0,0,w,H);x.strokeStyle='#79e5a0';x.beginPath();let m=Math.max(1,...h.map(Math.abs));h.forEach((v,i)=>{let X=i*w/159,Y=H/2-v/m*H*.42;i?x.lineTo(X,Y):x.moveTo(X,Y)});x.stroke()}async function update(){try{let j=await(await fetch('/api/latest',{cache:'no-store'})).json();status.textContent=`SD: ${j.sd}  記録: ${j.logging} (${j.run})\n制御リンク: ${j.control_age_ms} ms / frames ${j.control_frames} / error ${j.control_errors}\nBNO: ${j.bno} ${j.bno_fault}  age A/G/Q: ${j.accel_age_ms}/${j.gyro_age_ms}/${j.quat_age_ms} ms`;detail.textContent=`GNSS: ${j.gnss_receiving?'受信中':'未受信'}  fix: ${j.gnss_fix}  age: ${j.gnss_age_ms} ms\n緯度: ${j.lat_valid?j.lat.toFixed(7):'無効'}  経度: ${j.lon_valid?j.lon.toFixed(7):'無効'}\n高度: ${j.alt_valid?j.alt_m.toFixed(1)+' m':'無効'}  速度: ${j.speed_valid?j.speed_mps.toFixed(2)+' m/s':'無効'}  衛星: ${j.sats_valid?j.sats:'無効'}\n比較BNO Accel Z: ${j.az.toFixed(3)} m/s²  queue/drop: ${j.queue}/${j.drops}`;h.push(j.az);if(h.length>160)h.shift();draw()}catch(e){status.textContent='更新エラー'}}setInterval(update,50);update()</script></html>)HTML";

const char linkPage[] PROGMEM = R"HTML(<!doctype html><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:15px system-ui;background:#101720;color:#edf3fa;margin:12px}.card{background:#1d2a38;border-radius:8px;padding:10px;margin:8px 0;white-space:pre-wrap}button{padding:8px;margin:2px}canvas{width:100%;height:120px;background:#080d13}</style><h2>Boat GNSS round-trip / DRY RUN</h2><div class=card id=s>loading</div><div class=card><button onclick="post('/api/log/start')">Start log</button><button onclick="post('/api/log/stop')">Stop log</button><button onclick="post('/api/control/stop')">STOP</button><button onclick="post('/api/control/estop')">E-STOP</button></div><canvas id=g></canvas><script>let a=[];async function post(u){await fetch(u,{method:'POST'})}function draw(){let w=g.width=g.clientWidth*devicePixelRatio,h=g.height=g.clientHeight*devicePixelRatio,c=g.getContext('2d');c.clearRect(0,0,w,h);let m=Math.max(1,...a.map(Math.abs));c.strokeStyle='#79e5a0';c.beginPath();a.forEach((v,i)=>{let x=i*w/159,y=h/2-v/m*h*.42;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke()}async function load(){try{let j=await(await fetch('/api/link',{cache:'no-store'})).json();s.textContent=`SD ${j.sd}, logging ${j.logging}, run ${j.run}, records ${j.records}\nGNSS ${j.gnss_fix?'valid fix':'no fix'} age ${j.gnss_age_ms} ms: ${j.lat.toFixed(7)}, ${j.lon.toFixed(7)}, HDOP ${j.hdop.toFixed(2)}\nGNSS_NAV TX ${j.nav_tx}; result RX ${j.result_rx}; result age ${j.result_age_ms} ms; RTT ${j.rtt_ms} ms\nControl link ${j.link_healthy?'healthy':'STALE'}; CRC bad ${j.result_bad_crc}; ACK id ${j.ack_id}, age ${j.ack_age_ms} ms\nDRY_RUN ${j.result_dry_run}; control state ${j.control_state}`;a.push(j.north_m);if(a.length>160)a.shift();draw()}catch(e){s.textContent='update failed'}}setInterval(load,250);load()</script>)HTML";

const char clearPage[] PROGMEM = R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.card{background:#1d2a38;border-radius:10px;padding:12px;margin:10px 0}.pill{display:inline-block;padding:4px 8px;border-radius:99px;margin:2px;background:#46576a}.ok{background:#176b42}.bad{background:#8d3039}.wait{background:#8b641b}button{font:inherit;padding:12px 10px;margin:3px;border:0;border-radius:8px;background:#2f8fce;color:white}button:disabled{opacity:.45}.danger{background:#b32d3b}pre{margin:0;white-space:pre-wrap;font:14px ui-monospace,monospace}canvas{width:100%;height:110px;background:#090d12}</style><h2>ボート統合テレメトリ</h2><div class=card><span id=sd class=pill>SD確認中</span><span id=link class=pill>制御リンク確認中</span><span id=dry class=pill>DRY_RUN確認中</span><div id=feedback class=wait style="margin-top:8px;padding:8px;border-radius:6px">状態を取得中です。</div></div><div class=card><b>記録</b><br><button id=start onclick="act('/api/log/start','記録開始','start')">記録を開始</button><button id=stop onclick="act('/api/log/stop','記録停止','stop')">記録を停止</button><div id=loghint></div></div><div class=card><b>安全操作（制御側へ送信）</b><br><button id=bstop class=danger onclick="act('/api/control/stop','STOP','ack')">STOP</button><button id=bestop class=danger onclick="act('/api/control/estop','E-STOP','ack')">E-STOP</button><div>操作を押すと「送信要求」→「制御側ACK受信」を表示します。</div></div><div class=card><b>現在の状態</b><pre id=data>読み込み中</pre></div><canvas id=graph></canvas><script>let h=[],last={},pending=null;const state=['BOOT','DISARMED','ARMED_IDLE','RUNNING','E_STOP','FAULT'];function pill(e,ok,text){e.className='pill '+(ok?'ok':'bad');e.textContent=text}function say(t,c='wait'){feedback.className=c;feedback.textContent=t}function buttons(x){for(let e of document.querySelectorAll('button'))e.disabled=x}async function act(url,label,kind){buttons(true);pending={label,kind,ack:last.ack_id||0,at:Date.now()};say(label+'：通信側へ送信要求を登録しました。','wait');try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(r.status)}catch(e){pending=null;say(label+'：送信要求に失敗しました。','bad');buttons(false)}}function draw(){let w=graph.width=graph.clientWidth*devicePixelRatio,H=graph.height=graph.clientHeight*devicePixelRatio,c=graph.getContext('2d');c.clearRect(0,0,w,H);let m=Math.max(1,...h.map(Math.abs));c.strokeStyle='#78e39d';c.beginPath();h.forEach((v,i)=>{let x=i*w/159,y=H/2-v/m*H*.42;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke()}function confirm(j){if(!pending)return;if(pending.kind==='start'&&j.logging){say('記録開始：反映済みです。','ok');pending=null;buttons(false)}else if(pending.kind==='stop'&&!j.logging){say('記録停止：SDを閉じ、TXT概要を作成しました。','ok');pending=null;buttons(false)}else if(pending.kind==='ack'&&j.ack_id>pending.ack){let s=state[j.ack_state]||j.ack_state;say(pending.label+'：制御側ACK受信。状態は '+s+' です。','ok');pending=null;buttons(false)}else if(Date.now()-pending.at>600){say(pending.label+'：制御側の応答を待っています…','wait')}}async function load(){try{let j=await(await fetch('/api/ui',{cache:'no-store'})).json();last=j;pill(sd,j.sd==='ready','SD: '+j.sd);pill(link,j.link_healthy,'制御リンク: '+(j.link_healthy?'正常':'未接続/停止'));pill(dry,j.dry_run,'DRY_RUN: '+(j.dry_run?'有効':'無効'));loghint.textContent='現在: '+(j.logging?'記録中 '+j.run:'停止中')+' / '+j.records+' 件';data.textContent=`GNSS: ${j.gnss_fix?'有効fix':'fixなし'}  age ${j.gnss_age_ms} ms\n緯度 ${j.lat.toFixed(7)}  経度 ${j.lon.toFixed(7)}  HDOP ${j.hdop.toFixed(2)}\nGNSS_NAV送信 ${j.nav_tx} / 結果返信 ${j.result_rx} / RTT ${j.rtt_ms} ms\n制御状態 ${state[j.control_state]||j.control_state} / 結果age ${j.result_age_ms} ms\nACK id ${j.ack_id} / ACK age ${j.ack_age_ms} ms / SD drop ${j.queue_drops} / SD error ${j.sd_errors}`;h.push(j.north_m);if(h.length>160)h.shift();draw();confirm(j)}catch(e){say('画面の状態取得に失敗しました。接続を確認してください。','bad')}}setInterval(load,250);load()</script></html>)HTML";

// The default page deliberately requires an on-screen confirmation before a
// log is started. A stale tab or an automatic HTTP retry cannot create a RUN
// file by merely calling the former start endpoint.
const char manualPage[] PROGMEM = R"HTML(<!doctype html><html lang="ja"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.card{background:#1d2a38;border-radius:10px;padding:12px;margin:10px 0}.ok{color:#8ce8b1}.bad{color:#ff9da5}.warn{color:#ffd477}button{font:inherit;padding:12px;margin:4px;border:0;border-radius:8px;background:#2f8fce;color:#fff}.danger{background:#b32d3b}button:disabled{opacity:.45}pre{white-space:pre-wrap;margin:0}</style><h2>ボート通信・記録</h2><div class="card"><b id="state">状態を取得中</b><div id="message" class="warn">記録は停止中です。</div></div><div class="card"><b>SD 記録</b><p>「記録を開始」を押した後、確認を選んだ場合だけ新しい RUN ファイルを作成します。</p><button id="start" onclick="startLog()">記録を開始</button><button id="stop" class="danger" onclick="post('/api/log/stop','停止要求を送信しました。')">記録を停止</button></div><div class="card"><b>制御側へ送る安全操作</b><br><button class="danger" onclick="post('/api/control/stop','STOP を送信しました。')">STOP</button><button class="danger" onclick="post('/api/control/estop','E-STOP を送信しました。')">E-STOP</button></div><div class="card"><pre id="detail">読み込み中</pre></div><script>let busy=false;function show(t,c='warn'){let e=document.getElementById('message');e.className=c;e.textContent=t}async function post(url,text){if(busy)return;busy=true;try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(r.status);show(text,'warn')}catch(e){show('要求に失敗しました: '+e,'bad')}finally{busy=false}}function startLog(){if(confirm('SD に新しい RUN ファイルを作成して、記録を開始します。よろしいですか？'))post('/api/log/start?confirm=1','開始要求を送信しました。状態が「記録中」になることを確認してください。')}async function load(){try{let j=await(await fetch('/api/manual',{cache:'no-store'})).json();let e=document.getElementById('state');e.textContent='SD: '+j.sd+' / '+(j.logging?'記録中: '+j.run:'記録停止中');e.className=j.fault==='none'?'ok':'bad';document.getElementById('detail').textContent='records: '+j.records+'\nqueue drops: '+j.queue_drops+'\nSD write errors: '+j.sd_errors+'\nlog fault: '+j.fault+'\nGNSS NAV: '+j.nav_tx+' / result: '+j.result_rx+'\ncontrol link: '+(j.link_healthy?'正常':'未接続・古い')+' / DRY_RUN: '+j.dry_run;if(j.fault!=='none')show('記録異常: '+j.fault,'bad')}catch(e){show('状態の取得に失敗しました。','bad')}}setInterval(load,50);load()</script></html>)HTML";

bool startBenchmark(BenchPreset preset) {
  if (benchmark.state!=BenchState::Idle&&benchmark.state!=BenchState::Completed&&benchmark.state!=BenchState::Aborted) return false;
  if (!logStats.sdReady||!bno.ready||!kDryRunActuators) return false;
  if (!startLog(kBenchDirectory)) return false;
  benchmark.id=esp_random(); if(!benchmark.id)benchmark.id=1; benchmark.preset=preset; benchmark.startMs=benchmark.stateMs=millis(); benchmark.phaseIndex=0; benchmark.runNavStart=linkLatest.navTx; benchmark.runResultStart=linkLatest.resultRx; benchmark.runHeartbeatStart=0; benchmark.runBytesStart=0; benchmark.state=BenchState::Preflight;boat::CommandPayload stop{++commandSequence,(uint8_t)boat::Type::Stop,{0,0,0}};benchmark.stopCommandId=stop.commandId;if(!sendControl(boat::Type::Stop,&stop,sizeof(stop))){stopLog();return false;}emitLocal(boat::Type::Stop,&stop,sizeof(stop),1);
  emitBenchEvent(1,boat::BenchmarkStatus::Pass); Serial.printf("BENCH start id=%lu preset=%s run=%s\n",(unsigned long)benchmark.id,benchPresetName(preset),logStats.runName); return true;
}
void apiBenchmark() { const uint32_t now=millis(); const uint32_t phaseElapsed=benchmark.phaseStartMs?now-benchmark.phaseStartMs:0; const uint32_t total=benchmark.startMs?now-benchmark.startMs:0; char json[1800]; snprintf(json,sizeof(json),"{\"state\":\"%s\",\"campaign_id\":%lu,\"preset\":\"%s\",\"run\":\"%s\",\"phase\":%u,\"phase_total\":%u,\"phase_elapsed_ms\":%lu,\"total_elapsed_ms\":%lu,\"cable_profile\":\"%s\",\"cable_length_cm\":%u,\"wiring_type\":\"%s\",\"i2c_hz\":%lu,\"dry_run\":%s,\"sd\":\"%s\",\"sd_errors\":%lu,\"queue_drops\":%lu,\"control_ready\":%s,\"control_age_ms\":%lu,\"ready_status\":%u,\"result_status\":%u,\"ina_fresh\":%lu,\"ina_duplicates\":%lu,\"tof_frames\":%lu,\"synthetic_rx\":%lu,\"i2c_errors\":%lu,\"max_tof_read_us\":%lu,\"free_heap\":%lu,\"fault\":\"%s\"}",benchStateName(benchmark.state),(unsigned long)benchmark.id,benchPresetName(benchmark.preset),logStats.runName,benchmark.phaseIndex,(unsigned)(sizeof(kBenchPhases)/sizeof(kBenchPhases[0])),(unsigned long)phaseElapsed,(unsigned long)total,benchmark.cableProfile,benchmark.cableLengthCm,benchmark.wiringType,(unsigned long)(benchmark.phaseIndex<sizeof(kBenchPhases)/sizeof(kBenchPhases[0])?currentBenchPhase().i2cHz:0),kDryRunActuators?"true":"false",logStats.sdReady?"ready":"error",(unsigned long)logStats.writeErrors,(unsigned long)logStats.queueDrops,benchmark.controlReady?"true":"false",(unsigned long)ageMs(linkLatest.lastHeartbeatUs,nowUs()),benchmark.ready.status,benchmark.result.status,(unsigned long)benchmark.result.inaFresh,(unsigned long)benchmark.result.inaDuplicates,(unsigned long)benchmark.result.tofFrames,(unsigned long)benchmark.result.syntheticRx,(unsigned long)benchmark.result.i2cErrors,(unsigned long)benchmark.result.maxTofReadUs,(unsigned long)benchmark.result.freeHeap,logStats.fault); web.send(200,"application/json",json); }
void requestBenchmarkStart() { if(!web.hasArg("confirm")||web.arg("confirm")!="1"){web.send(400,"application/json","{\"error\":\"confirmation required\"}");return;} BenchPreset preset=BenchPreset::Quick; memset(&benchmark,0,sizeof(benchmark)); snprintf(benchmark.cableProfile,sizeof(benchmark.cableProfile),"%s",web.hasArg("cable")?web.arg("cable").c_str():"CABLE_10CM");snprintf(benchmark.wiringType,sizeof(benchmark.wiringType),"%s",web.hasArg("wiring")?web.arg("wiring").c_str():"direct");snprintf(benchmark.pullup,sizeof(benchmark.pullup),"%s",web.hasArg("pullup")?web.arg("pullup").c_str():"unknown");snprintf(benchmark.target,sizeof(benchmark.target),"%s",web.hasArg("target")?web.arg("target").c_str():"unknown");snprintf(benchmark.note,sizeof(benchmark.note),"%s",web.hasArg("note")?web.arg("note").c_str():"");benchmark.cableLengthCm=web.hasArg("length")?(uint16_t)web.arg("length").toInt():10;benchmark.customPhaseMs=0;if(!startBenchmark(preset)){web.send(409,"application/json","{\"error\":\"preflight/log/dry-run rejected\"}");return;}web.send(202,"application/json","{\"requested\":\"benchmark\"}"); }
void requestBenchmarkStop() { if(benchmark.state==BenchState::Idle){web.send(409,"application/json","{\"error\":\"not running\"}");return;}finishBenchmark("user_stop",false);web.send(202,"application/json","{\"requested\":\"safe stop\"}"); }
const char benchmarkPage[] PROGMEM = R"HTML(<!doctype html><html lang="ja"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;margin:10px 0}label{display:block;margin:7px 0}button,select,input{font:inherit;padding:8px;margin:3px}.danger{background:#a22;color:#fff}pre{white-space:pre-wrap}</style><h2>Automated Benchmark</h2><div class=c><b id=s>状態を取得中</b><pre id=d></pre></div><div class=c><label>Preset <select id=p><option>QUICK</option><option>STANDARD</option><option>ENDURANCE</option><option>CUSTOM</option></select></label><label>Cable <select id=c><option>CABLE_10CM</option><option>CABLE_1M_DIRECT</option><option>CABLE_1M_DIFFERENTIAL</option><option>CUSTOM</option></select></label><label>length cm <input id=l value=10 type=number></label><label>wiring <input id=w value=direct></label><label>pull-up ohm <input id=u value=unknown></label><label>target mm <input id=t value=unknown></label><label>note <input id=n></label><label>CUSTOM phase seconds <input id=x value=120 type=number></label><button onclick="go()">Start automated benchmark</button><button class=danger onclick="post('/api/benchmark/stop')">Stop benchmark</button><button class=danger onclick="post('/api/control/estop')">E-STOP</button></div><script>async function post(u){let r=await fetch(u,{method:'POST'});if(!r.ok)alert(await r.text())}function go(){if(!confirm('DRY_RUNで自動測定を開始します。1キャンペーン分のBENCHログを作成します。'))return;let q='?confirm=1&preset='+p.value+'&cable='+c.value+'&length='+l.value+'&wiring='+encodeURIComponent(w.value)+'&pullup='+encodeURIComponent(u.value)+'&target='+encodeURIComponent(t.value)+'&note='+encodeURIComponent(n.value)+'&duration_s='+x.value;post('/api/benchmark/start'+q)}async function load(){try{let j=await(await fetch('/api/benchmark',{cache:'no-store'})).json();s.textContent=j.state+' / '+j.preset+' / '+j.run+' / phase '+j.phase+'/'+j.phase_total;d.textContent='campaign '+j.campaign_id+'\nphase '+j.phase_elapsed_ms+' ms / total '+j.total_elapsed_ms+' ms\nI2C '+j.i2c_hz+' Hz / DRY_RUN '+j.dry_run+'\nSD '+j.sd+' error '+j.sd_errors+' drop '+j.queue_drops+'\nINA fresh/duplicate '+j.ina_fresh+'/'+j.ina_duplicates+'\nToF '+j.tof_frames+' / synthetic RX '+j.synthetic_rx+'\nfault '+j.fault}catch(e){s.textContent='更新失敗'}}setInterval(load,50);load()</script></html>)HTML";

const char simpleBenchmarkPage[] PROGMEM = R"HTML(<!doctype html><html lang="ja"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:17px system-ui;margin:14px;background:#101720;color:#f1f6fb}.card{background:#1d2a38;border-radius:12px;padding:15px;margin:12px 0}.state{font-size:21px;font-weight:700}.ok{color:#91e7b0}.warn{color:#ffda83}.bad{color:#ffabb0}button,select{font:inherit;padding:14px;border:0;border-radius:9px;margin:4px}.start{width:100%;font-size:20px;font-weight:700;background:#168450;color:white}.stop{background:#b98616;color:white}.estop{background:#b32d3b;color:white}details{margin-top:12px}pre{white-space:pre-wrap;margin:8px 0 0}.small{font-size:14px;color:#c7d2dd}canvas{width:100%;height:110px;background:#090d12}</style><h2>ボート 自動測定</h2><div class=card><div id=s class=state>機器の状態を確認中です</div><div id=m class=warn>この画面では、開始ボタンを一度押すだけです。</div><pre id=d></pre></div><canvas id=g></canvas><div class=card><b>接続中のToFケーブル</b><br><select id=c><option value=CABLE_10CM>約10 cm（現在の標準）</option><option value=CABLE_1M_DIRECT>約1 m・直接配線</option><option value=CABLE_1M_DIFFERENTIAL>約1 m・差動配線</option></select><p class=small>選んだ条件は結果ファイルに保存されます。</p><button id=start class=start onclick="start()">自動測定を開始する</button><p class=small>開始後は、停止確認 → センサ確認 → 固定条件の1測定 → SD保存 → 終了まで自動です。ブラウザを閉じても継続します。</p></div><div class=card><b>途中で止める場合だけ使います</b><br><button class=stop onclick="post('/api/benchmark/stop')">安全に停止</button><button class=estop onclick="post('/api/control/estop')">緊急停止 E-STOP</button></div><details class=card><summary>詳細設定（通常は不要）</summary><p class=small>初回は変更しないでください。測定時間は選んだ固定ファームウェアで決まります。</p></details><script>let h=[];function draw(){let w=g.width=g.clientWidth*devicePixelRatio,H=g.height=g.clientHeight*devicePixelRatio,c=g.getContext("2d");c.clearRect(0,0,w,H);let m=Math.max(1,...h);c.strokeStyle="#78e39d";c.beginPath();h.forEach((v,i)=>{let x=i*w/159,y=H-v/m*H*.85;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke()}const label={IDLE:'開始できます',PREFLIGHT:'安全確認を自動実行中',PREPARE_PHASE:'測定器を設定中',WAIT_CONTROL_READY:'制御側の準備完了を待機中',WARMUP:'センサを安定化中',MEASURING:'自動測定中',FINALIZING_PHASE:'この測定を保存中',NEXT_PHASE:'次の測定を準備中',FINALIZING_CAMPAIGN:'結果ファイルを閉じています',COMPLETED:'完了しました',ABORTED:'中断しました',E_STOP:'緊急停止中'};async function post(u){let r=await fetch(u,{method:'POST'});if(!r.ok)alert('操作できません: '+await r.text())}function start(){if(!confirm('DRY_RUN（実アクチュエータを動かさない）で、この固定条件の自動測定を開始します。よろしいですか？'))return;post('/api/benchmark/start?confirm=1&preset=QUICK&cable='+c.value+'&length='+(c.value==='CABLE_10CM'?10:100)+'&wiring='+encodeURIComponent(c.value==='CABLE_1M_DIFFERENTIAL'?'differential':'direct')+'&pullup=unknown&target=unknown')}async function load(){try{let j=await(await fetch('/api/benchmark',{cache:'no-store'})).json();let running=!['IDLE','COMPLETED','ABORTED','E_STOP'].includes(j.state);s.textContent=label[j.state]||j.state; s.className='state '+(j.state==='COMPLETED'?'ok':j.state==='ABORTED'||j.state==='E_STOP'||j.fault!=='none'?'bad':'warn');m.textContent=running?'操作は不要です。自動で進行しています。':'開始ボタンを一度押すと、全工程を自動で実行します。';start.disabled=running;d.textContent='制御リンク age: '+j.control_age_ms+' ms\n進行: '+j.phase+' / '+j.phase_total+' フェーズ\n経過: '+Math.floor(j.total_elapsed_ms/60000)+' 分\nSD: '+j.sd+' / 書込みエラー: '+j.sd_errors+' / キュードロップ: '+j.queue_drops+'\nDRY_RUN: '+j.dry_run+'\n現在の記録: '+j.run+(j.fault!=='none'?'\n異常: '+j.fault:'');h.push(j.tof_frames);if(h.length>160)h.shift();draw()}catch(e){s.textContent='通信側XIAOへ接続できません';s.className='state bad'}}setInterval(load,50);load()</script></html>)HTML";

void apiManual() {
  const uint64_t now=nowUs(); char json[700];
  snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"queue_drops\":%lu,\"sd_errors\":%lu,\"fault\":\"%s\",\"nav_tx\":%lu,\"result_rx\":%lu,\"link_healthy\":%s,\"dry_run\":%s}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,logStats.fault,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,ageMs(linkLatest.lastHeartbeatUs,now)<=kControlLinkTimeoutMs?"true":"false",linkLatest.result.dryRun?"true":"false");
  web.send(200,"application/json",json);
}

void apiUi() { const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); char json[1600]; snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"queue_drops\":%lu,\"sd_errors\":%lu,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"hdop\":%.2f,\"nav_tx\":%lu,\"result_rx\":%lu,\"result_age_ms\":%lu,\"rtt_ms\":%lu,\"link_healthy\":%s,\"dry_run\":%s,\"control_state\":%u,\"north_m\":%.3f,\"ack_id\":%lu,\"ack_state\":%u,\"ack_age_ms\":%lu}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.hdop,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)ageMs(linkLatest.lastResultUs,now),(unsigned long)(linkLatest.lastRttUs/1000ULL),ageMs(linkLatest.lastHeartbeatUs,now)<=kControlLinkTimeoutMs?"true":"false",linkLatest.result.dryRun?"true":"false",linkLatest.result.safetyState,linkLatest.result.northMm/1000.0f,(unsigned long)linkLatest.lastCommandId,linkLatest.ack.safetyState,(unsigned long)ageMs(linkLatest.lastAckUs,now)); web.send(200,"application/json",json); }

void apiLink() { const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); char json[1500]; snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"hdop\":%.2f,\"nav_tx\":%lu,\"result_rx\":%lu,\"result_age_ms\":%lu,\"rtt_ms\":%lu,\"link_healthy\":%s,\"result_bad_crc\":%lu,\"result_dry_run\":%u,\"control_state\":%u,\"north_m\":%.3f,\"ack_id\":%lu,\"ack_age_ms\":%lu}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.hdop,(unsigned long)linkLatest.navTx,(unsigned long)linkLatest.resultRx,(unsigned long)ageMs(linkLatest.lastResultUs,now),(unsigned long)(linkLatest.lastRttUs/1000ULL),ageMs(linkLatest.lastHeartbeatUs,now)<=kControlLinkTimeoutMs?"true":"false",(unsigned long)linkLatest.resultBadCrc,linkLatest.result.dryRun,linkLatest.result.safetyState,linkLatest.result.northMm/1000.0f,(unsigned long)linkLatest.lastCommandId,(unsigned long)ageMs(linkLatest.lastAckUs,now)); web.send(200,"application/json",json); }

void apiLatest() {
  const uint64_t now=nowUs(); const auto& g=gnssRx.latest(); uint16_t used; portENTER_CRITICAL(&queueMux); used=queueUsed; portEXIT_CRITICAL(&queueMux);
  char json[1800];
  snprintf(json,sizeof(json),"{\"sd\":\"%s\",\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"queue\":%u,\"drops\":%lu,\"control_frames\":%lu,\"control_errors\":%lu,\"control_age_ms\":%lu,\"bno\":%s,\"bno_fault\":\"%s\",\"accel_age_ms\":%lu,\"gyro_age_ms\":%lu,\"quat_age_ms\":%lu,\"magnetic_age_ms\":%lu,\"magnetic_valid\":%s,\"magnetic_accuracy\":%u,\"ax\":%.5f,\"ay\":%.5f,\"az\":%.5f,\"mx_ut\":%.3f,\"my_ut\":%.3f,\"mz_ut\":%.3f,\"gnss_receiving\":%s,\"gnss_fix\":%s,\"gnss_age_ms\":%lu,\"lat\":%.8f,\"lon\":%.8f,\"alt_m\":%.2f,\"speed_mps\":%.3f,\"course_deg\":%.2f,\"sats\":%u,\"hdop\":%.2f,\"lat_valid\":%s,\"lon_valid\":%s,\"alt_valid\":%s,\"speed_valid\":%s,\"sats_valid\":%s}",logStats.sdReady?"ready":"error",logStats.logging?"true":"false",logStats.runName,(unsigned long)logStats.records,used,(unsigned long)logStats.queueDrops,(unsigned long)logStats.controlFrames,(unsigned long)(logStats.controlCrc+logStats.controlCobs+logStats.controlLength),(unsigned long)ageMs(lastControlFrameUs,now),bno.ready?"true":"false",bno.fault,(unsigned long)ageMs(bno.accelUs,now),(unsigned long)ageMs(bno.gyroUs,now),(unsigned long)ageMs(bno.quatUs,now),(unsigned long)ageMs(bno.magneticUs,now),bno.magneticValid?"true":"false",bno.magneticAccuracy,bno.ax,bno.ay,bno.az,bno.mx,bno.my,bno.mz,gnssRx.receiving(now)?"true":"false",(g.flags&gnss::FixValid)?"true":"false",(unsigned long)ageMs(g.lastSentenceEndUs,now),g.latitude,g.longitude,g.altitudeM,g.speedMps,g.courseDeg,g.satellites,g.hdop,(g.flags&gnss::LatitudeValid)?"true":"false",(g.flags&gnss::LongitudeValid)?"true":"false",(g.flags&gnss::AltitudeValid)?"true":"false",(g.flags&gnss::SpeedValid)?"true":"false",(g.flags&gnss::SatellitesValid)?"true":"false");
  web.send(200,"application/json",json);
}
void requestStart() { if(!web.hasArg("confirm") || web.arg("confirm")!="1") { web.send(400,"application/json","{\"error\":\"confirmation required\"}"); return; } pendingCommand=CommandStartLog; web.send(202,"application/json","{\"requested\":\"start\"}"); }
void requestStop() { pendingCommand=CommandStopLog; web.send(202,"application/json","{\"requested\":\"stop\"}"); }
void requestP1Start() { if(!web.hasArg("confirm") || web.arg("confirm")!="1") { web.send(400,"application/json","{\"error\":\"confirmation required\"}"); return; } if(loggingActive()) { web.send(409,"application/json","{\"error\":\"another log is active\"}"); return; } pendingCommand=CommandStartP1; web.send(202,"application/json","{\"requested\":\"p1_start\"}"); }
void requestP1Stop() { pendingCommand=CommandStopP1; web.send(202,"application/json","{\"requested\":\"p1_stop\"}"); }
void apiP1() { char json[440]; snprintf(json,sizeof(json),"{\"capture_active\":%s,\"logging\":%s,\"capture_id\":%lu,\"run\":\"%s\",\"records\":%lu,\"queue_drops\":%lu,\"sd_errors\":%lu,\"boot_id\":%lu,\"reset_reason\":%d,\"recovery_detected\":%s,\"recovered_run\":\"%s\"}",p1CaptureActive?"true":"false",loggingActive()?"true":"false",(unsigned long)p1CaptureId,logStats.runName,(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,(unsigned long)commBootId,static_cast<int>(bootResetReason),p1RecoveryDetected?"true":"false",p1RecoveredRun); web.send(200,"application/json",json); }
bool parseCalibrationKind(const String& name, boat::CalibrationKind& kind) {
  if(name=="STATIC_6FACE")kind=boat::CalibrationKind::Static6Face;
  else if(name=="ROTATION_X")kind=boat::CalibrationKind::RotationX;
  else if(name=="ROTATION_Y")kind=boat::CalibrationKind::RotationY;
  else if(name=="ROTATION_Z")kind=boat::CalibrationKind::RotationZ;
  else if(name=="GYRO_BIAS")kind=boat::CalibrationKind::GyroBias;
  else if(name=="MAG")kind=boat::CalibrationKind::Magnetic;
  else if(name=="TIME_OFFSET")kind=boat::CalibrationKind::TimeOffset;
  else if(name=="TOF")kind=boat::CalibrationKind::Tof;
  else if(name=="SERVO_GEOMETRY")kind=boat::CalibrationKind::ServoGeometry;
  else if(name=="VESC_TELEMETRY")kind=boat::CalibrationKind::VescTelemetry;
  else return false;
  return true;
}
void requestCalibrationStart() { boat::CalibrationKind kind{}; if(!web.hasArg("confirm")||web.arg("confirm")!="1"||!web.hasArg("kind")||!parseCalibrationKind(web.arg("kind"),kind)){web.send(400,"application/json","{\"error\":\"confirm=1 and supported kind required\"}");return;} if(loggingActive()){web.send(409,"application/json","{\"error\":\"another log is active\"}");return;} pendingCalibrationKind=kind;pendingCommand=CommandStartCalibration;web.send(202,"application/json","{\"requested\":\"calibration_start\",\"output_enabled\":false}");}
void requestCalibrationStop() { pendingCommand=CommandStopCalibration; web.send(202,"application/json","{\"requested\":\"calibration_stop\"}"); }
void apiCalibration() { char json[640];snprintf(json,sizeof(json),"{\"mode\":\"%s\",\"active\":%s,\"session_id\":%lu,\"run\":\"%s\",\"logging\":%s,\"records\":%lu,\"queue_drops\":%lu,\"sd_errors\":%lu,\"control_output_enabled\":false,\"virtual_mode\":\"DISARMED\",\"instruction\":\"%s\"}",calibrationKindName(calibration.kind),calibration.active?"true":"false",(unsigned long)calibration.sessionId,calibration.runName,loggingActive()?"true":"false",(unsigned long)logStats.records,(unsigned long)logStats.queueDrops,(unsigned long)logStats.writeErrors,calibration.kind==boat::CalibrationKind::Static6Face?"Place one requested face, wait still, then stop. Repeat each face as a separate RUN.":"Keep all physical outputs disabled; start, perform only the named motion, then stop.");web.send(200,"application/json",json); }
const char calibrationPage[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;margin:8px 0;white-space:pre-wrap}button,select{font:inherit;padding:9px;margin:3px}.danger{background:#a22;color:white}</style><h2>CALIBRATION（実出力なし）</h2><div class=c id=v>取得中...</div><div class=c><select id=k><option>STATIC_6FACE</option><option>ROTATION_X</option><option>ROTATION_Y</option><option>ROTATION_Z</option><option>GYRO_BIAS</option><option>MAG</option><option>TIME_OFFSET</option><option>TOF</option><option>SERVO_GEOMETRY</option><option>VESC_TELEMETRY</option></select><button onclick="fetch('/api/calibration/start?confirm=1&kind='+k.value,{method:'POST'})">開始</button><button class=danger onclick="fetch('/api/calibration/stop',{method:'POST'})">停止</button></div><p class=c>CALIBRATIONは主副IMUの生値・時刻・精度・UART/SD時刻を/CAL/RUNxxxx.BINへ記録します。PCA9685、VESC、実翼への書込みはありません。STATIC_6FACEは +X, -X, +Y, -Y, +Z, -Z を各々別RUNで実施します。</p><script>async function u(){try{let j=await(await fetch('/api/calibration',{cache:'no-store'})).json();v.textContent=`mode: ${j.mode}\nactive/logging: ${j.active}/${j.logging}\nrun/session: ${j.run}/${j.session_id}\nrecords/drop/SD error: ${j.records}/${j.queue_drops}/${j.sd_errors}\noutput: ${j.virtual_mode}, enabled=${j.control_output_enabled}\n${j.instruction}`}catch(e){v.textContent='取得失敗'}}setInterval(u,100);u()</script></html>)HTML";
const char p1Page[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;margin:8px 0;white-space:pre-wrap}button{padding:10px;margin:3px}</style><h2>P1 IMU軸較正ログ</h2><div class=c id=v>取得中...</div><button onclick="fetch('/api/p1/start?confirm=1',{method:'POST'})">P1記録を開始</button><button onclick="fetch('/api/p1/stop',{method:'POST'})">P1記録を停止</button><p class=c>DRY_RUN専用。開始中は主副BNOの加速度・ジャイロ・地磁気を、BNO測定時刻、コールバック時刻、通信側ログ投入時刻付きで保存します。異常再起動時は次回起動後にP1回復記録を残します。アクチュエータは動かしません。</p><script>async function u(){try{let j=await(await fetch('/api/p1',{cache:'no-store'})).json();v.textContent=`capture: ${j.capture_active}\nlogging: ${j.logging}\nrun: ${j.run}\nrecords: ${j.records}\nqueue drops / SD errors: ${j.queue_drops} / ${j.sd_errors}\nboot: ${j.boot_id} reset: ${j.reset_reason}\nrecovery: ${j.recovery_detected} (${j.recovered_run})`}catch(e){v.textContent='更新失敗'}}setInterval(u,250);u()</script></html>)HTML";
void requestControlStop() { pendingCommand=CommandStop; web.send(202,"application/json","{\"requested\":\"stop\"}"); }
void requestControlEstop() { pendingCommand=CommandEstop; web.send(202,"application/json","{\"requested\":\"estop\"}"); }

bool validBenchDownloadPath(const String& path) {
  const bool bench=path.startsWith("/BENCH/RUN")&&path.length()==18&&path.charAt(14)=='.';
  const bool p1=path.startsWith("/P1/RUN")&&path.length()==15&&path.charAt(11)=='.';
  const bool cal=path.startsWith("/CAL/RUN")&&path.length()==16&&path.charAt(12)=='.';
  if(!bench&&!p1&&!cal)return false;
  const uint8_t first=bench?10:(p1?7:8);
  for(uint8_t i=first;i<first+4;++i)if(path.charAt(i)<'0'||path.charAt(i)>'9')return false;
  return path.endsWith(".BIN")||path.endsWith(".TXT");
}

void apiDownload() {
  if (loggingActive()) { web.send(409, "application/json", "{\"error\":\"logging active\"}"); return; }
  if (!web.hasArg("file") || !validBenchDownloadPath(web.arg("file"))) { web.send(400, "application/json", "{\"error\":\"invalid benchmark file\"}"); return; }
  const String path = web.arg("file");
  File file = SD.open(path, FILE_READ);
  if (!file) { web.send(404, "application/json", "{\"error\":\"file not found\"}"); return; }
  web.sendHeader("Content-Disposition", "attachment; filename=" + path.substring(7));
  web.streamFile(file, path.endsWith(".BIN") ? "application/octet-stream" : "text/plain; charset=utf-8");
  file.close();
}

// Sensor API and page declarations.
const char sensorPage[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;white-space:pre-wrap}</style><h2>BNO08X ????</h2><div class=c id=v>?????</div><script>async function u(){try{let j=await(await fetch('/api/latest',{cache:'no-store'})).json();v.textContent=`??? [m/s?]  X ${j.ax.toFixed(3)}  Y ${j.ay.toFixed(3)}  Z ${j.az.toFixed(3)}\n???? [rad/s]  X ${j.gx?.toFixed(3)??'n/a'}  Y ${j.gy?.toFixed(3)??'n/a'}  Z ${j.gz?.toFixed(3)??'n/a'}\n??? [?T]  X ${j.mx_ut.toFixed(2)}  Y ${j.my_ut.toFixed(2)}  Z ${j.mz_ut.toFixed(2)}\n???: ${j.magnetic_valid?'??':'??'} / ?? ${j.magnetic_accuracy}/3 / ?? ${j.magnetic_age_ms} ms\nBNO: ${j.bno?'??':'??'} ${j.bno_fault}`;}catch(e){v.textContent='?????'}}setInterval(u,50);u()</script></html>)HTML";

void apiSensors() {
  const uint64_t now=nowUs();
  BnoMetrics metrics{}; portENTER_CRITICAL(&bnoTaskMux); metrics=bnoMetrics; portEXIT_CRITICAL(&bnoTaskMux);
  const UBaseType_t eventQueueUsed=bnoEventQueue ? uxQueueMessagesWaiting(bnoEventQueue) : 0;
  char json[1200];
  snprintf(json,sizeof(json),"{\"bno\":%s,\"fault\":\"%s\",\"reinits\":%lu,\"ax\":%.5f,\"ay\":%.5f,\"az\":%.5f,\"gx\":%.5f,\"gy\":%.5f,\"gz\":%.5f,\"mx_ut\":%.3f,\"my_ut\":%.3f,\"mz_ut\":%.3f,\"magnetic_valid\":%s,\"magnetic_accuracy\":%u,\"magnetic_age_ms\":%lu,\"int_edges\":%lu,\"task_fallbacks\":%lu,\"service_calls\":%lu,\"callback_events\":%lu,\"decode_errors\":%lu,\"event_queue_drops\":%lu,\"event_queue_used\":%u,\"event_queue_high_water\":%u,\"accel_events\":%lu,\"gyro_events\":%lu,\"magnetic_events\":%lu,\"ignored_events\":%lu,\"max_service_us\":%lu}",bno.ready?"true":"false",bno.fault,(unsigned long)bno.reinits,bno.ax,bno.ay,bno.az,bno.gx,bno.gy,bno.gz,bno.mx,bno.my,bno.mz,bno.magneticValid?"true":"false",bno.magneticAccuracy,(unsigned long)ageMs(bno.magneticUs,now),(unsigned long)metrics.intEdges,(unsigned long)metrics.taskFallbacks,(unsigned long)metrics.serviceCalls,(unsigned long)metrics.callbackEvents,(unsigned long)metrics.decodeErrors,(unsigned long)metrics.eventQueueDrops,(unsigned)eventQueueUsed,(unsigned)metrics.eventQueueHighWater,(unsigned long)metrics.accelEvents,(unsigned long)metrics.gyroEvents,(unsigned long)metrics.magneticEvents,(unsigned long)metrics.ignoredEvents,(unsigned long)metrics.maxServiceUs);
  web.send(200,"application/json",json);
}

void transformSecondary(const float in[3], float out[3]) {
  out[0]=kDualImuR00*in[0]+kDualImuR01*in[1]+kDualImuR02*in[2];
  out[1]=kDualImuR10*in[0]+kDualImuR11*in[1]+kDualImuR12*in[2];
  out[2]=kDualImuR20*in[0]+kDualImuR21*in[1]+kDualImuR22*in[2];
}

const char* provisionalDualReason(uint64_t now) {
  if (!primaryImu.receivedUs || ageMs(primaryImu.receivedUs,now)>kDualImuStaleMs) return "primary_stale";
  if (!bno.accelUs || !bno.gyroUs || ageMs(bno.accelUs,now)>kDualImuStaleMs || ageMs(bno.gyroUs,now)>kDualImuStaleMs) return "secondary_stale";
  if (!isfinite(provisional.accelDeltaMps2) || provisional.accelDeltaMps2>kProvisionalMaxAccelDeltaMps2) return "accel_delta";
  if (!isfinite(provisional.gyroDeltaRadS) || provisional.gyroDeltaRadS>kProvisionalMaxGyroDeltaRadS) return "gyro_delta";
  return "accepted";
}

void emitProvisionalSystem(uint64_t now) {
  if (!loggingActive() || now-provisional.lastLogUs < kProvisionalLogIntervalMs*1000ULL) return;
  provisional.lastLogUs=now;
  boat::ProvisionalSystemPayload payload{};
  payload.estimateUs=now; payload.rollRad=provisional.rollRad; payload.pitchRad=provisional.pitchRad; payload.yawRad=provisional.yawRad;
  payload.rollRateRadS=provisional.rollRateRadS; payload.pitchRateRadS=provisional.pitchRateRadS; payload.yawRateRadS=provisional.yawRateRadS;
  payload.latitudeDeg=provisional.latitudeDeg; payload.longitudeDeg=provisional.longitudeDeg; payload.groundSpeedMps=provisional.groundSpeedMps; payload.courseRad=provisional.courseRad; payload.waterHeightM=provisional.waterHeightM;
  payload.accelDeltaMps2=provisional.accelDeltaMps2; payload.gyroDeltaRadS=provisional.gyroDeltaRadS;
  payload.primaryAgeMs=ageMs(primaryImu.receivedUs,now); payload.secondaryAgeMs=max(ageMs(bno.accelUs,now),ageMs(bno.gyroUs,now)); payload.gnssAgeMs=ageMs(gnssRx.latest().lastValidFixUs,now); payload.tofAgeMs=ageMs(tofLatest.receivedUs,now);
  payload.tofCenterMm=tofLatest.centerMm; payload.flags=provisional.flags; payload.virtualMode=0;  // 0=DISARMED; no command is transmitted.
  emitLocal(boat::Type::ProvisionalSystem,&payload,sizeof(payload),1);
}

void serviceProvisionalSystem() {
  const uint64_t now=nowUs();
  if (provisional.lastServiceUs && now-provisional.lastServiceUs<kProvisionalSystemIntervalMs*1000ULL) return;
  const float dt=provisional.lastServiceUs ? constrain((now-provisional.lastServiceUs)/1000000.0f,0.001f,0.10f) : 0.0f;
  provisional.lastServiceUs=now;
  provisional.flags=ProvisionalOutputBlocked|ProvisionalCalibrationPending;
  provisional.attitudeAvailable=linkLatest.estimatedStateUs && ageMs(linkLatest.estimatedStateUs,now)<=kDualImuStaleMs;
  const bool primaryFresh=primaryImu.receivedUs && ageMs(primaryImu.receivedUs,now)<=kDualImuStaleMs;
  const bool secondaryFresh=bno.accelUs&&bno.gyroUs&&ageMs(bno.accelUs,now)<=kDualImuStaleMs&&ageMs(bno.gyroUs,now)<=kDualImuStaleMs;
  float secondaryAccel[3]{},secondaryGyro[3]{};
  if (primaryFresh&&secondaryFresh) {
    const float localAccel[3]={bno.ax,bno.ay,bno.az},localGyro[3]={bno.gx,bno.gy,bno.gz};
    transformSecondary(localAccel,secondaryAccel); transformSecondary(localGyro,secondaryGyro);
    provisional.accelDeltaMps2=sqrtf(sq(primaryImu.sample.accel[0]-secondaryAccel[0])+sq(primaryImu.sample.accel[1]-secondaryAccel[1])+sq(primaryImu.sample.accel[2]-secondaryAccel[2]));
    provisional.gyroDeltaRadS=sqrtf(sq(primaryImu.sample.gyro[0]-secondaryGyro[0])+sq(primaryImu.sample.gyro[1]-secondaryGyro[1])+sq(primaryImu.sample.gyro[2]-secondaryGyro[2]));
  } else { provisional.accelDeltaMps2=NAN; provisional.gyroDeltaRadS=NAN; }
  provisional.dualAccepted=primaryFresh&&secondaryFresh&&isfinite(provisional.accelDeltaMps2)&&isfinite(provisional.gyroDeltaRadS)&&provisional.accelDeltaMps2<=kProvisionalMaxAccelDeltaMps2&&provisional.gyroDeltaRadS<=kProvisionalMaxGyroDeltaRadS;
  const boat::EstimatedStatePayload& primaryState=linkLatest.estimatedState;
  if (provisional.attitudeAvailable) {
    provisional.flags|=ProvisionalAttitudeBaseline;
    float rate[3]={primaryState.rollRateRadS,primaryState.pitchRateRadS,primaryState.yawRateRadS};
    if (provisional.dualAccepted) {
      provisional.flags|=ProvisionalDualImuAccepted;
      const float fused[3]={(primaryImu.sample.gyro[0]+secondaryGyro[0])*0.5f,(primaryImu.sample.gyro[1]+secondaryGyro[1])*0.5f,(primaryImu.sample.gyro[2]+secondaryGyro[2])*0.5f};
      for(uint8_t i=0;i<3;++i) { provisional.attitudeCorrection[i]=constrain(0.98f*provisional.attitudeCorrection[i]+(fused[i]-rate[i])*dt,-kProvisionalMaxDualCorrectionRad,kProvisionalMaxDualCorrectionRad); rate[i]=fused[i]; }
    } else for(uint8_t i=0;i<3;++i) provisional.attitudeCorrection[i]*=0.90f;
    provisional.rollRad=primaryState.rollRad+provisional.attitudeCorrection[0]; provisional.pitchRad=primaryState.pitchRad+provisional.attitudeCorrection[1]; provisional.yawRad=primaryState.yawRad+provisional.attitudeCorrection[2];
    provisional.rollRateRadS=rate[0]; provisional.pitchRateRadS=rate[1]; provisional.yawRateRadS=rate[2];
  } else { provisional.attitudeCorrection[0]=provisional.attitudeCorrection[1]=provisional.attitudeCorrection[2]=0; }
  const auto& fix=gnssRx.latest();
  provisional.gnssAccepted=fix.lastValidFixUs&&ageMs(fix.lastValidFixUs,now)<=kProvisionalGnssStaleMs&&(fix.flags&(gnss::FixValid|gnss::LatitudeValid|gnss::LongitudeValid))==(gnss::FixValid|gnss::LatitudeValid|gnss::LongitudeValid);
  if(provisional.gnssAccepted) { provisional.flags|=ProvisionalGnssAccepted; provisional.latitudeDeg=fix.latitude; provisional.longitudeDeg=fix.longitude; provisional.groundSpeedMps=(fix.flags&gnss::SpeedValid)?fix.speedMps:0; provisional.courseRad=(fix.flags&gnss::CourseValid)?fix.courseDeg*PI/180.0f:0; }
  const bool validTof=tofLatest.receivedUs&&ageMs(tofLatest.receivedUs,now)<=kProvisionalTofStaleMs&&tofLatest.centerMm>0&&tofLatest.centerMm<4000;
  provisional.tofAccepted=validTof&&provisional.attitudeAvailable;
  if(provisional.tofAccepted) { provisional.flags|=ProvisionalTofAccepted; provisional.waterHeightM=(tofLatest.centerMm/1000.0f)*cosf(provisional.rollRad)*cosf(provisional.pitchRad); }
  else provisional.waterHeightM=0;
  emitProvisionalSystem(now);
}

void apiProvisionalSystem() {
  const uint64_t now=nowUs();
  const auto& fix=gnssRx.latest();
  const char* gnssReason=!fix.lastValidFixUs||ageMs(fix.lastValidFixUs,now)>kProvisionalGnssStaleMs?"fix_stale":provisional.gnssAccepted?"accepted":"fix_or_position_invalid";
  const char* tofReason=!tofLatest.receivedUs||ageMs(tofLatest.receivedUs,now)>kProvisionalTofStaleMs?"tof_stale":!provisional.attitudeAvailable?"attitude_unavailable":provisional.tofAccepted?"accepted":"distance_invalid";
  char json[2200];
  snprintf(json,sizeof(json),"{\"provisional\":true,\"control_output_enabled\":false,\"virtual_mode\":\"DISARMED\",\"attitude_available\":%s,\"dual_imu_accepted\":%s,\"dual_imu_reason\":\"%s\",\"gnss_accepted\":%s,\"gnss_reason\":\"%s\",\"tof_accepted\":%s,\"tof_reason\":\"%s\",\"roll_deg\":%.3f,\"pitch_deg\":%.3f,\"yaw_deg\":%.3f,\"roll_rate_rad_s\":%.4f,\"pitch_rate_rad_s\":%.4f,\"yaw_rate_rad_s\":%.4f,\"latitude_deg\":%.8f,\"longitude_deg\":%.8f,\"ground_speed_mps\":%.3f,\"course_deg\":%.3f,\"water_height_m\":%.3f,\"accel_delta_mps2\":%.4f,\"gyro_delta_rad_s\":%.4f,\"primary_age_ms\":%lu,\"secondary_accel_age_ms\":%lu,\"secondary_gyro_age_ms\":%lu,\"gnss_age_ms\":%lu,\"tof_age_ms\":%lu,\"tof_center_mm\":%u,\"virtual_vesc_duty\":0.0,\"virtual_wing_left\":0.0,\"virtual_wing_right\":0.0,\"virtual_wing_center\":0.0,\"flags\":%u}",provisional.attitudeAvailable?"true":"false",provisional.dualAccepted?"true":"false",provisionalDualReason(now),provisional.gnssAccepted?"true":"false",gnssReason,provisional.tofAccepted?"true":"false",tofReason,provisional.rollRad*180.0f/PI,provisional.pitchRad*180.0f/PI,provisional.yawRad*180.0f/PI,provisional.rollRateRadS,provisional.pitchRateRadS,provisional.yawRateRadS,provisional.latitudeDeg,provisional.longitudeDeg,provisional.groundSpeedMps,provisional.courseRad*180.0f/PI,provisional.waterHeightM,isfinite(provisional.accelDeltaMps2)?provisional.accelDeltaMps2:-1.0f,isfinite(provisional.gyroDeltaRadS)?provisional.gyroDeltaRadS:-1.0f,(unsigned long)ageMs(primaryImu.receivedUs,now),(unsigned long)ageMs(bno.accelUs,now),(unsigned long)ageMs(bno.gyroUs,now),(unsigned long)ageMs(fix.lastValidFixUs,now),(unsigned long)ageMs(tofLatest.receivedUs,now),tofLatest.centerMm,provisional.flags);
  web.send(200,"application/json",json);
}

const char provisionalSystemPage[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;white-space:pre-wrap}</style><h2>仮統合システム（影系統）</h2><div class=c id=v>取得中...</div><p class=c>主IMUを基準に副IMUを品質ゲート付きで影融合し、GNSS・ToF・3翼/VESCミキサまで接続しています。機体軸・磁気・ToF取付が未較正のため、全出力は DISARMED / 0 のままです。</p><script>function f(x,n=3){return Number.isFinite(x)?x.toFixed(n):'n/a'}async function u(){try{let j=await(await fetch('/api/provisional-system',{cache:'no-store'})).json();v.textContent=`mode/output: ${j.virtual_mode} / enabled=${j.control_output_enabled}\n\nattitude: ${j.attitude_available}  dual IMU: ${j.dual_imu_accepted} (${j.dual_imu_reason})\nroll/pitch/yaw: ${f(j.roll_deg)} / ${f(j.pitch_deg)} / ${f(j.yaw_deg)} deg\nrates: ${f(j.roll_rate_rad_s,4)} / ${f(j.pitch_rate_rad_s,4)} / ${f(j.yaw_rate_rad_s,4)} rad/s\naccel/gyro difference: ${f(j.accel_delta_mps2,4)} m/s² / ${f(j.gyro_delta_rad_s,4)} rad/s\n\nGNSS: ${j.gnss_accepted} (${j.gnss_reason})\nlat/lon: ${f(j.latitude_deg,8)} / ${f(j.longitude_deg,8)}\nspeed/course: ${f(j.ground_speed_mps)} m/s / ${f(j.course_deg)} deg\nToF height: ${j.tof_accepted} (${j.tof_reason})  ${f(j.water_height_m)} m, centre ${j.tof_center_mm} mm\n\nages primary / secondary accel / gyro / GNSS / ToF [ms]: ${j.primary_age_ms} / ${j.secondary_accel_age_ms} / ${j.secondary_gyro_age_ms} / ${j.gnss_age_ms} / ${j.tof_age_ms}\nvirtual VESC / left / right / centre: ${j.virtual_vesc_duty} / ${j.virtual_wing_left} / ${j.virtual_wing_right} / ${j.virtual_wing_center}`;}catch(e){v.textContent='取得失敗'}}setInterval(u,50);u()</script></html>)HTML";

void apiDualImu() {
  const uint64_t now=nowUs();
  const bool primaryFresh=primaryImu.receivedUs && ageMs(primaryImu.receivedUs,now)<=kDualImuStaleMs;
  const bool secondaryFresh=bno.accelUs && bno.gyroUs && bno.magneticUs && ageMs(bno.accelUs,now)<=kDualImuStaleMs && ageMs(bno.gyroUs,now)<=kDualImuStaleMs && ageMs(bno.magneticUs,now)<=kDualImuStaleMs;
  float accelNorm=NAN,gyroNorm=NAN,magNorm=NAN;
  if(primaryFresh&&secondaryFresh){
    const float ca[3]={kDualImuR00*bno.ax+kDualImuR01*bno.ay+kDualImuR02*bno.az,kDualImuR10*bno.ax+kDualImuR11*bno.ay+kDualImuR12*bno.az,kDualImuR20*bno.ax+kDualImuR21*bno.ay+kDualImuR22*bno.az};
    const float cg[3]={kDualImuR00*bno.gx+kDualImuR01*bno.gy+kDualImuR02*bno.gz,kDualImuR10*bno.gx+kDualImuR11*bno.gy+kDualImuR12*bno.gz,kDualImuR20*bno.gx+kDualImuR21*bno.gy+kDualImuR22*bno.gz};
    const float cm[3]={kDualImuR00*bno.mx+kDualImuR01*bno.my+kDualImuR02*bno.mz,kDualImuR10*bno.mx+kDualImuR11*bno.my+kDualImuR12*bno.mz,kDualImuR20*bno.mx+kDualImuR21*bno.my+kDualImuR22*bno.mz};
    accelNorm=sqrtf(sq(primaryImu.sample.accel[0]-ca[0])+sq(primaryImu.sample.accel[1]-ca[1])+sq(primaryImu.sample.accel[2]-ca[2]));
    gyroNorm=sqrtf(sq(primaryImu.sample.gyro[0]-cg[0])+sq(primaryImu.sample.gyro[1]-cg[1])+sq(primaryImu.sample.gyro[2]-cg[2]));
    magNorm=sqrtf(sq(primaryImu.sample.magnetic[0]-cm[0])+sq(primaryImu.sample.magnetic[1]-cm[1])+sq(primaryImu.sample.magnetic[2]-cm[2]));
  }
  char json[620]; snprintf(json,sizeof(json),"{\"available\":%s,\"provisional\":true,\"comparison_only\":true,\"primary_age_ms\":%lu,\"secondary_accel_age_ms\":%lu,\"secondary_gyro_age_ms\":%lu,\"secondary_magnetic_age_ms\":%lu,\"accel_delta_norm_mps2\":%.4f,\"gyro_delta_norm_rad_s\":%.4f,\"magnetic_delta_norm_ut\":%.4f,\"transform_residual_mean_deg\":4.683,\"transform_residual_max_deg\":6.528}",primaryFresh&&secondaryFresh?"true":"false",(unsigned long)ageMs(primaryImu.receivedUs,now),(unsigned long)ageMs(bno.accelUs,now),(unsigned long)ageMs(bno.gyroUs,now),(unsigned long)ageMs(bno.magneticUs,now),accelNorm,gyroNorm,magNorm); web.send(200,"application/json",json);
}
const char dualImuPage[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;white-space:pre-wrap}</style><h2>DUAL IMU 比較（暫定）</h2><div class=c id=v>取得中...</div><p class=c>RUN0005/0007/0009〜0012由来の主副IMU相対変換による比較専用表示です。機体軸変換・姿勢/Yaw有効化・制御出力には使いません。機体固定後の正式較正で置き換えます。</p><script>async function u(){try{let j=await(await fetch('/api/dual-imu',{cache:'no-store'})).json();v.textContent=`available: ${j.available}\nprovisional / comparison only: ${j.provisional} / ${j.comparison_only}\nprimary / secondary accel / gyro / mag age [ms]: ${j.primary_age_ms} / ${j.secondary_accel_age_ms} / ${j.secondary_gyro_age_ms} / ${j.secondary_magnetic_age_ms}\naccel delta: ${j.accel_delta_norm_mps2.toFixed(4)} m/s²\ngyro delta: ${j.gyro_delta_norm_rad_s.toFixed(4)} rad/s\nmagnetic delta: ${j.magnetic_delta_norm_ut.toFixed(4)} uT\ncalibration residual mean/max: ${j.transform_residual_mean_deg} / ${j.transform_residual_max_deg} deg`}catch(e){v.textContent='取得失敗'}}setInterval(u,50);u()</script></html>)HTML";

const char* estimateHealthName(uint8_t health) {
  return health == (uint8_t)boat::EstimateHealth::Valid ? "VALID" : health == (uint8_t)boat::EstimateHealth::Degraded ? "DEGRADED" : "INVALID";
}

void apiEstimatedState() {
  const uint64_t now = nowUs();
  const boat::EstimatedStatePayload& state = linkLatest.estimatedState;
  const bool available = linkLatest.estimatedStateUs != 0;
  char json[1800];
  snprintf(json, sizeof(json), "{\"available\":%s,\"age_ms\":%lu,\"estimate_time_us\":%llu,\"roll_deg\":%.3f,\"pitch_deg\":%.3f,\"yaw_deg\":%.3f,\"roll_rate_rad_s\":%.4f,\"pitch_rate_rad_s\":%.4f,\"yaw_rate_rad_s\":%.4f,\"qw\":%.6f,\"qx\":%.6f,\"qy\":%.6f,\"qz\":%.6f,\"water_distance_m\":%.3f,\"ground_speed_mps\":%.3f,\"course_deg\":%.3f,\"gyro_age_us\":%lu,\"accel_age_us\":%lu,\"mag_age_us\":%lu,\"gnss_age_us\":%lu,\"tof_age_us\":%lu,\"attitude_health\":\"%s\",\"yaw_health\":\"%s\",\"navigation_health\":\"%s\",\"height_health\":\"%s\",\"accel_correction_active\":%s,\"mag_correction_active\":%s,\"gnss_yaw_correction_active\":%s,\"mount_validated\":%s}", available?"true":"false", (unsigned long)ageMs(linkLatest.estimatedStateUs, now), (unsigned long long)state.estimateUs, state.rollRad*180.0f/PI, state.pitchRad*180.0f/PI, state.yawRad*180.0f/PI, state.rollRateRadS, state.pitchRateRadS, state.yawRateRadS, state.qw, state.qx, state.qy, state.qz, state.waterDistanceM, state.groundSpeedMps, state.courseOverGroundRad*180.0f/PI, (unsigned long)state.gyroAgeUs, (unsigned long)state.accelAgeUs, (unsigned long)state.magAgeUs, (unsigned long)state.gnssAgeUs, (unsigned long)state.tofAgeUs, estimateHealthName(state.attitudeHealth), estimateHealthName(state.yawHealth), estimateHealthName(state.navigationHealth), estimateHealthName(state.heightHealth), (state.flags&boat::EstimateAccelCorrection)?"true":"false", (state.flags&boat::EstimateMagCorrection)?"true":"false", (state.flags&boat::EstimateGnssYawCorrection)?"true":"false", (state.flags&boat::EstimateMountValidated)?"true":"false");
  web.send(200, "application/json", json);
}

const char estimatedStatePage[] PROGMEM = R"HTML(<!doctype html><html lang="ja"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;white-space:pre-wrap}.bad{color:#ffabb0}.warn{color:#ffda83}.ok{color:#91e7b0}</style><h2>制御側 EstimatedState</h2><div class=c id=v>取得中...</div><p class=c>推定器はDRY_RUN専用です。取付座標が未校正の間は姿勢・YawをDEGRADEDとして表示し、アクチュエータ出力には使用しません。</p><script>function f(x,n=3){return Number.isFinite(x)?x.toFixed(n):'n/a'}async function u(){try{let j=await(await fetch('/api/estimated-state',{cache:'no-store'})).json();let c=j.available&&j.age_ms<500?'ok':'bad';v.className='c '+c;v.textContent=`link age: ${j.age_ms} ms\nattitude / yaw / nav / height: ${j.attitude_health} / ${j.yaw_health} / ${j.navigation_health} / ${j.height_health}\nmount validated: ${j.mount_validated}\n\nroll: ${f(j.roll_deg)} deg  (${f(j.roll_rate_rad_s,4)} rad/s)\npitch: ${f(j.pitch_deg)} deg  (${f(j.pitch_rate_rad_s,4)} rad/s)\nyaw: ${f(j.yaw_deg)} deg  (${f(j.yaw_rate_rad_s,4)} rad/s)\nwater distance: ${f(j.water_distance_m)} m\nground speed/course: ${f(j.ground_speed_mps)} m/s / ${f(j.course_deg)} deg\n\ninput age gyro/accel/mag/GNSS/ToF: ${j.gyro_age_us}/${j.accel_age_us}/${j.mag_age_us}/${j.gnss_age_us}/${j.tof_age_us} us\ncorrection accel/mag/GNSS: ${j.accel_correction_active}/${j.mag_correction_active}/${j.gnss_yaw_correction_active}`;}catch(e){v.className='c bad';v.textContent='通信側XIAOから状態を取得できません'}}setInterval(u,50);u()</script></html>)HTML";

const char sensorPage2[] PROGMEM=R"HTML(<!doctype html><html lang=ja><meta name=viewport content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:12px;background:#101720;color:#edf3fa}.c{background:#1d2a38;border-radius:10px;padding:12px;white-space:pre-wrap}</style><h2>BNO08X sensor values</h2><div class=c id=v>Loading...</div><script>async function u(){try{let j=await(await fetch('/api/sensors',{cache:'no-store'})).json();v.textContent=`Acceleration [m/s2]  X ${j.ax.toFixed(3)}  Y ${j.ay.toFixed(3)}  Z ${j.az.toFixed(3)}\nGyroscope [rad/s]  X ${j.gx.toFixed(3)}  Y ${j.gy.toFixed(3)}  Z ${j.gz.toFixed(3)}\nMagnetic field [uT]  X ${j.mx_ut.toFixed(2)}  Y ${j.my_ut.toFixed(2)}  Z ${j.mz_ut.toFixed(2)}\nMagnetic: ${j.magnetic_valid?'valid':'invalid'} / accuracy ${j.magnetic_accuracy}/3 / age ${j.magnetic_age_ms} ms\nBNO: ${j.bno?'ready':'fault'} ${j.fault} / reinitializations ${j.reinits}\n\nINT edges ${j.int_edges} / fallback wakeups ${j.task_fallbacks} / SH-2 services ${j.service_calls}\nCallback events ${j.callback_events} (accel ${j.accel_events}, gyro ${j.gyro_events}, magnetic ${j.magnetic_events})\nDecode errors ${j.decode_errors} / event-queue drops ${j.event_queue_drops}\nEvent queue used/high-water ${j.event_queue_used}/${j.event_queue_high_water} of 96 / max service ${j.max_service_us} us`;}catch(e){v.textContent='Update error'}}setInterval(u,50);u()</script></html>)HTML";
void beginWeb() {
  WiFi.persistent(false); WiFi.mode(WIFI_AP); WiFi.softAP(kApSsid,kApPassword);
  web.on("/",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",simpleBenchmarkPage); }); web.on("/sensors",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",sensorPage2); }); web.on("/state",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",estimatedStatePage); }); web.on("/p1",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",p1Page); }); web.on("/calibration",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",calibrationPage); }); web.on("/dual-imu",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",dualImuPage); }); web.on("/provisional-system",HTTP_GET,[]{ web.send_P(200,"text/html; charset=utf-8",provisionalSystemPage); }); web.on("/api/latest",HTTP_GET,apiLatest); web.on("/api/sensors",HTTP_GET,apiSensors); web.on("/api/dual-imu",HTTP_GET,apiDualImu); web.on("/api/provisional-system",HTTP_GET,apiProvisionalSystem); web.on("/api/estimated-state",HTTP_GET,apiEstimatedState); web.on("/api/p1",HTTP_GET,apiP1); web.on("/api/calibration",HTTP_GET,apiCalibration); web.on("/api/link",HTTP_GET,apiLink); web.on("/api/ui",HTTP_GET,apiUi); web.on("/api/manual",HTTP_GET,apiManual); web.on("/api/benchmark",HTTP_GET,apiBenchmark); web.on("/api/download",HTTP_GET,apiDownload);
  web.on("/api/log/start",HTTP_POST,requestStart); web.on("/api/log/stop",HTTP_POST,requestStop); web.on("/api/p1/start",HTTP_POST,requestP1Start); web.on("/api/p1/stop",HTTP_POST,requestP1Stop); web.on("/api/calibration/start",HTTP_POST,requestCalibrationStart); web.on("/api/calibration/stop",HTTP_POST,requestCalibrationStop);
  web.on("/api/benchmark/start",HTTP_POST,requestBenchmarkStart); web.on("/api/benchmark/stop",HTTP_POST,requestBenchmarkStop);
  web.on("/api/control/stop",HTTP_POST,requestControlStop); web.on("/api/control/estop",HTTP_POST,requestControlEstop); web.begin();
}

void setup() {
  Serial.begin(115200); delay(300); bootResetReason=esp_reset_reason(); commBootId=esp_random();
  controlUart.setRxBufferSize(kControlUartRxBufferBytes); controlUart.begin(kControlUartBaud,SERIAL_8N1,kControlUartRxPin,kControlUartTxPin); controlTxMutex=xSemaphoreCreateMutex();
  gnssUart.setRxBufferSize(kGnssUartRxBufferBytes); gnssRx.begin(gnssUart);
  SPI.begin(kSdSckPin,kSdMisoPin,kSdMosiPin,kSdCsPin); logStats.sdReady=SD.begin(kSdCsPin,SPI);
  recoverP1Journal();
  logMutex=xSemaphoreCreateMutex();
  bnoEventQueue=xQueueCreate(kBnoEventQueueDepth,sizeof(BnoQueuedEvent));
  if (!bnoEventQueue) setBnoFault("BNO event queue allocation failed");
  if (!logMutex || xTaskCreatePinnedToCore(logTask,"LogWriter",4096,nullptr,kLogTaskPriority,&logTaskHandle,1) != pdPASS) {
    logStats.sdReady=false; snprintf(logStats.fault,sizeof(logStats.fault),"log task allocation failed");
  }
  startBno(); xTaskCreatePinnedToCore(controlRxTask,"ControlRx",4096,nullptr,2,nullptr,0); xTaskCreatePinnedToCore(gnssNavTask,"GnssNavTx",4096,nullptr,2,nullptr,1); xTaskCreatePinnedToCore(bnoTask,"CommBno",4096,nullptr,kBnoTaskPriority,&bnoTaskHandle,0); attachInterrupt(digitalPinToInterrupt(kBnoIntPin),bnoIntIsr,FALLING); beginWeb();
  Serial.printf("%s %s boot=%lu reset=%d SD=%d AP=%s URL=http://%s/\n",kFirmwareName,kFirmwareVersion,(unsigned long)commBootId,static_cast<int>(bootResetReason),logStats.sdReady,kApSsid,WiFi.softAPIP().toString().c_str());
}
void loop() {
  serviceGnss(); serviceProvisionalSystem(); serviceTimeSync(); serviceCommands(); serviceBenchmark(); web.handleClient(); serviceBenchmark();
  if (millis()-lastDiagMs >= kDiagnosticIntervalMs) { lastDiagMs=millis(); Serial.printf("SD=%d log=%d rec=%lu q=%u drop=%lu control=%lu GNSS=%d BNO=%d\n",logStats.sdReady,logStats.logging,(unsigned long)logStats.records,queueUsed,(unsigned long)logStats.queueDrops,(unsigned long)logStats.controlFrames,gnssRx.receiving(nowUs()),bno.ready); }
  delay(1);
}
