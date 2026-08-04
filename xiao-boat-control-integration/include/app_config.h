#pragma once
#include <Arduino.h>
#include "experiment_config.h"

// Proposal evaluation is an opt-in compile-time layer. The defaults are
// inactive so BOAT_EXPERIMENT=23 keeps its existing behavior.
#ifndef BENCHMARK_ENABLE
#define BENCHMARK_ENABLE 0
#endif
#ifndef REPLAY_ENABLE
#define REPLAY_ENABLE 0
#endif
#ifndef SHADOW_CONTROL_ENABLE
#define SHADOW_CONTROL_ENABLE 0
#endif
#ifndef ACTUATOR_OUTPUT_ENABLE
#define ACTUATOR_OUTPUT_ENABLE 0
#endif
#ifndef PROPOSAL_PROFILE
#define PROPOSAL_PROFILE 0
#endif
static_assert(!BENCHMARK_ENABLE || !ACTUATOR_OUTPUT_ENABLE,
              "proposal benchmark must never enable actuator output");
static_assert(!REPLAY_ENABLE || !ACTUATOR_OUTPUT_ENABLE,
              "proposal replay must never enable actuator output");
namespace app_config {
constexpr char kFirmwareName[]="xiao-boat-control-integration";
constexpr char kFirmwareVersion[]="0.3.5-estimated-state-dry-run";
constexpr int kPeripheralSdaPin=D1,kPeripheralSclPin=D0;
constexpr int kBnoRstPin=D2,kBnoIntPin=D3,kBnoSdaPin=D4,kBnoSclPin=D5;
constexpr int kLinkRxPin=D6,kLinkTxPin=D7,kVescRxPin=D8,kVescTxPin=D9,kFuturePcaOePin=D10;
constexpr uint8_t kBnoAddress=0x4A,kBnoAlternateAddress=0x4B,kTofAddress=0x29,kInaAddress=0x44,kPcaAddress=0x40;
constexpr uint32_t kBnoI2cHz=100000UL,kPeripheralI2cHz=experiment_config::kPeripheralI2cHz,kAccelGyroIntervalUs=20000UL,kRotationIntervalUs=20000UL,kMagneticIntervalUs=50000UL;
constexpr uint16_t kBnoEventQueueDepth=96; constexpr uint8_t kBnoServiceCallBudget=8;
// The TX drain must preempt active-INT BNO servicing on the same core.
constexpr uint32_t kBnoTaskFallbackMs=2UL; constexpr UBaseType_t kBnoTaskPriority=3,kLinkTxTaskPriority=4;
constexpr uint32_t kTofFrequencyHz=10UL,kInaSampleUs=20000UL,kServoControlUs=20000UL;
constexpr uint32_t kOscillatorHz=25000000UL; constexpr float kServoPwmHz=50.0f; constexpr uint8_t kServoChannel=0;
constexpr uint16_t kHardMinUs=500,kHardMaxUs=2500,kServoCenterUs=1500,kServoIntegrationMinUs=1400,kServoIntegrationMaxUs=1600,kServoSlewUsPerUpdate=5;
constexpr float kVescMaxDuty=0.03f,kVescTestDuty=0.03f; constexpr uint32_t kVescUartBaud=115200UL,kVescRequestIntervalMs=20UL,kVescFrameTimeoutMs=100UL,kVescMaxPayloadBytes=512,kVescKeepaliveMs=50UL,kMaxTestMs=300000UL;
constexpr uint32_t kLinkBaud=921600UL,kProtocolVersion=1,kLinkHeartbeatTimeoutMs=500UL; constexpr bool kRequireHostHeartbeat=false;
constexpr bool kDryRunActuators=true; constexpr uint32_t kGnssNavExpectedIntervalMs=100UL,kControlHeartbeatIntervalMs=100UL,kLinkFailSafeTimeoutMs=500UL;
// ESKF shadow-run configuration. It has no path to PWM, VESC, arming, or navigation control.
constexpr bool kShadowOnly=true,kActuatorOutputEnabled=false,kEnableIna226=false;
constexpr bool kBenchmarkEnable=BENCHMARK_ENABLE!=0,kReplayEnable=REPLAY_ENABLE!=0;
constexpr bool kShadowControlEnable=SHADOW_CONTROL_ENABLE!=0;
constexpr bool kActuatorOutputCompileEnable=ACTUATOR_OUTPUT_ENABLE!=0;
constexpr uint8_t kProposalProfile=(uint8_t)PROPOSAL_PROFILE;
// Future code must use this guard before any PCA9685/VESC write.
constexpr bool kProposalActuatorPathEnabled = kActuatorOutputEnabled &&
                                               kActuatorOutputCompileEnable &&
                                               !kBenchmarkEnable && !kReplayEnable;
constexpr bool kPrimaryBnoEnabled=true,kSecondaryBnoEnabled=false;
constexpr uint32_t kEskfStateIntervalMs=50UL,kEskfHealthIntervalMs=200UL,kEskfAlignmentUs=2000000UL,kEskfCheckpointIntervalUs=50000UL,kEskfImuStaleUs=50000UL;
constexpr float kEskfGyroNoise=0.020f,kEskfAccelNoise=0.35f,kEskfGyroBiasRw=0.0005f,kEskfAccelBiasRw=0.010f;
constexpr float kEskfGnssPositionNoiseM=3.0f,kEskfGnssVelocityNoiseMps=0.8f,kEskfTofNoiseM=0.15f;
constexpr float kEskfGnssNisGate=13.3f,kEskfTofNisGate=6.63f,kEskfMinCourseSpeedMps=1.0f;
constexpr float kEskfTofMinM=0.20f,kEskfTofMaxM=4.0f,kEskfTofMaxSpreadM=0.25f,kEskfTofMaxTiltRad=0.52f;
constexpr float kEskfWaterPlaneDownM=0.0f,kEskfTofOffsetM=0.0f;
constexpr float kEskfTofPositionBodyM[3]={0,0,0},kEskfTofBeamBody[3]={0,0,1};
constexpr bool kEskfGnssAltitudeEnabled=false,kEskfCourseYawEnabled=false,kEskfMountValid=false;
// The control node has no radio role. Integration status is served by the communication node.
constexpr bool kEnableTemporaryDebugWifi=false; constexpr char kDebugApSsid[]="XIAO-BOAT-DEBUG",kDebugApPass[]="12345678"; constexpr uint16_t kDebugHttpPort=80;
constexpr uint16_t kInaConfig=0x08DF,kInaCalibration=0x0800; constexpr uint16_t kConfig=kInaConfig,kCalibration=kInaCalibration; constexpr float kShuntOhm=0.002f,kCurrentLsbA=0.00125f,kPowerLsbW=0.03125f;
constexpr bool kEnableOverCurrentTrip=false,kEnableLowVoltageTrip=false; constexpr float kOverCurrentTripA=30.0f,kLowVoltageTripV=6.0f;
constexpr uint32_t kDiagnosticIntervalMs=1000UL,kBnoNoDataTimeoutMs=3000UL,kReinitIntervalMs=2000UL;
constexpr uint16_t kLinkMaxPayload=768,kLinkTxQueueDepth=64;
constexpr uint16_t kLinkRxByteBudget=512;
// Mount calibration is deliberately opt-in. The identity matrix keeps the
// estimator observable for Stage A but reports DEGRADED until the measured
// BNO-to-body transform has been confirmed on the boat.
constexpr bool kBnoMountValidated=false;
constexpr float kBnoBodyXx=1,kBnoBodyXy=0,kBnoBodyXz=0,kBnoBodyYx=0,kBnoBodyYy=1,kBnoBodyYz=0,kBnoBodyZx=0,kBnoBodyZy=0,kBnoBodyZz=1;
constexpr float kEstimatorKp=1.2f,kEstimatorKi=0.002f,kEstimatorAccelNormGain=0.35f,kEstimatorAccelJerkGain=0.015f,kEstimatorAccelWeightMin=0.05f,kEstimatorWeightRecovery=0.08f,kEstimatorMagKp=0.15f,kEstimatorMagFieldTolerance=0.35f,kEstimatorYawCorrectionDecay=0.92f;
constexpr uint32_t kEstimatorGyroStaleUs=30000UL,kEstimatorAccelStaleUs=30000UL,kEstimatorMagStaleUs=120000UL,kEstimatorGnssStaleUs=500000UL,kEstimatorTofStaleUs=100000UL,kEstimatedStateTxIntervalMs=100UL,kPrimaryImuSnapshotTxIntervalMs=50UL;
}
namespace cfg=app_config;
