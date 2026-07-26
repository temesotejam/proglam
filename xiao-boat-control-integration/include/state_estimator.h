#pragma once

#include <Arduino.h>
#include <boat_protocol.h>

namespace state_estimator {

// Event-driven Mahony-style attitude estimator.  It owns no I2C, UART, SD,
// or actuator resources, so acquisition and safety outputs remain independent.
class Estimator {
 public:
  void ingestGyro(uint64_t sensorUs, uint64_t receivedUs, float x, float y, float z);
  void ingestAccel(uint64_t sensorUs, uint64_t receivedUs, uint8_t accuracy, float x, float y, float z);
  void ingestMagnetic(uint64_t sensorUs, uint64_t receivedUs, uint8_t accuracy, float x, float y, float z);
  void updateGnss(uint64_t receivedUs, double latitudeDeg, double longitudeDeg, float speedMps, float courseRad, bool valid);
  void updateWaterDistance(uint64_t receivedUs, float distanceM, bool valid);
  boat::EstimatedStatePayload snapshot(uint64_t nowUs) const;

 private:
  static void bodyVector(float& x, float& y, float& z);
  void normalizeQuaternion();
  void applyAccelCorrection(float x, float y, float z, uint64_t sensorUs);
  void refreshEuler();

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  float qw_ = 1, qx_ = 0, qy_ = 0, qz_ = 0;
  float gyroBiasX_ = 0, gyroBiasY_ = 0, gyroBiasZ_ = 0;
  float gyroX_ = 0, gyroY_ = 0, gyroZ_ = 0;
  float roll_ = 0, pitch_ = 0, yaw_ = 0;
  float accelWeight_ = 0, yawCorrectionRadS_ = 0;
  float previousAccelNorm_ = 0;
  float magneticReferenceUt_ = 0;
  float latitudeDeg_ = NAN, longitudeDeg_ = NAN, groundSpeedMps_ = NAN, courseRad_ = NAN;
  float waterDistanceM_ = NAN;
  uint64_t lastGyroSensorUs_ = 0, lastAccelSensorUs_ = 0;
  uint64_t gyroReceivedUs_ = 0, accelReceivedUs_ = 0, magReceivedUs_ = 0, gnssReceivedUs_ = 0, tofReceivedUs_ = 0;
  bool accelCorrectionActive_ = false, magCorrectionActive_ = false, gnssYawCorrectionActive_ = false;
  bool gnssValid_ = false, waterValid_ = false;
};

}  // namespace state_estimator
