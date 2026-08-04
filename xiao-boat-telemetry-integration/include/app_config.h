#pragma once

#include <Arduino.h>
#include "experiment_config.h"
#ifndef SHADOW_CONTROL_ENABLE
#define SHADOW_CONTROL_ENABLE 0
#endif
#ifndef ACTUATOR_OUTPUT_ENABLE
#define ACTUATOR_OUTPUT_ENABLE 0
#endif
#ifndef PROPOSAL_PROFILE
#define PROPOSAL_PROFILE 0
#endif
static_assert(!SHADOW_CONTROL_ENABLE || !ACTUATOR_OUTPUT_ENABLE, "communication shadow cannot enable actuator output");

namespace app_config {
constexpr bool kProposalShadowEnable=SHADOW_CONTROL_ENABLE!=0; constexpr uint8_t kProposalProfile=(uint8_t)PROPOSAL_PROFILE; constexpr bool kPhysicalOutputCompileEnabled=ACTUATOR_OUTPUT_ENABLE!=0 && !kProposalShadowEnable;
constexpr char kFirmwareName[] = "xiao-boat-telemetry-integration";
constexpr char kFirmwareVersion[] = "0.3.8-provisional-shadow-system";
constexpr bool kDualImuTransformProvisional = true;
constexpr float kDualImuR00=0.99876f,kDualImuR01=0.03251f,kDualImuR02=0.03770f;
constexpr float kDualImuR10=-0.03271f,kDualImuR11=0.99945f,kDualImuR12=0.00477f;
constexpr float kDualImuR20=-0.03753f,kDualImuR21=-0.00600f,kDualImuR22=0.99928f;
constexpr uint32_t kDualImuStaleMs = 250UL;
// These gates intentionally admit only a provisional shadow estimate. They
// are replaced by the formal hull-axis calibration after mechanical fixing.
constexpr float kProvisionalMaxAccelDeltaMps2 = 3.0f;
constexpr float kProvisionalMaxGyroDeltaRadS = 0.25f;
// Relative IMU orientation is not yet a hull transform. Keep its shadow
// contribution bounded so a stationary coordinate mismatch cannot integrate
// into an implausible attitude display.
constexpr float kProvisionalMaxDualCorrectionRad = 5.0f * PI / 180.0f;
constexpr uint32_t kProvisionalGnssStaleMs = 500UL;
constexpr uint32_t kProvisionalTofStaleMs = 250UL;
constexpr uint32_t kProvisionalSystemIntervalMs = 50UL;
constexpr uint32_t kProvisionalLogIntervalMs = 100UL;

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
// Keep the RAM staging buffer large, but commit to SPI microSD one sector at a
// time.  A failed sector is never retried; the campaign is aborted safely.
constexpr size_t kSdWriteChunkBytes = 512;
// SD metadata is finalized when a run stops.  Calling File::flush() during a
// measurement can block long enough to invalidate the 80 ms BNO log-gap gate.
constexpr uint32_t kLogTaskWakeMs = 5UL;
constexpr UBaseType_t kLogTaskPriority = 2;
// Persist near-limit scheduling stalls; BOAT24 acceptance remains <= 80 ms.
constexpr uint32_t kTimingDiagnosticThresholdUs = 60000UL;
constexpr char kLogDirectory[] = "/BOATLOG";

constexpr uint8_t kBnoAddressPrimary = 0x4A;
constexpr uint32_t kBnoTaskFallbackMs = 2UL;
// BNO callbacks are queued before logging. At the BOAT23 local request rate
// (100 + 100 + 20 Hz), 96 slots give over 0.4 s of scheduling headroom.
constexpr uint16_t kBnoEventQueueDepth = 96;
constexpr uint8_t kBnoServiceCallBudget = 8;
constexpr UBaseType_t kBnoTaskPriority = 3;
constexpr uint8_t kBnoAddressAlternate = 0x4B;
constexpr uint32_t kBnoI2cHz = 100000UL;
constexpr uint32_t kAccelGyroIntervalUs = 10000UL;
constexpr uint32_t kMagneticIntervalUs = 50000UL;
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

constexpr uint32_t kGnssNavIntervalMs = 100UL;
constexpr uint32_t kControlHeartbeatIntervalMs = 100UL;
constexpr uint32_t kTimeSyncIntervalMs = 1000UL;
constexpr uint32_t kControlLinkTimeoutMs = 500UL;
constexpr uint32_t kDiagnosticIntervalMs = 1000UL;

// Automated benchmark. The browser only starts/stops a campaign; this state
// machine runs on the communication XIAO and continues after a page reload.
constexpr char kBenchDirectory[] = "/BENCH";
constexpr uint32_t kBenchWebRefreshMs = 50UL;
constexpr uint32_t kBenchPreflightMs = 1000UL;
constexpr uint32_t kBenchWarmupInaMs = 10000UL;
constexpr uint32_t kBenchWarmupTofMs = 20000UL;
constexpr uint32_t kBenchWarmupI2cMs = 20000UL;
constexpr uint32_t kBenchWarmupUartMs = 5000UL;
constexpr uint32_t kBenchCommandTimeoutMs = 500UL;
constexpr uint32_t kBenchResultTimeoutMs = 3000UL;
constexpr uint32_t kBenchPrepareTimeoutMs = 10000UL;  // ToF/I2C reinitialization may briefly block heartbeats.
constexpr uint32_t kBenchLinkWarnMs = 300UL;
constexpr uint32_t kBenchLinkAbortMs = 500UL;
constexpr uint16_t kBenchSyntheticPayloadBytes = 64;
constexpr bool kDryRunActuators = true;  // Required for benchmark admission.
}  // namespace app_config
