#pragma once

#include <Adafruit_BNO08x.h>

#include "app_config.h"
#include "diagnostics.h"

class Bno08xReader {
 public:
  void beginI2c();
  uint8_t scanAndSelectAddress() const;
  bool initialize(uint8_t address, Diagnostics& diagnostics);
  uint8_t service(Diagnostics& diagnostics, uint8_t max_events);
  bool consumeResetEvent(uint8_t& reset_reason);
  bool initialized() const { return initialized_; }
  uint8_t address() const { return address_; }

 private:
  void pulseResetPin() const;
  bool configureReports();

  // Diagnostic mode: do not let the library toggle the BNO08X RST pin.
  Adafruit_BNO08x bno_{};
  uint8_t address_ = 0;
  bool initialized_ = false;
  bool reset_event_pending_ = false;
  uint8_t reset_reason_ = 0;
};