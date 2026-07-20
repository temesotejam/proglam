#pragma once
#include <Arduino.h>
namespace app_config {
constexpr char kFirmwareName[] = "xiao-vesc-duty-5min-run-test";
constexpr char kFirmwareVersion[] = "0.4.0-stage3-duty-5min";
constexpr char kBoardName[] = "Seeed Studio XIAO ESP32S3 Sense";
constexpr uint8_t kActiveStage = 3;
// Stage 3: fixed positive duty, bounded five-minute run. Change only after review.
constexpr float kPulseDuty = 0.03f;
constexpr uint32_t kPulseDurationMs = 300000UL;
constexpr uint32_t kDutyKeepaliveMs = 50UL;
constexpr uint32_t kValuesTimeoutMsDuringRun = 500UL;
constexpr uint32_t kArmWindowMs = 10000UL;
constexpr int kVescUartTxPin = D6;
constexpr int kVescUartRxPin = D7;
constexpr uint32_t kVescUartBaud = 115200UL;
constexpr uint32_t kVescResponseTimeoutMs = 100UL;
constexpr uint32_t kVescRequestIntervalMs = 20UL;
constexpr uint32_t kVescFrameTimeoutMs = 100UL;
constexpr uint32_t kFwRetryIntervalMs = 1000UL;
constexpr uint8_t kFwMaxRequests = 5;
constexpr size_t kVescMaxPayloadBytes = 512;
constexpr int kSdCsPin = 21, kSdSckPin = 7, kSdMisoPin = 8, kSdMosiPin = 9;
constexpr char kLogDirectory[] = "/VESCLOG";
constexpr uint16_t kLogFormatVersion = 1, kHeaderBytes = 256, kRecordBytes = 160, kPayloadBytes = 118, kLogQueueCapacity = 512, kSdWriteBlockBytes = 4096;
constexpr uint32_t kSdFlushIntervalMs = 2000UL;
constexpr char kApSsid[] = "XIAO-VESC-LOGGER", kApPassword[] = "12345678";
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kWebRefreshMs = 250UL, kSystemIntervalMs = 1000UL, kSerialStatusIntervalMs = 1000UL;
}