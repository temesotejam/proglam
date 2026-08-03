#pragma once
#include <Arduino.h>
namespace app_config {
constexpr char kFirmwareName[] = "cores3-telemetry-bridge";
constexpr char kFirmwareVersion[] = "0.1.0-temporary-comm-replacement";
// CoreS3 Port C: RX=GPIO18, TX=GPIO17.  GNSS TX must go to GPIO18.
constexpr int kGnssRxPin = 18, kGnssTxPin = 17;
// CoreS3 Port B: GPIO8 is receiver and GPIO9 is transmitter for the control XIAO link.
constexpr int kControlUartRxPin = 8, kControlUartTxPin = 9;
constexpr uint32_t kGnssBaud = 115200UL, kControlUartBaud = 921600UL;
constexpr uint16_t kGnssUartRxBufferBytes = 2048, kGnssReadBudgetBytes = 512;
constexpr uint16_t kGnssInputLineChars = 127, kGnssMaxSentenceChars = 110;
constexpr uint32_t kGnssSentenceTimeoutMs = 500UL, kGnssNoDataTimeoutMs = 3000UL;
constexpr uint16_t kFrameQueueDepth = 96;
constexpr size_t kSdWriteBufferBytes = 8192, kSdWriteChunkBytes = 512;
constexpr int kSdCsPin = 4, kSdSckPin = 36, kSdMisoPin = 35, kSdMosiPin = 37;
constexpr char kLogDirectory[] = "/BOATLOG";
constexpr uint32_t kGnssNavIntervalMs = 100UL, kHeartbeatIntervalMs = 100UL;
constexpr char kApSsid[] = "BOAT-ESKF-CORE", kApPassword[] = "12345678";
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kWebRefreshMs = 50UL, kTimeSyncIntervalMs = 1000UL;
}