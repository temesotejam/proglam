#pragma once
#include <Arduino.h>
namespace app_config {
constexpr char kFirmwareName[] = "xiao-bno08x-sd-gnss-logger";
constexpr char kFirmwareVersion[] = "0.5.0";
constexpr char kBoardName[] = "Seeed Studio XIAO ESP32S3 Sense";
// RUN0010 BNO baseline: do not change in this GNSS addition.
constexpr int kBnoRstPin=D2, kBnoIntPin=D3, kI2cSdaPin=D4, kI2cSclPin=D5;
constexpr uint8_t kAddressPrimary=0x4A, kAddressAlternate=0x4B;
constexpr uint32_t kI2cClock100kHz=100000UL, kI2cClock400kHz=400000UL;
constexpr uint32_t kI2cClockHz=kI2cClock100kHz;
constexpr char kTestSettingName[]="RUN0010_BASELINE_100K";
constexpr uint32_t kAccelGyroReportIntervalUs=5000UL, kRotationVectorReportIntervalUs=20000UL;
constexpr bool kUseHardwareReset=false;
constexpr uint32_t kResetLowMs=10UL, kResetBootWaitMs=300UL, kNoDataTimeoutMs=3000UL, kReinitRetryIntervalMs=2000UL, kStatusPrintIntervalMs=1000UL;
constexpr int kSdCsPin=21, kSdSckPin=7, kSdMisoPin=8, kSdMosiPin=9;
// GT-505GGBL5-DR-N: manufacturer datasheet default UART rate is 115200 bps.
constexpr int kGnssRxPin=D0, kGnssTxPin=D1;
constexpr uint32_t kGnssBaud=115200UL; // Change here only if the receiver was reconfigured.
constexpr uint16_t kGnssUartRxBufferBytes=2048, kGnssReadBudgetBytes=512;
constexpr uint16_t kGnssMaxSentenceChars=110, kGnssInputLineChars=127;
constexpr uint32_t kGnssSentenceTimeoutMs=500UL, kGnssNoDataTimeoutMs=3000UL, kGnssStatusIntervalMs=1000UL;
constexpr uint16_t kLogFormatVersion=3, kLogHeaderBytes=256, kLogRecordBytes=160;
constexpr uint16_t kLogQueueCapacity=512, kSdWriteBlockBytes=4096;
constexpr uint32_t kSdFlushIntervalMs=2000UL, kSystemRecordIntervalMs=1000UL;
constexpr char kLogDirectory[]="/BNOLOG";
constexpr char kApSsid[]="XIAO-BNO08X-SDLOG", kApPassword[]="12345678";
constexpr uint16_t kHttpPort=80;
constexpr uint32_t kWebRefreshMs=500UL;
}