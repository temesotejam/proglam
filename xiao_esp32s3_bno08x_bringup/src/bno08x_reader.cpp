#include "bno08x_reader.h"

#include "app_config.h"

namespace {
template <typename T>
auto setInterruptPinTry(T& device, int pin, int) -> decltype(device.setInterruptPin(pin), void()) {
  device.setInterruptPin(pin);
}

template <typename T>
void setInterruptPinTry(T&, int, long) {}
}  // namespace

void Bno08xReader::beginI2c() {
  Wire.begin(app_config::kI2cSdaPin, app_config::kI2cSclPin, app_config::kI2cClockHz);
  pinMode(app_config::kBnoIntPin, INPUT_PULLUP);
  pinMode(app_config::kBnoRstPin, OUTPUT);
  // Keep RST released. This diagnostic build intentionally avoids toggling it.
  digitalWrite(app_config::kBnoRstPin, HIGH);
}

void Bno08xReader::pulseResetPin() const {
  digitalWrite(app_config::kBnoRstPin, LOW);
  delay(app_config::kResetLowMs);
  digitalWrite(app_config::kBnoRstPin, HIGH);
  delay(app_config::kResetBootWaitMs);
}

uint8_t Bno08xReader::scanAndSelectAddress() const {
  bool primary_found = false;
  bool alternate_found = false;
  Serial.println("I2C scan:");
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  found 0x%02X%s\n", address,
                    address == app_config::kBnoAddressPrimary ? " (BNO primary candidate)" :
                    address == app_config::kBnoAddressAlternate ? " (BNO alternate candidate)" : "");
      primary_found |= address == app_config::kBnoAddressPrimary;
      alternate_found |= address == app_config::kBnoAddressAlternate;
    }
  }
  if (primary_found) {
    return app_config::kBnoAddressPrimary;
  }
  if (alternate_found) {
    return app_config::kBnoAddressAlternate;
  }
  Serial.printf("  BNO08X candidates 0x%02X / 0x%02X were not found\n",
                app_config::kBnoAddressPrimary, app_config::kBnoAddressAlternate);
  return 0;
}

bool Bno08xReader::configureReports() {
  const bool accel_ok = bno_.enableReport(SH2_ACCELEROMETER, app_config::kAccelReportIntervalUs);
  const bool gyro_ok = bno_.enableReport(SH2_GYROSCOPE_CALIBRATED, app_config::kGyroReportIntervalUs);
  const bool rotation_ok = bno_.enableReport(SH2_ROTATION_VECTOR, app_config::kRotationReportIntervalUs);
  Serial.printf("Report enable: accel=%s gyro=%s rotation=%s\n", accel_ok ? "OK" : "FAIL",
                gyro_ok ? "OK" : "FAIL", rotation_ok ? "OK" : "FAIL");
  return accel_ok && gyro_ok && rotation_ok;
}

bool Bno08xReader::initialize(uint8_t address, Diagnostics& diagnostics) {
  initialized_ = false;
  reset_event_pending_ = false;
  Wire.begin(app_config::kI2cSdaPin, app_config::kI2cSclPin, app_config::kI2cClockHz);
  address_ = address;

  if (!bno_.begin_I2C(address_, &Wire)) {
    diagnostics.noteI2cError();
    Serial.println("BNO08X initialization failed (no ACK or SH-2 startup failure)");
    return false;
  }
  setInterruptPinTry(bno_, app_config::kBnoIntPin, 0);
  initialized_ = configureReports();
  if (!initialized_) {
    diagnostics.noteI2cError();
    Serial.println("BNO08X initialization failed while enabling reports");
    return false;
  }
  Serial.printf("BNO08X initialization OK at 0x%02X (Adafruit event reader)\n", address_);
  return true;
}

uint8_t Bno08xReader::service(Diagnostics& diagnostics, uint8_t max_events) {
  if (!initialized_) {
    return 0;
  }

  if (bno_.wasReset()) {
    reset_reason_ = 0;
    reset_event_pending_ = !configureReports();
    if (reset_event_pending_) {
      diagnostics.noteBnoReset(reset_reason_);
    } else {
      Serial.println("BNO08X reset detected; reports enabled again.");
    }
  }

  uint8_t handled = 0;
  sh2_SensorValue_t sensor_value{};
  while (handled < max_events && bno_.getSensorEvent(&sensor_value)) {
    const uint32_t rx_us = micros();
    const uint64_t sensor_us = sensor_value.timestamp;
    if (sensor_value.sensorId == SH2_ACCELEROMETER) {
      diagnostics.updateAccel(sensor_value.un.accelerometer.x, sensor_value.un.accelerometer.y,
                              sensor_value.un.accelerometer.z, sensor_value.status, sensor_us, rx_us);
    } else if (sensor_value.sensorId == SH2_GYROSCOPE_CALIBRATED) {
      diagnostics.updateGyro(sensor_value.un.gyroscope.x, sensor_value.un.gyroscope.y,
                             sensor_value.un.gyroscope.z, sensor_value.status, sensor_us, rx_us);
    } else if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
      attitude_math::Quaternion quaternion{sensor_value.un.rotationVector.i,
                                            sensor_value.un.rotationVector.j,
                                            sensor_value.un.rotationVector.k,
                                            sensor_value.un.rotationVector.real};
      diagnostics.updateRotation(quaternion, sensor_value.status, sensor_us, rx_us);
    }
    ++handled;
  }
  return handled;
}

bool Bno08xReader::consumeResetEvent(uint8_t& reset_reason) {
  if (!reset_event_pending_) {
    return false;
  }
  reset_event_pending_ = false;
  reset_reason = reset_reason_;
  return true;
}