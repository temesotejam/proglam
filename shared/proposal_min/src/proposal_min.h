#pragma once

#include <stdint.h>
#include <stddef.h>

namespace proposal_min {

enum class Safety : uint8_t { Disarmed = 0, Running = 1, EStop = 2, Fault = 3 };
enum class Saturation : uint8_t { Yaw = 0, Roll = 1, Height = 2, LeftWing = 3, RightWing = 4, RearYaw = 5, Propulsion = 6, Count = 7 };

struct GnssInput { double latitudeDeg = 0, longitudeDeg = 0; float speedMps = 0, courseRad = 0; uint64_t timestampUs = 0; bool valid = false; };
struct ImuInput { float yawRad = 0, rollRad = 0, pitchRad = 0, yawRateRadS = 0, rollRateRadS = 0, pitchRateRadS = 0; uint64_t timestampUs = 0; bool valid = false; };
struct TofInput { const uint16_t* rangesMm = nullptr; const uint8_t* status = nullptr; uint8_t count = 0; uint64_t timestampUs = 0; bool valid = false; };
struct Waypoint { float northM; float eastM; Waypoint(float n = 0, float e = 0) : northM(n), eastM(e) {} };

struct ActuatorConfig {
  float neutral = 0, min = -1, max = 1, sign = 1, maxDeltaPerStep = 1;
  bool calibrationRequired = true;
  ActuatorConfig() = default;
  ActuatorConfig(float n,float lo,float hi,float sg,float slew,bool cal) : neutral(n), min(lo), max(hi), sign(sg), maxDeltaPerStep(slew), calibrationRequired(cal) {}
};
struct ControlConfig {
  ActuatorConfig leftFrontWing{}, rightFrontWing{}, rearYaw{}, propulsion{};
  float targetHeightM = 1.2f, targetPitchRad = 0, targetRollRad = 0;
  float kpHeight = 0.7f, kpPitch = 0.8f, kdPitchRate = 0.08f;
  float kpRoll = 1.4f, kdRollRate = 0.25f;
  float kpYaw = 0.8f, kdYawRate = 0.0f;
  float propulsionCommand = 0, propulsionStop = 0;
  uint32_t gnssStaleUs = 500000, imuStaleUs = 100000, tofStaleUs = 250000, heartbeatStaleUs = 500000;
  float waypointReachM = 0.5f, minCourseSpeedMps = 0.5f;
  bool requireTofForAuto = true, calibrationRequired = true;
};

struct Input {
  GnssInput gnss{}; ImuInput imu{}; TofInput tof{};
  const Waypoint* waypoints = nullptr; uint8_t waypointCount = 0; uint64_t nowUs = 0;
  bool start = false, stop = false, estop = false, heartbeatOk = true;
};
struct Output {
  float leftFront = 0, rightFront = 0, rearYaw = 0, propulsion = 0;
  float left_front_wing = 0, right_front_wing = 0, rear_yaw = 0;
  float u_height = 0, u_pitch = 0, u_roll = 0, targetCourseRad = 0, courseErrorRad = 0;
  float leftPrelimit = 0, rightPrelimit = 0, rearYawPrelimit = 0, propulsionPrelimit = 0;
  float waypointDistanceM = 0;
  Safety safety = Safety::Disarmed;
  uint8_t stopReason = 0, waypointIndex = 0;
  bool finite = true, inputValid = true, saturated = false, waypointReached = false;
};

struct Metric { uint32_t calls = 0; uint64_t totalUs = 0; uint32_t maxUs = 0, deadlineMiss = 0, nanInf = 0, invalid = 0, saturation = 0, samples = 0; uint32_t sampleUs[64]{}; };
struct Metrics {
  Metric task; Metric operation[8];
  uint32_t queueCurrent = 0, queueHighWater = 0, queueDrop = 0, uartBytes = 0, uartDrop = 0, uartGaps = 0;
  uint32_t sdGenerated = 0, sdWritten = 0, sdBlocked = 0, sdErrors = 0;
  uint32_t sensorAgeGnssUs = 0, sensorAgeGvrUs = 0, sensorAgeGyroUs = 0, sensorAgeTofUs = 0, sensorAgeInaUs = 0;
  uint32_t i2cTransactions = 0, i2cErrors = 0, watchdogResets = 0, nanInf = 0, stateTransitions = 0, stopCount = 0, estopCount = 0, heartbeatTimeout = 0, overheadUs = 0;
  uint32_t shadowOutputCount = 0; float shadowMin[4] = {0, 0, 0, 0}, shadowMax[4] = {0, 0, 0, 0};
};

class Controller {
 public:
  Controller();
  void reset();
  Output step(const Input& input);
  Output step(const Input& input, const ControlConfig& config);
  const Metrics& metrics() const { return metrics_; }
  Metrics& metrics() { return metrics_; }
  Safety safety() const { return safety_; }
 private:
  void record(Metric& metric, uint32_t elapsedUs, uint32_t deadlineUs, bool invalid, bool saturated, bool finite);
  float clamp(float value, Saturation category);
  float slew(float desired, float previous, const ActuatorConfig& config, Saturation category);
  Safety safety_ = Safety::Disarmed;
  uint8_t waypointIndex_ = 0;
  bool originSet_ = false;
  double originLat_ = 0, originLon_ = 0;
  float heightM_ = 1.2f;
  float previousOutputs_[4] = {0, 0, 0, 0};
  uint64_t lastGnssUs_ = 0, lastGvrUs_ = 0, lastGyroUs_ = 0, lastTofUs_ = 0, lastStepUs_ = 0;
  Metrics metrics_{};
};

const char* safetyName(Safety state);
const char* stopReasonName(uint8_t reason);

}  // namespace proposal_min
