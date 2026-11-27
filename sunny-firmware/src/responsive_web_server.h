#pragma once

#include <WebServer.h>

// Arduino ESP32 WebServer handles one client at a time. Preserve its normal
// timeout unless an idle connection is holding up another client.
class ResponsiveWebServer : public WebServer {
 public:
  explicit ResponsiveWebServer(int port) : WebServer(port) {}

  void handleClient() override {
    if (_currentStatus == HC_WAIT_READ && (uint32_t)(millis() - _statusChange) >= 1500 &&
        _currentClient.connected() && !_currentClient.available() &&
        _server.hasClient() && !_currentClient.available()) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
      _currentUpload.reset();
      _currentRaw.reset();
    }
    WebServer::handleClient();
  }

};
