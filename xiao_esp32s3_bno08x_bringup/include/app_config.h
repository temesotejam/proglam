#pragma once

#include <Arduino.h>

namespace app_config {

constexpr char kFirmwareName[] = "xiao-esp32s3-bno08x-bringup";
constexpr char kFirmwareVersion[] = "0.1.0";
// Standalone Web UI: join this WPA2 SoftAP, then open http://192.168.4.1/.
constexpr char kWebApSsid[] = "XIAO-BNO08X";
constexpr char kWebApPassword[] = "bno08x-test";
constexpr uint8_t kWebApChannel = 1;
constexpr uint16_t kWebHttpPort = 80;
constexpr uint32_t kWebRefreshMs = 20UL;

// Seeed Studio XIAO ESP32S3 Arduino pin aliases. No board-axis remap is applied.
constexpr int kI2cSdaPin = D3;
constexpr int kI2cSclPin = D2;
constexpr int kBnoIntPin = D1;
constexpr int kBnoRstPin = D0;
constexpr uint32_t kI2cClockHz = 400000UL;
constexpr uint8_t kBnoAddressPrimary = 0x4A;
constexpr uint8_t kBnoAddressAlternate = 0x4B;

constexpr uint32_t kAccelReportIntervalUs = 5000UL;       // 200 Hz
constexpr uint32_t kGyroReportIntervalUs = 5000UL;        // 200 Hz
constexpr uint32_t kRotationReportIntervalUs = 20000UL;   // 50 Hz
constexpr uint32_t kCsvIntervalUs = 20000UL;              // <= 50 Hz
constexpr uint32_t kDiagnosticIntervalUs = 1000000UL;

constexpr uint32_t kResetLowMs = 10UL;
constexpr uint32_t kResetBootWaitMs = 300UL;
constexpr uint32_t kInitialDataTimeoutUs = 2000000UL;
constexpr uint32_t kStreamTimeoutUs = 500000UL;
constexpr uint32_t kReinitRetryIntervalUs = 3000000UL;
constexpr uint8_t kMaxReinitAttempts = 3;
constexpr uint8_t kMaxEventsPerLoop = 12;

}  // namespace app_config
