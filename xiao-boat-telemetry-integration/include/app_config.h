#pragma once

#include <Arduino.h>

namespace app_config {
constexpr char kFirmwareName[] = "xiao-boat-telemetry-integration";
constexpr char kFirmwareVersion[] = "0.1.0-vertical-slice";

// SoftAP/Web UI. Connect directly and open http://192.168.4.1/.
constexpr char kApSsid[] = "XIAO-BOAT-TELEMETRY";
constexpr char kApPassword[] = "12345678";
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kWebRefreshMs = 50UL;

// Communication-side XIAO ESP32S3 Sense wiring.
constexpr int kGnssRxPin = D0;
constexpr int kGnssTxPin = D1;
constexpr int kBnoRstPin = D2;
constexpr int kBnoIntPin = D3;
constexpr int kBnoSdaPin = D4;
constexpr int kBnoSclPin = D5;
constexpr int kControlUartRxPin = D7;
constexpr int kControlUartTxPin = D6;
constexpr int kSdCsPin = 21;
constexpr int kSdSckPin = D8;
constexpr int kSdMisoPin = D9;
constexpr int kSdMosiPin = D10;

constexpr uint32_t kControlUartBaud = 921600UL;
constexpr uint16_t kControlUartRxBufferBytes = 16384;
constexpr uint16_t kFrameQueueDepth = 160;
constexpr size_t kSdWriteBufferBytes = 8192;
constexpr uint32_t kSdFlushIntervalMs = 100UL;
constexpr char kLogDirectory[] = "/BOATLOG";

constexpr uint8_t kBnoAddressPrimary = 0x4A;
constexpr uint8_t kBnoAddressAlternate = 0x4B;
constexpr uint32_t kBnoI2cHz = 100000UL;
constexpr uint32_t kAccelGyroIntervalUs = 5000UL;
constexpr uint32_t kRotationIntervalUs = 20000UL;
constexpr uint32_t kBnoNoDataTimeoutMs = 3000UL;
constexpr uint32_t kBnoReinitIntervalMs = 2000UL;

constexpr uint32_t kGnssBaud = 115200UL;
constexpr uint16_t kGnssUartRxBufferBytes = 2048;
constexpr uint16_t kGnssReadBudgetBytes = 512;
constexpr uint16_t kGnssInputLineChars = 127;
constexpr uint16_t kGnssMaxSentenceChars = 110;
constexpr uint32_t kGnssSentenceTimeoutMs = 500UL;
constexpr uint32_t kGnssNoDataTimeoutMs = 3000UL;
constexpr uint32_t kGnssStatusIntervalMs = 1000UL;

constexpr uint32_t kControlHeartbeatIntervalMs = 250UL;
constexpr uint32_t kDiagnosticIntervalMs = 1000UL;
}  // namespace app_config
