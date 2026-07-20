#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <boat_protocol.h>

// Communication XIAO: connector wiring is intentionally D7 RX / D6 TX.
constexpr int kRx = D7, kTx = D6, kCs = 21, kSck = D8, kMiso = D9, kMosi = D10;
constexpr uint32_t kBaud = 921600;
constexpr size_t kUartRxBufferBytes = 16384;
constexpr uint16_t kFrameQueueDepth = 160;
constexpr size_t kSdWriteBufferBytes = 8192;
constexpr uint32_t kSdFlushIntervalMs = 100;
constexpr char kSsid[] = "XIAO-BOAT-LOGGER", kPass[] = "12345678";

HardwareSerial telemetryUart(1);
WebServer web(80);
File logFile;
boat::Decoder decoder;

boat::Frame frameQueue[kFrameQueueDepth];
uint16_t qHead = 0, qTail = 0, qUsed = 0, qHighWater = 0;
portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

uint8_t sdWriteBuffer[kSdWriteBufferBytes];
size_t sdWriteUsed = 0;
uint32_t lastSdFlushMs = 0;

volatile uint32_t drops = 0, records = 0, bytesRx = 0, lastRxMs = 0;
volatile uint32_t sdWriteErrors = 0;
volatile bool logging = false;
uint32_t sessionRecords = 0;
bool sdOk = false;
char runName[24] = "none";

bool queuePush(const boat::Frame &frame) {
  bool accepted = false;
  portENTER_CRITICAL(&queueMux);
  if (logging && qUsed < kFrameQueueDepth) {
    frameQueue[qHead] = frame;
    qHead = (qHead + 1) % kFrameQueueDepth;
    ++qUsed;
    if (qUsed > qHighWater) qHighWater = qUsed;
    accepted = true;
  } else if (logging) {
    ++drops;
  }
  portEXIT_CRITICAL(&queueMux);
  return accepted;
}

bool queuePop(boat::Frame &frame) {
  bool available = false;
  portENTER_CRITICAL(&queueMux);
  if (qUsed) {
    frame = frameQueue[qTail];
    qTail = (qTail + 1) % kFrameQueueDepth;
    --qUsed;
    available = true;
  }
  portEXIT_CRITICAL(&queueMux);
  return available;
}

uint16_t queueDepth() {
  portENTER_CRITICAL(&queueMux);
  const uint16_t depth = qUsed;
  portEXIT_CRITICAL(&queueMux);
  return depth;
}

uint16_t queueHighWater() {
  portENTER_CRITICAL(&queueMux);
  const uint16_t highWater = qHighWater;
  portEXIT_CRITICAL(&queueMux);
  return highWater;
}

void clearQueue() {
  portENTER_CRITICAL(&queueMux);
  qHead = qTail = qUsed = qHighWater = 0;
  portEXIT_CRITICAL(&queueMux);
}

void flushSdBuffer() {
  if (!sdWriteUsed || !logFile) return;
  const size_t written = logFile.write(sdWriteBuffer, sdWriteUsed);
  if (written != sdWriteUsed) ++sdWriteErrors;
  sdWriteUsed = 0;
  lastSdFlushMs = millis();
}

void appendRecord(const boat::Frame &frame) {
  const size_t recordBytes = 4 + 8 + sizeof(frame.header) + frame.header.length;
  if (recordBytes > kSdWriteBufferBytes) {
    ++sdWriteErrors;
    return;
  }
  if (sdWriteUsed + recordBytes > kSdWriteBufferBytes) flushSdBuffer();

  const uint32_t magic = 0x424C4F47;
  const uint64_t rxUs = esp_timer_get_time();
  memcpy(sdWriteBuffer + sdWriteUsed, &magic, sizeof(magic));
  sdWriteUsed += sizeof(magic);
  memcpy(sdWriteBuffer + sdWriteUsed, &rxUs, sizeof(rxUs));
  sdWriteUsed += sizeof(rxUs);
  memcpy(sdWriteBuffer + sdWriteUsed, &frame.header, sizeof(frame.header));
  sdWriteUsed += sizeof(frame.header);
  memcpy(sdWriteBuffer + sdWriteUsed, frame.payload, frame.header.length);
  sdWriteUsed += frame.header.length;
  ++records;
  ++sessionRecords;
}

void serviceLog() {
  if (!sdOk || !logging) return;
  boat::Frame frame;
  uint8_t drained = 0;
  while (drained < 32 && queuePop(frame)) {
    appendRecord(frame);
    ++drained;
  }
  if (sdWriteUsed && (sdWriteUsed >= (kSdWriteBufferBytes / 2) ||
      static_cast<uint32_t>(millis() - lastSdFlushMs) >= kSdFlushIntervalMs)) {
    flushSdBuffer();
  }
}

