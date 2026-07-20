#include <Arduino.h>

#include "app_config.h"
#include "bno08x_reader.h"
#include "diagnostics.h"
#include "web_ui.h"

namespace {
Bno08xReader g_bno;
Diagnostics g_diagnostics;
WebUi g_web_ui;
uint32_t g_last_csv_us = 0;
uint32_t g_last_diagnostic_us = 0;
uint32_t g_last_reinit_attempt_us = 0;
uint8_t g_reinit_attempts = 0;

void printStartupBanner() {
  Serial.println();
  Serial.println("=== BNO08X standalone bring-up ===");
  Serial.printf("firmware: %s v%s\n", app_config::kFirmwareName, app_config::kFirmwareVersion);
  Serial.printf("build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("pins: SDA=D3(%d), SCL=D2(%d), INT=D1(%d), RST=D0(%d)\n",
                app_config::kI2cSdaPin, app_config::kI2cSclPin, app_config::kBnoIntPin,
                app_config::kBnoRstPin);
  Serial.printf("I2C: %lu Hz; BNO address: primary=0x%02X alternate=0x%02X\n",
                static_cast<unsigned long>(app_config::kI2cClockHz), app_config::kBnoAddressPrimary,
                app_config::kBnoAddressAlternate);
  Serial.printf("report intervals: accel=%lu us gyro=%lu us rotation=%lu us\n",
                static_cast<unsigned long>(app_config::kAccelReportIntervalUs),
                static_cast<unsigned long>(app_config::kGyroReportIntervalUs),
                static_cast<unsigned long>(app_config::kRotationReportIntervalUs));
  Serial.println("INT: D1 is configured as input; events are polled in loop(); RST is held HIGH (diagnostic).");
  Serial.println("sensor timestamp: available from the Adafruit SH-2 event timestamp (logged in diagnostics only).");
}

bool initializeDetectedBno() {
  const uint8_t address = g_bno.scanAndSelectAddress();
  if (address == 0) {
    Serial.println("ERROR STATE: BNO08X not detected. No automatic reset loop will be started.");
    return false;
  }
  if (!g_bno.initialize(address, g_diagnostics)) {
    Serial.println("ERROR STATE: initialization failed. Check wiring, voltage, I2C mode, and address.");
    return false;
  }
  g_diagnostics.startMonitoring(micros());
  g_reinit_attempts = 0;
  Serial.println("Acquisition started.");
  Serial.println("timestamp_us,accel_x_mps2,accel_y_mps2,accel_z_mps2,gyro_x_rad_s,gyro_y_rad_s,gyro_z_rad_s,quat_x,quat_y,quat_z,quat_w,roll_deg,pitch_deg,yaw_deg,quat_accuracy,accel_age_us,gyro_age_us,rotation_age_us");
  return true;
}

void attemptReinitialization(const char* reason) {
  uint32_t now_us = micros();
  if (g_reinit_attempts >= app_config::kMaxReinitAttempts ||
      (now_us - g_last_reinit_attempt_us) < app_config::kReinitRetryIntervalUs) {
    return;
  }
  g_last_reinit_attempt_us = now_us;
  ++g_reinit_attempts;
  g_diagnostics.noteReinitialization(reason);
  Serial.printf("Reinitialization attempt %u/%u: %s\n", g_reinit_attempts,
                app_config::kMaxReinitAttempts, reason);

  g_bno.beginI2c();
  const uint8_t address = g_bno.scanAndSelectAddress();
  if (address != 0 && g_bno.initialize(address, g_diagnostics)) {
    g_diagnostics.startMonitoring(micros());
    g_reinit_attempts = 0;
    Serial.println("Reinitialization succeeded.");
  } else {
    Serial.println("Reinitialization failed; retry is rate-limited and capped.");
  }
}
}  // namespace

void setup() {
  Serial.begin(921600);
  const uint32_t serial_wait_start = millis();
  while (!Serial && (millis() - serial_wait_start) < 1500UL) {
    delay(1);
  }
  printStartupBanner();
  g_web_ui.begin(g_diagnostics);

  g_bno.beginI2c();
  initializeDetectedBno();
}

void loop() {
  uint32_t now_us = micros();
  if (g_bno.initialized()) {
    // Adafruit_BNO08x exposes getSensorEvent() as a polling reader.  Do not
    // make the I2C transfer conditional on INT; each event is still classified
    // by sensorId inside Bno08xReader::service().
    g_bno.service(g_diagnostics, app_config::kMaxEventsPerLoop);

    uint8_t reset_reason = 0;
    if (g_bno.consumeResetEvent(reset_reason)) {
      static char reason[32];
      snprintf(reason, sizeof(reason), "BNO reset event reason=%u", reset_reason);
      attemptReinitialization(reason);
    } else if (g_diagnostics.hasTimedOut(now_us)) {
      attemptReinitialization(g_diagnostics.firstTimedOutStream(now_us));
    }
  }

  // Refresh after I2C servicing so age calculations use a timestamp no older
  // than the received sample timestamps.
  now_us = micros();

  // HTTP only serves the cached Diagnostics state; it never accesses I2C.
  g_web_ui.handleClient();

  if ((now_us - g_last_csv_us) >= app_config::kCsvIntervalUs) {
    g_last_csv_us = now_us;
    if (g_bno.initialized()) {
      g_diagnostics.printCsv(now_us);
    }
  }
  if ((now_us - g_last_diagnostic_us) >= app_config::kDiagnosticIntervalUs) {
    g_last_diagnostic_us = now_us;
    g_diagnostics.printSummary(now_us);
    g_web_ui.printStatus();
  }
}