#include "state_estimator.h"

#include <math.h>

#include "app_config.h"

namespace state_estimator {
namespace {
constexpr float kGravityMps2 = 9.80665f;
constexpr float kMinDt = 0.0005f;
constexpr float kMaxDt = 0.050f;
constexpr float kPi = 3.14159265358979323846f;

float wrapPi(float value) {
  while (value > kPi) value -= 2.0f * kPi;
  while (value < -kPi) value += 2.0f * kPi;
  return value;
}

uint32_t ageUs(uint64_t then, uint64_t now) {
  return then && now >= then ? static_cast<uint32_t>(min<uint64_t>(now - then, UINT32_MAX)) : UINT32_MAX;
}
}  // namespace

void Estimator::bodyVector(float& x, float& y, float& z) {
  const float sx = x, sy = y, sz = z;
  x = app_config::kBnoBodyXx * sx + app_config::kBnoBodyXy * sy + app_config::kBnoBodyXz * sz;
  y = app_config::kBnoBodyYx * sx + app_config::kBnoBodyYy * sy + app_config::kBnoBodyYz * sz;
  z = app_config::kBnoBodyZx * sx + app_config::kBnoBodyZy * sy + app_config::kBnoBodyZz * sz;
}

void Estimator::normalizeQuaternion() {
  const float norm = sqrtf(qw_ * qw_ + qx_ * qx_ + qy_ * qy_ + qz_ * qz_);
  if (norm < 1e-6f) { qw_ = 1; qx_ = qy_ = qz_ = 0; return; }
  qw_ /= norm; qx_ /= norm; qy_ /= norm; qz_ /= norm;
}

void Estimator::refreshEuler() {
  // The quaternion uses the declared body axes.  The public control angles
  // follow the design: roll about +Y, pitch about -X, yaw about +Z.
  const float standardRoll = atan2f(2.0f * (qw_ * qx_ + qy_ * qz_), 1.0f - 2.0f * (qx_ * qx_ + qy_ * qy_));
  const float standardPitch = asinf(constrain(2.0f * (qw_ * qy_ - qz_ * qx_), -1.0f, 1.0f));
  roll_ = standardPitch;
  pitch_ = -standardRoll;
  yaw_ = wrapPi(atan2f(2.0f * (qw_ * qz_ + qx_ * qy_), 1.0f - 2.0f * (qy_ * qy_ + qz_ * qz_)));
}

void Estimator::applyAccelCorrection(float x, float y, float z, uint64_t sensorUs) {
  const float norm = sqrtf(x * x + y * y + z * z);
  if (norm < 0.1f) { accelCorrectionActive_ = false; return; }
  const float dt = lastAccelSensorUs_ && sensorUs > lastAccelSensorUs_ ? (sensorUs - lastAccelSensorUs_) / 1000000.0f : 0.0f;
  const float jerk = dt > kMinDt && dt < kMaxDt ? fabsf(norm - previousAccelNorm_) / dt : 0.0f;
  previousAccelNorm_ = norm;
  const float targetWeight = constrain(1.0f - app_config::kEstimatorAccelNormGain * fabsf(norm - kGravityMps2) - app_config::kEstimatorAccelJerkGain * jerk, app_config::kEstimatorAccelWeightMin, 1.0f);
  accelWeight_ += (targetWeight - accelWeight_) * app_config::kEstimatorWeightRecovery;
  x /= norm; y /= norm; z /= norm;
  const float vx = 2.0f * (qx_ * qz_ - qw_ * qy_);
  const float vy = 2.0f * (qw_ * qx_ + qy_ * qz_);
  const float vz = qw_ * qw_ - qx_ * qx_ - qy_ * qy_ + qz_ * qz_;
  gyroBiasX_ += app_config::kEstimatorKi * accelWeight_ * (y * vz - z * vy);
  gyroBiasY_ += app_config::kEstimatorKi * accelWeight_ * (z * vx - x * vz);
  gyroBiasZ_ += app_config::kEstimatorKi * accelWeight_ * (x * vy - y * vx);
  gyroX_ += app_config::kEstimatorKp * accelWeight_ * (y * vz - z * vy);
  gyroY_ += app_config::kEstimatorKp * accelWeight_ * (z * vx - x * vz);
  gyroZ_ += app_config::kEstimatorKp * accelWeight_ * (x * vy - y * vx);
  accelCorrectionActive_ = accelWeight_ > app_config::kEstimatorAccelWeightMin;
}

void Estimator::ingestGyro(uint64_t sensorUs, uint64_t receivedUs, float x, float y, float z) {
  bodyVector(x, y, z);
  portENTER_CRITICAL(&mux_);
  const float dt = lastGyroSensorUs_ && sensorUs > lastGyroSensorUs_ ? (sensorUs - lastGyroSensorUs_) / 1000000.0f : 0.0f;
  gyroX_ = x - gyroBiasX_;
  gyroY_ = y - gyroBiasY_;
  gyroZ_ = z - gyroBiasZ_ + yawCorrectionRadS_;
  yawCorrectionRadS_ *= app_config::kEstimatorYawCorrectionDecay;
  if (dt >= kMinDt && dt <= kMaxDt) {
    const float halfDt = 0.5f * dt;
    const float w = qw_, qx = qx_, qy = qy_, qz = qz_;
    qw_ += (-qx * gyroX_ - qy * gyroY_ - qz * gyroZ_) * halfDt;
    qx_ += ( w * gyroX_ + qy * gyroZ_ - qz * gyroY_) * halfDt;
    qy_ += ( w * gyroY_ - qx * gyroZ_ + qz * gyroX_) * halfDt;
    qz_ += ( w * gyroZ_ + qx * gyroY_ - qy * gyroX_) * halfDt;
    normalizeQuaternion(); refreshEuler();
  }
  lastGyroSensorUs_ = sensorUs; gyroReceivedUs_ = receivedUs;
  portEXIT_CRITICAL(&mux_);
}

void Estimator::ingestAccel(uint64_t sensorUs, uint64_t receivedUs, uint8_t, float x, float y, float z) {
  bodyVector(x, y, z);
  portENTER_CRITICAL(&mux_);
  applyAccelCorrection(x, y, z, sensorUs);
  lastAccelSensorUs_ = sensorUs; accelReceivedUs_ = receivedUs;
  portEXIT_CRITICAL(&mux_);
}

void Estimator::ingestMagnetic(uint64_t, uint64_t receivedUs, uint8_t accuracy, float x, float y, float z) {
  bodyVector(x, y, z);
  portENTER_CRITICAL(&mux_);
  const float magnitude = sqrtf(x * x + y * y + z * z);
  if (!magneticReferenceUt_ && magnitude > 1.0f) magneticReferenceUt_ = magnitude;
  const bool usable = app_config::kBnoMountValidated && accuracy >= 1 && magneticReferenceUt_ > 1.0f && fabsf(magnitude - magneticReferenceUt_) <= magneticReferenceUt_ * app_config::kEstimatorMagFieldTolerance;
  if (usable) {
    const float yawMag = atan2f(x, y);
    yawCorrectionRadS_ = app_config::kEstimatorMagKp * wrapPi(yawMag - yaw_);
    magCorrectionActive_ = true;
  } else {
    magCorrectionActive_ = false;
  }
  magReceivedUs_ = receivedUs;
  portEXIT_CRITICAL(&mux_);
}

void Estimator::updateGnss(uint64_t receivedUs, double latitudeDeg, double longitudeDeg, float speedMps, float courseRad, bool valid) {
  portENTER_CRITICAL(&mux_);
  latitudeDeg_ = latitudeDeg; longitudeDeg_ = longitudeDeg; groundSpeedMps_ = speedMps; courseRad_ = courseRad; gnssValid_ = valid; gnssReceivedUs_ = receivedUs;
  gnssYawCorrectionActive_ = false;  // GNSS course is comparison-only until straight-run validation.
  portEXIT_CRITICAL(&mux_);
}

void Estimator::updateWaterDistance(uint64_t receivedUs, float distanceM, bool valid) {
  portENTER_CRITICAL(&mux_);
  if (valid) waterDistanceM_ = distanceM;
  waterValid_ = valid; tofReceivedUs_ = receivedUs;
  portEXIT_CRITICAL(&mux_);
}

boat::EstimatedStatePayload Estimator::snapshot(uint64_t nowUs) const {
  boat::EstimatedStatePayload state{};
  portENTER_CRITICAL(&mux_);
  state.estimateUs = nowUs; state.qw = qw_; state.qx = qx_; state.qy = qy_; state.qz = qz_;
  state.rollRad = roll_; state.pitchRad = pitch_; state.yawRad = yaw_;
  state.rollRateRadS = gyroY_; state.pitchRateRadS = -gyroX_; state.yawRateRadS = gyroZ_;
  state.gyroBiasX = gyroBiasX_; state.gyroBiasY = gyroBiasY_; state.gyroBiasZ = gyroBiasZ_;
  state.latitudeDeg = latitudeDeg_; state.longitudeDeg = longitudeDeg_; state.groundSpeedMps = groundSpeedMps_; state.courseOverGroundRad = courseRad_; state.sideslipEstimateRad = NAN; state.waterDistanceM = waterDistanceM_;
  state.gyroAgeUs = ageUs(gyroReceivedUs_, nowUs); state.accelAgeUs = ageUs(accelReceivedUs_, nowUs); state.magAgeUs = ageUs(magReceivedUs_, nowUs); state.gnssAgeUs = ageUs(gnssReceivedUs_, nowUs); state.tofAgeUs = ageUs(tofReceivedUs_, nowUs);
  state.attitudeHealth = static_cast<uint8_t>(state.gyroAgeUs <= app_config::kEstimatorGyroStaleUs && state.accelAgeUs <= app_config::kEstimatorAccelStaleUs ? (app_config::kBnoMountValidated ? boat::EstimateHealth::Valid : boat::EstimateHealth::Degraded) : boat::EstimateHealth::Invalid);
  state.yawHealth = static_cast<uint8_t>(state.gyroAgeUs <= app_config::kEstimatorGyroStaleUs && state.magAgeUs <= app_config::kEstimatorMagStaleUs && magCorrectionActive_ && app_config::kBnoMountValidated ? boat::EstimateHealth::Valid : state.gyroAgeUs <= app_config::kEstimatorGyroStaleUs ? boat::EstimateHealth::Degraded : boat::EstimateHealth::Invalid);
  state.navigationHealth = static_cast<uint8_t>(gnssValid_ && state.gnssAgeUs <= app_config::kEstimatorGnssStaleUs ? boat::EstimateHealth::Valid : boat::EstimateHealth::Invalid);
  state.heightHealth = static_cast<uint8_t>(waterValid_ && state.tofAgeUs <= app_config::kEstimatorTofStaleUs ? boat::EstimateHealth::Valid : boat::EstimateHealth::Invalid);
  if (accelCorrectionActive_) state.flags |= boat::EstimateAccelCorrection;
  if (magCorrectionActive_) state.flags |= boat::EstimateMagCorrection;
  if (gnssYawCorrectionActive_) state.flags |= boat::EstimateGnssYawCorrection;
  if (app_config::kBnoMountValidated) state.flags |= boat::EstimateMountValidated;
  portEXIT_CRITICAL(&mux_);
  return state;
}

}  // namespace state_estimator
