#pragma once
#include <Arduino.h>
namespace app_config {
constexpr char kFirmwareName[]="xiao-boat-control-integration";
constexpr char kFirmwareVersion[]="0.2.1-heartbeat-task";
constexpr int kPeripheralSdaPin=D1,kPeripheralSclPin=D0;
constexpr int kBnoRstPin=D2,kBnoIntPin=D3,kBnoSdaPin=D4,kBnoSclPin=D5;
constexpr int kLinkRxPin=D6,kLinkTxPin=D7,kVescRxPin=D8,kVescTxPin=D9,kFuturePcaOePin=D10;
constexpr uint8_t kBnoAddress=0x4A,kBnoAlternateAddress=0x4B,kTofAddress=0x29,kInaAddress=0x44,kPcaAddress=0x40;
constexpr uint32_t kBnoI2cHz=100000UL,kPeripheralI2cHz=400000UL,kAccelGyroIntervalUs=5000UL,kRotationIntervalUs=20000UL;
constexpr uint32_t kTofFrequencyHz=10UL,kInaSampleUs=20000UL,kServoControlUs=20000UL;
constexpr uint32_t kOscillatorHz=25000000UL; constexpr float kServoPwmHz=50.0f; constexpr uint8_t kServoChannel=0;
constexpr uint16_t kHardMinUs=500,kHardMaxUs=2500,kServoCenterUs=1500,kServoIntegrationMinUs=1400,kServoIntegrationMaxUs=1600,kServoSlewUsPerUpdate=5;
constexpr float kVescMaxDuty=0.03f,kVescTestDuty=0.03f; constexpr uint32_t kVescUartBaud=115200UL,kVescRequestIntervalMs=20UL,kVescFrameTimeoutMs=100UL,kVescMaxPayloadBytes=512,kVescKeepaliveMs=50UL,kMaxTestMs=300000UL;
constexpr uint32_t kLinkBaud=921600UL,kProtocolVersion=1,kLinkHeartbeatTimeoutMs=500UL; constexpr bool kRequireHostHeartbeat=false;
constexpr bool kDryRunActuators=true; constexpr uint32_t kGnssNavExpectedIntervalMs=100UL,kControlHeartbeatIntervalMs=100UL,kLinkFailSafeTimeoutMs=500UL;
// The control node has no radio role. Integration status is served by the communication node.
constexpr bool kEnableTemporaryDebugWifi=false; constexpr char kDebugApSsid[]="XIAO-BOAT-DEBUG",kDebugApPass[]="12345678"; constexpr uint16_t kDebugHttpPort=80;
constexpr uint16_t kInaConfig=0x08DF,kInaCalibration=0x0800; constexpr uint16_t kConfig=kInaConfig,kCalibration=kInaCalibration; constexpr float kShuntOhm=0.002f,kCurrentLsbA=0.00125f,kPowerLsbW=0.03125f;
constexpr bool kEnableOverCurrentTrip=false,kEnableLowVoltageTrip=false; constexpr float kOverCurrentTripA=30.0f,kLowVoltageTripV=6.0f;
constexpr uint32_t kDiagnosticIntervalMs=1000UL,kBnoNoDataTimeoutMs=3000UL,kReinitIntervalMs=2000UL;
constexpr uint16_t kLinkMaxPayload=768,kLinkTxQueueDepth=64;
}
namespace cfg=app_config;
