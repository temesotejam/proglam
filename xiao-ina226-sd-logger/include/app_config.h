#pragma once
#include <Arduino.h>
namespace cfg {
constexpr char kName[]="xiao-ina226-sd-logger"; constexpr char kFirmwareName[]="xiao-ina226-sd-logger"; constexpr char kFirmwareVersion[]="0.1.0-ina226-complete"; constexpr uint8_t kActiveStage=1;
constexpr char kVersion[]="0.1.0-ina226-complete";
constexpr int kSda=D1,kScl=D0,kSdCs=21,kSdSck=7,kSdMiso=8,kSdMosi=9; constexpr int kSdCsPin=kSdCs,kSdSckPin=kSdSck,kSdMisoPin=kSdMiso,kSdMosiPin=kSdMosi;
constexpr uint8_t kAddr=0x44;
constexpr uint32_t kI2cHz=100000,kSampleHz=50,kSampleUs=1000000UL/kSampleHz;
constexpr float kShuntOhm=0.002f,kCurrentLsbA=0.00125f,kPowerLsbW=0.03125f;
constexpr uint16_t kCalibration=0x0800,kConfig=0x08DF; // AVG=16, VBUS/VSH=588us, continuous
constexpr char kApSsid[]="XIAO-INA226-LOGGER",kApPass[]="12345678";
constexpr uint32_t kWebMs=250,kTimeoutMs=500,kReinitMs=2000;
constexpr char kLogDir[]="/INALOG"; constexpr char kLogDirectory[]="/INALOG";
constexpr uint16_t kLogFormatVersion=1,kHeaderBytes=256,kRecordBytes=160,kPayloadBytes=118,kLogQueueCapacity=512,kSdWriteBlockBytes=4096;
constexpr uint32_t kSdFlushIntervalMs=2000UL;
}