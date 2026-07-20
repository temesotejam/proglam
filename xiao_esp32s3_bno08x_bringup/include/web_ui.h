#pragma once

#include <WebServer.h>

#include "app_config.h"
#include "diagnostics.h"

class WebUi {
 public:
  void begin(const Diagnostics& diagnostics);
  void handleClient();
  void printStatus() const;
  bool started() const { return started_; }

 private:
  void handleRoot();
  void handleLatest();
  void handleNotFound();
  static uint32_t ageUs(bool valid, uint32_t last_rx_us, uint32_t now_us);

  WebServer server_{app_config::kWebHttpPort};
  const Diagnostics* diagnostics_ = nullptr;
  bool started_ = false;
};
