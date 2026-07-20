#pragma once
#include <Arduino.h>
namespace cfg {
constexpr char kFirmwareName[]="xiao-pca9685-servo-sd-controller";
constexpr char kFirmwareVersion[]="0.1.2-endpoint-sweep-test";
constexpr uint8_t kActiveStage=1;
constexpr int kSda=D1,kScl=D0,kOe=D2;
constexpr uint8_t kPcaAddress=0x40;
constexpr uint32_t kI2cHz=400000;
constexpr uint32_t kOscillatorHz=25000000UL;
constexpr float kServoPwmHz=50.0f;
constexpr uint8_t kServoChannel=0;
constexpr bool kUseHardwareOe=true;
constexpr uint16_t kHardMinUs=500,kTestMinUs=500,kCenterUs=1500,kTestMaxUs=2500,kHardMaxUs=2500;
// 50 Hz control: 100000 us/s yields a 2000 us step, covering the complete
// 500--2500 us permitted range in one control update (nominally <= 20 ms).
constexpr uint32_t kSlewUsPerSecond=100000UL;
constexpr uint32_t kControlHz=50,kControlUs=1000000UL/kControlHz;
constexpr uint32_t kEndpointSweepDurationMs=10000UL,kEndpointSweepHalfPeriodMs=500UL;
constexpr uint32_t kHeartbeatIntervalMs=500,kHeartbeatTimeoutMs=1000;
constexpr char kApSsid[]="XIAO-PCA9685-SERVO",kApPass[]="12345678";
constexpr uint32_t kWebMs=250;
constexpr int kSdCs=21,kSdSck=7,kSdMiso=8,kSdMosi=9;
constexpr int kSdCsPin=kSdCs,kSdSckPin=kSdSck,kSdMisoPin=kSdMiso,kSdMosiPin=kSdMosi;
constexpr char kLogDirectory[]="/PCALOG";
constexpr uint16_t kLogFormatVersion=1,kHeaderBytes=256,kRecordBytes=160,kPayloadBytes=118,kLogQueueCapacity=512,kSdWriteBlockBytes=4096;
constexpr uint32_t kSdFlushIntervalMs=2000UL;
}