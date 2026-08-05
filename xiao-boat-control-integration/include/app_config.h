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
#ifndef COMPETITION_SHADOW_ENABLE
#define COMPETITION_SHADOW_ENABLE 0
#endif
#ifndef COMPETITION_HARDWARE_ENABLE
#define COMPETITION_HARDWARE_ENABLE 0
#endif
#ifndef PROPOSAL_PROFILE
#define PROPOSAL_PROFILE 0
#endif
static_assert(!BENCHMARK_ENABLE || !ACTUATOR_OUTPUT_ENABLE,
              "proposal benchmark must never enable actuator output");
static_assert(!REPLAY_ENABLE || !ACTUATOR_OUTPUT_ENABLE,
              "proposal replay must never enable actuator output");
static_assert(!SHADOW_CONTROL_ENABLE || !ACTUATOR_OUTPUT_ENABLE,
              "MIN shadow control must never enable actuator output");
namespace app_config {
static_assert(!(COMPETITION_SHADOW_ENABLE && COMPETITION_HARDWARE_ENABLE), "competition shadow and hardware profiles are mutually exclusive");
static_assert(!COMPETITION_SHADOW_ENABLE || (!ACTUATOR_OUTPUT_ENABLE && SHADOW_CONTROL_ENABLE), "competition shadow must keep physical output disabled");
static_assert(!COMPETITION_HARDWARE_ENABLE || (ACTUATOR_OUTPUT_ENABLE && !SHADOW_CONTROL_ENABLE), "competition hardware requires its explicit physical-output profile");

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
constexpr float kVescMaxDuty=0.03f,kVescTestDuty=0.03f,kVescRampRiseSeconds=.5f,kVescRampFallSeconds=.5f; constexpr uint32_t kVescUartBaud=115200UL,kVescRequestIntervalMs=20UL,kVescFrameTimeoutMs=100UL,kVescMaxPayloadBytes=512,kVescKeepaliveMs=50UL,kVescControlIntervalUs=20000UL,kMaxTestMs=300000UL;
constexpr uint32_t kLinkBaud=921600UL,kProtocolVersion=1,kLinkHeartbeatTimeoutMs=500UL; constexpr bool kRequireHostHeartbeat=false;
constexpr bool kBenchmarkEnable=BENCHMARK_ENABLE!=0,kReplayEnable=REPLAY_ENABLE!=0;
constexpr bool kShadowControlEnable=SHADOW_CONTROL_ENABLE!=0;
constexpr bool kCompetitionShadowEnable=COMPETITION_SHADOW_ENABLE!=0;
constexpr bool kCompetitionHardwareEnable=COMPETITION_HARDWARE_ENABLE!=0;
constexpr bool kCompetitionControlEnable=kCompetitionShadowEnable||kCompetitionHardwareEnable;
constexpr bool kActuatorOutputCompileEnable=ACTUATOR_OUTPUT_ENABLE!=0;
constexpr bool kDryRunActuators=!kCompetitionHardwareEnable; constexpr uint32_t kGnssNavExpectedIntervalMs=100UL,kControlHeartbeatIntervalMs=100UL,kLinkFailSafeTimeoutMs=500UL;
// Only the dedicated hardware environment can reach PWM/UART output routines.
constexpr bool kShadowOnly=!kCompetitionHardwareEnable,kActuatorOutputEnabled=kCompetitionHardwareEnable,kEnableIna226=false;
constexpr uint8_t kProposalProfile=(uint8_t)PROPOSAL_PROFILE;
// Future code must use this guard before any PCA9685/VESC write.
constexpr bool kProposalActuatorPathEnabled = kActuatorOutputEnabled &&
                                               kActuatorOutputCompileEnable &&
                                               !kBenchmarkEnable && !kReplayEnable;
constexpr bool kPhysicalOutputCompileEnabled = kCompetitionHardwareEnable && kActuatorOutputCompileEnable && !kShadowControlEnable && !kBenchmarkEnable && !kReplayEnable;
// Commissioning defaults. They are deliberately narrow until each linkage is calibrated.
constexpr uint8_t kCompetitionLeftChannel=0,kCompetitionRightChannel=1,kCompetitionRearChannel=2;
constexpr float kCompetitionServoMinUs=1480.0f,kCompetitionServoNeutralUs=1500.0f,kCompetitionServoMaxUs=1520.0f,kCompetitionServoRateUsPerSecond=100.0f;
constexpr bool kCompetitionLeftReversed=false,kCompetitionRightReversed=false,kCompetitionRearReversed=false;
constexpr float kCompetitionKpPitch=kCompetitionHardwareEnable?0.0f:0.8f,kCompetitionKdPitch=kCompetitionHardwareEnable?0.0f:0.08f,kCompetitionKpRoll=kCompetitionHardwareEnable?0.0f:1.4f,kCompetitionKdRoll=kCompetitionHardwareEnable?0.0f:0.25f,kCompetitionKpHeight=kCompetitionHardwareEnable?0.0f:0.7f,kCompetitionKpYaw=kCompetitionHardwareEnable?0.0f:0.8f,kCompetitionKdYaw=0.0f;
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
constexpr bool kBnoMountValidated=false; // Proposal MIN output/control configuration. Values are normalized until mechanism calibration.
constexpr float kProposalTargetHeightM=1.2f, kProposalTargetPitchRad=0.0f, kProposalTargetRollRad=0.0f;
constexpr float kProposalKpHeight=0.7f, kProposalKpPitch=0.8f, kProposalKdPitchRate=0.08f, kProposalKpRoll=1.4f, kProposalKdRollRate=0.25f, kProposalKpYaw=0.8f, kProposalKdYawRate=0.0f;
constexpr float kProposalPropulsionCommand=0.0f, kProposalPropulsionStop=0.0f;
constexpr uint32_t kProposalGnssStaleUs=500000UL, kProposalImuStaleUs=100000UL, kProposalTofStaleUs=250000UL, kProposalHeartbeatStaleUs=500000UL;
constexpr float kProposalWaypointReachM=0.5f, kProposalMinCourseSpeedMps=0.5f;
constexpr float kProposalLeftNeutral=0.0f, kProposalLeftMin=-1.0f, kProposalLeftMax=1.0f, kProposalLeftSign=1.0f, kProposalLeftSlew=1.0f;
constexpr float kProposalRightNeutral=0.0f, kProposalRightMin=-1.0f, kProposalRightMax=1.0f, kProposalRightSign=1.0f, kProposalRightSlew=1.0f;
constexpr float kProposalRearNeutral=0.0f, kProposalRearMin=-1.0f, kProposalRearMax=1.0f, kProposalRearSign=1.0f, kProposalRearSlew=1.0f;
constexpr float kProposalPropNeutral=0.0f, kProposalPropMin=0.0f, kProposalPropMax=0.0f, kProposalPropSign=1.0f, kProposalPropSlew=1.0f;
constexpr bool kProposalCalibrationRequired=true, kProposalLeftCalibrationRequired=true, kProposalRightCalibrationRequired=true, kProposalRearCalibrationRequired=true, kProposalPropulsionCalibrationRequired=true;
constexpr float kBnoBodyXx=1,kBnoBodyXy=0,kBnoBodyXz=0,kBnoBodyYx=0,kBnoBodyYy=1,kBnoBodyYz=0,kBnoBodyZx=0,kBnoBodyZy=0,kBnoBodyZz=1;
constexpr float kEstimatorKp=1.2f,kEstimatorKi=0.002f,kEstimatorAccelNormGain=0.35f,kEstimatorAccelJerkGain=0.015f,kEstimatorAccelWeightMin=0.05f,kEstimatorWeightRecovery=0.08f,kEstimatorMagKp=0.15f,kEstimatorMagFieldTolerance=0.35f,kEstimatorYawCorrectionDecay=0.92f;
constexpr uint32_t kEstimatorGyroStaleUs=30000UL,kEstimatorAccelStaleUs=30000UL,kEstimatorMagStaleUs=120000UL,kEstimatorGnssStaleUs=500000UL,kEstimatorTofStaleUs=100000UL,kEstimatedStateTxIntervalMs=100UL,kPrimaryImuSnapshotTxIntervalMs=50UL;
}
namespace cfg=app_config;
