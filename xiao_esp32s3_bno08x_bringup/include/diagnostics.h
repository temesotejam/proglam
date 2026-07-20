#pragma once

#include <Arduino.h>

#include "attitude_math.h"

struct VectorSample {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  uint8_t accuracy = 0;
  uint64_t sensor_timestamp_us = 0;
  uint32_t last_rx_us = 0;
  uint32_t previous_rx_us = 0;
  uint32_t sequence = 0;
  uint32_t interval_sum_us = 0;
  uint32_t interval_count = 0;
  uint32_t max_interval_us = 0;
  bool valid = false;
};

struct RotationSample {
  attitude_math::Quaternion quaternion{0.0F, 0.0F, 0.0F, 1.0F};
  attitude_math::EulerDegrees euler{0.0F, 0.0F, 0.0F};
  uint8_t accuracy = 0;
  uint64_t sensor_timestamp_us = 0;
  uint32_t last_rx_us = 0;
  uint32_t previous_rx_us = 0;
  uint32_t sequence = 0;
  uint32_t interval_sum_us = 0;
  uint32_t interval_count = 0;
  uint32_t max_interval_us = 0;
  bool valid = false;
};

class Diagnostics {
 public:
  void startMonitoring(uint32_t now_us);
  void updateAccel(float x, float y, float z, uint8_t accuracy, uint64_t sensor_timestamp_us,
                   uint32_t rx_us);
  void updateGyro(float x, float y, float z, uint8_t accuracy, uint64_t sensor_timestamp_us,
                  uint32_t rx_us);
  void updateRotation(const attitude_math::Quaternion& quaternion, uint8_t accuracy,
                      uint64_t sensor_timestamp_us, uint32_t rx_us);

  bool hasTimedOut(uint32_t now_us) const;
  const char* firstTimedOutStream(uint32_t now_us) const;
  void noteReinitialization(const char* reason);
  void noteI2cError();
  void noteBnoReset(uint8_t reason);
  void printCsv(uint32_t now_us) const;
  void printSummary(uint32_t now_us);

  const VectorSample& accel() const { return accel_; }
  const VectorSample& gyro() const { return gyro_; }
  const RotationSample& rotation() const { return rotation_; }
  uint32_t reinitializationCount() const { return reinitialization_count_; }

 private:
  static void updateTiming(VectorSample& sample, uint32_t rx_us);
  static void updateTiming(RotationSample& sample, uint32_t rx_us);
  static uint32_t ageUs(bool valid, uint32_t last_rx_us, uint32_t now_us);
  static bool timedOut(bool valid, uint32_t last_rx_us, uint32_t start_us, uint32_t now_us);
  static void printVectorPeriod(const char* label, const VectorSample& sample);
  static void printRotationPeriod(const RotationSample& sample);

  VectorSample accel_;
  VectorSample gyro_;
  RotationSample rotation_;
  uint32_t monitoring_start_us_ = 0;
  uint32_t reinitialization_count_ = 0;
  uint32_t i2c_error_count_ = 0;
  uint32_t bno_reset_count_ = 0;
  uint8_t last_bno_reset_reason_ = 0;
  const char* last_reinit_reason_ = "none";
};