// I2C/SD/Web must never block this task. It only drains UART and decodes frames.
void telemetryRxTask(void *) {
  for (;;) {
    bool readAny = false;
    while (telemetryUart.available()) {
      const int read = telemetryUart.read();
      if (read < 0) break;
      readAny = true;
      ++bytesRx;
      lastRxMs = millis();
      boat::Frame frame;
      if (decoder.feed(static_cast<uint8_t>(read), frame) && logging) queuePush(frame);
    }
    if (!readAny) vTaskDelay(pdMS_TO_TICKS(1));
    else taskYIELD();
  }
}

void api() {
  char response[700];
  snprintf(response, sizeof(response),
           "{\"sd\":%s,\"logging\":%s,\"run\":\"%s\",\"records\":%lu,\"bytes_rx\":%lu,\"age_ms\":%lu,\"queue\":%u,\"queue_high_water\":%u,\"drops\":%lu,\"sd_write_errors\":%lu,\"crc_errors\":%lu,\"cobs_errors\":%lu,\"length_errors\":%lu}",
           sdOk ? "true" : "false", logging ? "true" : "false", runName,
           static_cast<unsigned long>(sessionRecords), static_cast<unsigned long>(bytesRx),
           static_cast<unsigned long>(millis() - lastRxMs), queueDepth(), queueHighWater(),
           static_cast<unsigned long>(drops), static_cast<unsigned long>(sdWriteErrors),
           static_cast<unsigned long>(decoder.crcErrors), static_cast<unsigned long>(decoder.cobsErrors),
           static_cast<unsigned long>(decoder.lengthErrors));
  web.send(200, "application/json", response);
}

void startLog() {
  if (!sdOk || logging) {
    web.send(409, "application/json", "{\"accepted\":false}");
    return;
  }
  for (uint16_t i = 1; i < 10000; ++i) {
    snprintf(runName, sizeof(runName), "/BOATLOG/RUN%04u.BIN", i);
    if (!SD.exists(runName)) {
      logFile = SD.open(runName, FILE_WRITE);
      break;
    }
  }
  sessionRecords = 0;
  drops = 0;
  sdWriteErrors = 0;
  clearQueue();
  sdWriteUsed = 0;
  lastSdFlushMs = millis();
  logging = static_cast<bool>(logFile);
  web.send(logging ? 202 : 500, "application/json", logging ? "{\"accepted\":true}" : "{\"accepted\":false}");
}

void stopLog() {
  logging = false;
  boat::Frame frame;
  while (queuePop(frame)) appendRecord(frame);
  flushSdBuffer();
  if (logFile) {
    logFile.flush();
    logFile.close();
  }
  clearQueue();
  web.send(200, "application/json", "{\"accepted\":true}");
}

const char page[] = R"HTML(<!doctype html><meta name=viewport content="width=device-width"><style>body{font:16px system-ui;background:#101820;color:#eef}.c{padding:12px;margin:10px;background:#1d2935;white-space:pre-wrap}button{padding:9px;margin:3px}</style><h2 class=c>Boat telemetry SD logger</h2><div class=c><button onclick="post('/api/log/start')">Start log</button><button onclick="post('/api/log/stop')">Stop log</button></div><div id=s class=c>loading</div><script>async function post(x){await fetch(x,{method:'POST'});setTimeout(u,100)}async function u(){let j=await(await fetch('/api/status')).json();s.textContent=`SD ${j.sd}, logging ${j.logging}, file ${j.run}
records ${j.records}, UART bytes ${j.bytes_rx}
age ${j.age_ms} ms, queue ${j.queue}, high-water ${j.queue_high_water}, drops ${j.drops}
SD write/CRC/COBS/length errors ${j.sd_write_errors}/${j.crc_errors}/${j.cobs_errors}/${j.length_errors}`}setInterval(u,1000);u()</script>)HTML";

void setup() {
  Serial.begin(115200);
  telemetryUart.setRxBufferSize(kUartRxBufferBytes);
  telemetryUart.begin(kBaud, SERIAL_8N1, kRx, kTx);
  SPI.begin(kSck, kMiso, kMosi, kCs);
  sdOk = SD.begin(kCs, SPI);
  if (sdOk) SD.mkdir("/BOATLOG");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kSsid, kPass);
  web.on("/", HTTP_GET, [] { web.send(200, "text/html", page); });
  web.on("/api/status", HTTP_GET, api);
  web.on("/api/log/start", HTTP_POST, startLog);
  web.on("/api/log/stop", HTTP_POST, stopLog);
  web.begin();
  xTaskCreatePinnedToCore(telemetryRxTask, "telemetryRx", 4096, nullptr, 3, nullptr, 0);
  Serial.printf("logger SD=%d AP=%s IP=%s UART=%lu RXbuf=%u queue=%u\n", sdOk, kSsid,
                WiFi.softAPIP().toString().c_str(), static_cast<unsigned long>(kBaud),
                static_cast<unsigned>(kUartRxBufferBytes), static_cast<unsigned>(kFrameQueueDepth));
}

void loop() {
  serviceLog();
  web.handleClient();
  delay(1);
}
