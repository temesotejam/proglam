#pragma once
#include <Arduino.h>
namespace app_config {
constexpr char kFirmwareName[]="xiao-bno08x-vl53l5cx-sd-logger";
constexpr char kFirmwareVersion[]="0.2.0-stage2-bno-tof";
constexpr char kBoardName[]="Seeed Studio XIAO ESP32S3 Sense";
constexpr int kBnoSdaPin=D4,kBnoSclPin=D5,kBnoIntPin=D3,kBnoRstPin=D2;
constexpr uint8_t kBnoAddress=0x4A,kBnoAlternateAddress=0x4B;
constexpr uint32_t kBnoI2cHz=100000UL,kAccelGyroIntervalUs=5000UL,kRotationIntervalUs=20000UL;
constexpr int kTofSdaPin=D1,kTofSclPin=D0; constexpr uint8_t kTofAddress=0x29; constexpr uint32_t kTofI2cHz=400000UL,kTofFrequencyHz=10UL,kTofBootWaitMs=2000UL;
constexpr int kSdCsPin=21,kSdSckPin=7,kSdMisoPin=8,kSdMosiPin=9;
constexpr char kLogDirectory[]="/BNO_TOF"; constexpr uint16_t kLogFormatVersion=1,kHeaderBytes=256,kRecordBytes=160,kPayloadBytes=118,kLogQueueCapacity=512,kSdWriteBlockBytes=4096;
constexpr uint32_t kSdFlushIntervalMs=2000UL,kSystemIntervalMs=1000UL,kStatusIntervalMs=1000UL,kNoDataTimeoutMs=3000UL,kReinitIntervalMs=2000UL;
constexpr char kApSsid[]="XIAO-BNO-TOF",kApPassword[]="12345678"; constexpr uint16_t kHttpPort=80; constexpr uint32_t kWebRefreshMs=250UL;
}