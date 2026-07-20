#include "diagnostics.h"

#include "app_config.h"

namespace {
template <typename Sample>
void updateTimingCommon(Sample& sample, uint32_t rx_us) {
  if (sample.valid) {
    const uint32_t interval_us = rx_us - sample.previous_rx_us;
    sample.interval_sum_us += interval_us;
    ++sample.interval_count;
    if (interval_us > sample.max_interval_us) {
      sample.max_interval_us = interval_us;
    }
  }
  sample.previous_rx_us = rx_us;
  sample.last_rx_us = rx_us;
  ++sample.sequence;
  sample.valid = true;
}

uint32_t periodHzMilli(uint32_t interval_count, uint32_t interval_sum_us) {
  if (interval_count == 0 || interval_sum_us == 0) {
    return 0;
  }
  return static_cast<uint32_t>((1000000000ULL * interval_count) / interval_sum_us);
}
}  // namespace

void Diagnostics::startMonitoring(uint32_t now_us) {
  accel_ = VectorSample{};
  gyro_ = VectorSample{};
  rotation_ = RotationSample{};
  monitoring_start_us_ = now_us;
}

void Diagnostics::updateTiming(VectorSample& sample, uint32_t rx_us) {
  updateTimingCommon(sample, rx_us);
}

void Diagnostics::updateTiming(RotationSample& sample, uint32_t rx_us) {
  updateTimingCommon(sample, rx_us);
}

void Diagnostics::updateAccel(float x, float y, float z, uint8_t accuracy,
                              uint64_t sensor_timestamp_us, uint32_t rx_us) {
  accel_.x = x;
  accel_.y = y;
  accel_.z = z;
  accel_.accuracy = accuracy;
  accel_.sensor_timestamp_us = sensor_timestamp_us;
  updateTiming(accel_, rx_us);
}

void Diagnostics::updateGyro(float x, float y, float z, uint8_t accuracy,
                             uint64_t sensor_timestamp_us, uint32_t rx_us) {
  gyro_.x = x;
  gyro_.y = y;
  gyro_.z = z;
  gyro_.accuracy = accuracy;
  gyro_.sensor_timestamp_us = sensor_timestamp_us;
  updateTiming(gyro_, rx_us);
}

void Diagnostics::updateRotation(const attitude_math::Quaternion& quaternion, uint8_t accuracy,
                                 uint64_t sensor_timestamp_us, uint32_t rx_us) {
  rotation_.quaternion = quaternion;
  rotation_.accuracy = accuracy;
  rotation_.sensor_timestamp_us = sensor_timestamp_us;
  attitude_math::quaternionToEulerDegrees(quaternion, rotation_.euler);
  updateTiming(rotation_, rx_us);
}

uint32_t Diagnostics::ageUs(bool valid, uint32_t last_rx_us, uint32_t now_us) {
  return valid ? now_us - last_rx_us : UINT32_MAX;
}

bool Diagnostics::timedOut(bool valid, uint32_t last_rx_us, uint32_t start_us, uint32_t now_us) {
  const uint32_t reference_us = valid ? last_rx_us : start_us;
  return (now_us - reference_us) >= app_config::kStreamTimeoutUs &&
         (now_us - start_us) >= app_config::kInitialDataTimeoutUs;
}

bool Diagnostics::hasTimedOut(uint32_t now_us) const {
  return timedOut(accel_.valid, accel_.last_rx_us, monitoring_start_us_, now_us) ||
         timedOut(gyro_.valid, gyro_.last_rx_us, monitoring_start_us_, now_us) ||
         timedOut(rotation_.valid, rotation_.last_rx_us, monitoring_start_us_, now_us);
}

const char* Diagnostics::firstTimedOutStream(uint32_t now_us) const {
  if (timedOut(accel_.valid, accel_.last_rx_us, monitoring_start_us_, now_us)) {
    return "accelerometer timeout";
  }
  if (timedOut(gyro_.valid, gyro_.last_rx_us, monitoring_start_us_, now_us)) {
    return "gyroscope timeout";
  }
  if (timedOut(rotation_.valid, rotation_.last_rx_us, monitoring_start_us_, now_us)) {
    return "rotation-vector timeout";
  }
  return "none";
}

void Diagnostics::noteReinitialization(const char* reason) {
  ++reinitialization_count_;
  last_reinit_reason_ = reason;
}

void Diagnostics::noteI2cError() { ++i2c_error_count_; }

void Diagnostics::noteBnoReset(uint8_t reason) {
  ++bno_reset_count_;
  last_bno_reset_reason_ = reason;
}

void Diagnostics::printCsv(uint32_t now_us) const {
  const uint32_t accel_age = ageUs(accel_.valid, accel_.last_rx_us, now_us);
  const uint32_t gyro_age = ageUs(gyro_.valid, gyro_.last_rx_us, now_us);
  const uint32_t rotation_age = ageUs(rotation_.valid, rotation_.last_rx_us, now_us);
  Serial.printf("%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.7f,%.7f,%.7f,%.7f,%.3f,%.3f,%.3f,%u,%lu,%lu,%lu\n",
                static_cast<unsigned long>(now_us), accel_.x, accel_.y, accel_.z, gyro_.x, gyro_.y,
                gyro_.z, rotation_.quaternion.x, rotation_.quaternion.y, rotation_.quaternion.z,
                rotation_.quaternion.w, rotation_.euler.roll_deg, rotation_.euler.pitch_deg,
                rotation_.euler.yaw_deg, rotation_.accuracy, static_cast<unsigned long>(accel_age),
                static_cast<unsigned long>(gyro_age), static_cast<unsigned long>(rotation_age));
}

void Diagnostics::printVectorPeriod(const char* label, const VectorSample& sample) {
  Serial.printf("  %s: valid=%d seq=%lu hz_milli=%lu max_gap_us=%lu age_us=%lu accuracy=%u sensor_us=%llu\n",
                label, sample.valid, static_cast<unsigned long>(sample.sequence),
                static_cast<unsigned long>(periodHzMilli(sample.interval_count, sample.interval_sum_us)),
                static_cast<unsigned long>(sample.max_interval_us),
                static_cast<unsigned long>(ageUs(sample.valid, sample.last_rx_us, micros())), sample.accuracy,
                static_cast<unsigned long long>(sample.sensor_timestamp_us));
}

void Diagnostics::printRotationPeriod(const RotationSample& sample) {
  Serial.printf("  rotation: valid=%d seq=%lu hz_milli=%lu max_gap_us=%lu age_us=%lu accuracy=%u sensor_us=%llu\n",
                sample.valid, static_cast<unsigned long>(sample.sequence),
                static_cast<unsigned long>(periodHzMilli(sample.interval_count, sample.interval_sum_us)),
                static_cast<unsigned long>(sample.max_interval_us),
                static_cast<unsigned long>(ageUs(sample.valid, sample.last_rx_us, micros())), sample.accuracy,
                static_cast<unsigned long long>(sample.sensor_timestamp_us));
}

void Diagnostics::printSummary(uint32_t now_us) {
  Serial.printf("# diagnostic: reinit=%lu i2c_errors=%lu bno_resets=%lu reset_reason=%u last_reinit=%s timeout=%s\n",
                static_cast<unsigned long>(reinitialization_count_), static_cast<unsigned long>(i2c_error_count_),
                static_cast<unsigned long>(bno_reset_count_), last_bno_reset_reason_, last_reinit_reason_,
                firstTimedOutStream(now_us));
  printVectorPeriod("accel", accel_);
  printVectorPeriod("gyro", gyro_);
  printRotationPeriod(rotation_);
}
