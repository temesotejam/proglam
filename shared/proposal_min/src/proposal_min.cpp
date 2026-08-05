#include "proposal_min.h"
#include "state_reason_generated.h"
#include <math.h>
#include <stdint.h>

namespace proposal_min {
static_assert(static_cast<uint8_t>(Safety::Disarmed)==state_reason_generated::kStateDisarmed && static_cast<uint8_t>(Safety::Running)==state_reason_generated::kStateRunning && static_cast<uint8_t>(Safety::EStop)==state_reason_generated::kStateEStop && static_cast<uint8_t>(Safety::Fault)==state_reason_generated::kStateFault, "state numbers must match canonical manifest");
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEarthMPerDeg = 111320.0f;
float wrapPi(float x) { while (x > kPi) x -= 2.0f * kPi; while (x < -kPi) x += 2.0f * kPi; return x; }
float finiteOr(float x, float fallback = 0) { return isfinite(x) ? x : fallback; }
bool fresh(uint64_t now, uint64_t stamp, uint32_t stale) {
  return stamp != 0 && now >= stamp && now - stamp <= stale;
}
float bounded(float x, float lo, float hi) { if (x < lo) return lo; if (x > hi) return hi; return x; }
}
Controller::Controller() { reset(); }
void Controller::reset() {
  safety_ = Safety::Disarmed; waypointIndex_ = 0; originSet_ = false;
  originLat_ = originLon_ = 0; heightM_ = 1.2f;
  previousOutputs_[0] = previousOutputs_[1] = previousOutputs_[2] = previousOutputs_[3] = 0;
  lastGnssUs_ = lastGvrUs_ = lastGyroUs_ = lastTofUs_ = lastStepUs_ = 0; metrics_ = Metrics{};
}
void Controller::record(Metric& m, uint32_t elapsed, uint32_t deadline, bool invalid, bool saturated, bool finite) {
  ++m.calls; m.totalUs += elapsed; if (elapsed > m.maxUs) m.maxUs = elapsed;
  if (elapsed > deadline) ++m.deadlineMiss;
  if (invalid) ++m.invalid;
  if (!finite) ++m.nanInf;
  if (saturated) ++m.saturation;
  if (m.samples < 64) m.sampleUs[m.samples++] = elapsed;
}
float Controller::clamp(float v, Saturation c) {
  if (!isfinite(v)) { ++metrics_.nanInf; return 0; }
  const float old = v; v = bounded(v, -1.0f, 1.0f);
  if (v != old) ++metrics_.operation[(uint8_t)c].saturation;
  return v;
}
float Controller::slew(float desired, float previous, const ActuatorConfig& cfg, Saturation c) {
  if (!isfinite(desired)) { ++metrics_.nanInf; return cfg.neutral; }
  float v = cfg.neutral + cfg.sign * (desired - cfg.neutral);
  v = bounded(v, cfg.min, cfg.max);
  const float delta = bounded(v - previous, -fabsf(cfg.maxDeltaPerStep), fabsf(cfg.maxDeltaPerStep));
  if (delta != v - previous) ++metrics_.operation[(uint8_t)c].saturation;
  return previous + delta;
}
Output Controller::step(const Input& input) {
  static const ControlConfig defaults{};
  return step(input, defaults);
}
Output Controller::step(const Input& in, const ControlConfig& cfg) {
  const uint64_t startUs = in.nowUs;
  Output out{};
  bool valid = true;
  if (in.stop) {
    if (safety_ != Safety::Disarmed) { safety_ = Safety::Disarmed; ++metrics_.stateTransitions; }
    ++metrics_.stopCount; out.stopReason = state_reason_generated::kReasonStop;
  } else if (in.estop) {
    if (safety_ != Safety::EStop) { safety_ = Safety::EStop; ++metrics_.stateTransitions; }
    ++metrics_.estopCount; out.stopReason = state_reason_generated::kReasonEStop;
  } else if (!in.heartbeatOk) {
    if (safety_ != Safety::Fault) { safety_ = Safety::Fault; ++metrics_.stateTransitions; }
    ++metrics_.heartbeatTimeout; out.stopReason = state_reason_generated::kReasonHeartbeatTimeout;
  } else if (in.start && safety_ == Safety::Disarmed) {
    safety_ = Safety::Running; ++metrics_.stateTransitions;
  }

  const bool gnssOk = in.gnss.valid && isfinite(in.gnss.latitudeDeg) && isfinite(in.gnss.longitudeDeg) &&
                      fresh(in.nowUs, in.gnss.timestampUs, cfg.gnssStaleUs);
  float north = 0, east = 0;
  if (gnssOk) {
    if (!originSet_) { originSet_ = true; originLat_ = in.gnss.latitudeDeg; originLon_ = in.gnss.longitudeDeg; }
    north = (float)((in.gnss.latitudeDeg - originLat_) * kEarthMPerDeg);
    east = (float)((in.gnss.longitudeDeg - originLon_) * kEarthMPerDeg * cos(originLat_ * kPi / 180.0));
    lastGnssUs_ = in.gnss.timestampUs;
  } else valid = false;
  record(metrics_.operation[0], 0, 100000, !gnssOk, false, isfinite(north) && isfinite(east));

  float targetCourse = finiteOr(in.gnss.courseRad, in.imu.valid ? in.imu.yawRad : 0);
  float distance = 0;
  bool reached = false;
  if (in.waypoints && in.waypointCount && gnssOk) {
    const uint8_t idx = waypointIndex_ < in.waypointCount ? waypointIndex_ : (uint8_t)(in.waypointCount - 1);
    Waypoint target = in.waypoints[idx];
    distance = hypotf(target.northM - north, target.eastM - east);
    reached = distance <= cfg.waypointReachM;
    if (reached && !in.stop && safety_ == Safety::Running && waypointIndex_ + 1 < in.waypointCount) {
      ++waypointIndex_; target = in.waypoints[waypointIndex_];
      distance = hypotf(target.northM - north, target.eastM - east);
      reached = false;
    }
    targetCourse = atan2f(target.eastM - east, target.northM - north);
  }
  out.targetCourseRad = targetCourse; out.waypointDistanceM = distance; out.waypointReached = reached;
  const bool imuOk = in.imu.valid && fresh(in.nowUs, in.imu.timestampUs, cfg.imuStaleUs) &&
                     isfinite(in.imu.yawRad) && isfinite(in.imu.rollRad) && isfinite(in.imu.pitchRad);
  if (imuOk) { lastGvrUs_ = lastGyroUs_ = in.imu.timestampUs; } else valid = false;
  out.gnssValid = gnssOk; out.imuValid = imuOk;
  record(metrics_.operation[1], 0, 100000, !imuOk, false, imuOk);

  const bool cogOk = gnssOk && isfinite(in.gnss.speedMps) && isfinite(in.gnss.courseRad) &&
                     in.gnss.speedMps >= cfg.minCourseSpeedMps;
  const float currentCourse = cogOk ? in.gnss.courseRad : (imuOk ? in.imu.yawRad : 0);
  const float courseError = wrapPi(targetCourse - currentCourse);
  const float yawRate = imuOk ? finiteOr(in.imu.yawRateRadS) : 0;
  const float uYaw = cfg.kpYaw * courseError - cfg.kdYawRate * yawRate;
  out.courseErrorRad = courseError;
  record(metrics_.operation[2], 0, 50000, !cogOk, false, isfinite(uYaw));

  const float roll = imuOk ? in.imu.rollRad : 0, pitch = imuOk ? in.imu.pitchRad : 0;
  const float rollRate = imuOk ? finiteOr(in.imu.rollRateRadS) : 0;
  const float pitchRate = imuOk ? finiteOr(in.imu.pitchRateRadS) : 0;
  out.u_roll = cfg.kpRoll * (cfg.targetRollRad - roll) - cfg.kdRollRate * rollRate;
  out.u_pitch = cfg.kpPitch * (cfg.targetPitchRad - pitch) - cfg.kdPitchRate * pitchRate;
  record(metrics_.operation[3], 0, 20000, !imuOk, false, isfinite(out.u_roll) && isfinite(out.u_pitch));

  bool tofOk = in.tof.valid && in.tof.rangesMm && in.tof.status && in.tof.count &&
               fresh(in.nowUs, in.tof.timestampUs, cfg.tofStaleUs);
  uint16_t values[64]{}; uint8_t n = 0;
  if (tofOk) {
    for (uint8_t i = 0; i < in.tof.count && i < 64; ++i) {
      if ((in.tof.status[i] == 5 || in.tof.status[i] == 9) && in.tof.rangesMm[i] >= 200 && in.tof.rangesMm[i] <= 4000) values[n++] = in.tof.rangesMm[i];
    }
    if (n < 4) tofOk = false;
  }
  if (tofOk) {
    for (uint8_t i = 1; i < n; ++i) { uint16_t x = values[i]; uint8_t j = i; while (j && values[j - 1] > x) { values[j] = values[j - 1]; --j; } values[j] = x; }
    float distanceM = values[n / 2] / 1000.0f;
    out.tofRawMm = values[n / 2];
    distanceM *= cosf(roll) * cosf(pitch);
    heightM_ = 0.9f * heightM_ + 0.1f * distanceM; lastTofUs_ = in.tof.timestampUs;
  } else if (cfg.requireTofForAuto && safety_ == Safety::Running) valid = false;
  out.tofValid = tofOk;
  out.u_height = cfg.kpHeight * (cfg.targetHeightM - heightM_);
  out.tofFilteredM = heightM_; out.heightErrorM = cfg.targetHeightM - heightM_; out.heightValid = tofOk;
  record(metrics_.operation[4], 0, 100000, !tofOk, false, isfinite(heightM_));
  record(metrics_.operation[5], 0, 20000, !tofOk, false, isfinite(out.u_height));

  const float common = out.u_height + out.u_pitch;
  out.u_yaw = uYaw; out.frontCommon = common; out.frontDifferential = out.u_roll;
  out.leftPrelimit = common + out.u_roll + uYaw;
  out.rightPrelimit = common - out.u_roll + uYaw;
  out.rearYawPrelimit = uYaw;
  out.propulsionPrelimit = cfg.propulsionCommand;
  const bool running = safety_ == Safety::Running;
  if (!running || !gnssOk || !imuOk || (cfg.requireTofForAuto && !tofOk)) {
    if (running && !in.stop && !in.estop && in.heartbeatOk) {
      safety_ = Safety::Fault; ++metrics_.stateTransitions;
      if (!gnssOk) out.stopReason = !in.gnss.valid ? state_reason_generated::kReasonGnssInvalid : ((!isfinite(in.gnss.latitudeDeg) || !isfinite(in.gnss.longitudeDeg)) ? state_reason_generated::kReasonNonfinite : state_reason_generated::kReasonGnssStale);
      else if (!imuOk) out.stopReason = !in.imu.valid ? state_reason_generated::kReasonImuInvalid : ((!isfinite(in.imu.yawRad) || !isfinite(in.imu.rollRad) || !isfinite(in.imu.pitchRad)) ? state_reason_generated::kReasonNonfinite : state_reason_generated::kReasonImuStale);
      else if (!tofOk) out.stopReason = in.tof.valid ? state_reason_generated::kReasonTofStale : state_reason_generated::kReasonTofInvalid;
      else out.stopReason = state_reason_generated::kReasonGnssInvalid;
    }
    out.leftPrelimit = out.rightPrelimit = out.rearYawPrelimit = 0;
    out.propulsionPrelimit = cfg.propulsionStop;
  }
  const bool forceSafe = safety_ != Safety::Running;
  if (forceSafe) { out.leftFront = cfg.leftFrontWing.neutral; out.rightFront = cfg.rightFrontWing.neutral; out.rearYaw = cfg.rearYaw.neutral; out.propulsion = cfg.propulsionStop; }
  else {
    out.leftFront = slew(out.leftPrelimit, previousOutputs_[0], cfg.leftFrontWing, Saturation::LeftWing);
    out.rightFront = slew(out.rightPrelimit, previousOutputs_[1], cfg.rightFrontWing, Saturation::RightWing);
    out.rearYaw = slew(out.rearYawPrelimit, previousOutputs_[2], cfg.rearYaw, Saturation::RearYaw);
    out.propulsion = slew(out.propulsionPrelimit, previousOutputs_[3], cfg.propulsion, Saturation::Propulsion);
  }
  previousOutputs_[0] = out.leftFront; previousOutputs_[1] = out.rightFront; previousOutputs_[2] = out.rearYaw; previousOutputs_[3] = out.propulsion;
  if (!isfinite(out.leftFront) || !isfinite(out.rightFront) || !isfinite(out.rearYaw) || !isfinite(out.propulsion)) {
    ++metrics_.nanInf; out.leftFront = out.rightFront = out.rearYaw = out.propulsion = 0; safety_ = Safety::Fault; out.stopReason = state_reason_generated::kReasonNonfinite; valid = false;
  }
  out.left_front_wing = out.leftFront; out.right_front_wing = out.rightFront; out.rear_yaw = out.rearYaw;
  out.safety = safety_; out.waypointIndex = waypointIndex_; out.inputValid = valid;
  out.finite = isfinite(out.leftFront) && isfinite(out.rightFront) && isfinite(out.rearYaw) && isfinite(out.propulsion);
  out.saturated = metrics_.operation[(uint8_t)Saturation::LeftWing].saturation || metrics_.operation[(uint8_t)Saturation::RightWing].saturation || metrics_.operation[(uint8_t)Saturation::RearYaw].saturation || metrics_.operation[(uint8_t)Saturation::Propulsion].saturation;
  if (lastStepUs_ && in.nowUs >= lastStepUs_ && in.nowUs - lastStepUs_ > 20000) ++metrics_.task.deadlineMiss;
  lastStepUs_ = in.nowUs;
  metrics_.sensorAgeGnssUs = lastGnssUs_ ? (uint32_t)(in.nowUs >= lastGnssUs_ ? in.nowUs - lastGnssUs_ : 0) : UINT32_MAX;
  metrics_.sensorAgeGvrUs = lastGvrUs_ ? (uint32_t)(in.nowUs >= lastGvrUs_ ? in.nowUs - lastGvrUs_ : 0) : UINT32_MAX;
  metrics_.sensorAgeGyroUs = lastGyroUs_ ? (uint32_t)(in.nowUs >= lastGyroUs_ ? in.nowUs - lastGyroUs_ : 0) : UINT32_MAX;
  metrics_.sensorAgeTofUs = lastTofUs_ ? (uint32_t)(in.nowUs >= lastTofUs_ ? in.nowUs - lastTofUs_ : 0) : UINT32_MAX;
  metrics_.overheadUs += (uint32_t)(in.nowUs - startUs);
  const float vals[4] = {out.leftFront, out.rightFront, out.rearYaw, out.propulsion};
  if (!metrics_.shadowOutputCount) { for (uint8_t i = 0; i < 4; ++i) metrics_.shadowMin[i] = metrics_.shadowMax[i] = vals[i]; }
  else for (uint8_t i = 0; i < 4; ++i) { if (vals[i] < metrics_.shadowMin[i]) metrics_.shadowMin[i] = vals[i]; if (vals[i] > metrics_.shadowMax[i]) metrics_.shadowMax[i] = vals[i]; }
  ++metrics_.shadowOutputCount;
  return out;
}
const char* safetyName(Safety state) {
  switch (state) { case Safety::Disarmed: return "DISARMED"; case Safety::Running: return "RUNNING"; case Safety::EStop: return "E_STOP"; default: return "FAULT"; }
}
const char* stopReasonName(uint8_t reason) {
  switch (reason) { case 1: return "STOP"; case 2: return "ESTOP"; case 3: return "HEARTBEAT_TIMEOUT"; case 4: return "GNSS_INVALID"; case 5: return "GNSS_STALE"; case 6: return "IMU_INVALID"; case 7: return "IMU_STALE"; case 8: return "TOF_INVALID"; case 9: return "TOF_STALE"; case 10: return "NONFINITE"; case 11: return "VESC_FAULT"; default: return "NONE"; }
}
}  // namespace proposal_min
