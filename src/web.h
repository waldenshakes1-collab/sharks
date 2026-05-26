#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "storage.h"
#include "radio_cc1101.h"

class WebUI {
public:
  void begin(AppConfig* cfg, RadioCC1101* radio);

private:
  AsyncWebServer _server{80};
  AppConfig* _cfg = nullptr;
  RadioCC1101* _radio = nullptr;

  bool checkAuth(AsyncWebServerRequest* req);
  bool tokenOk(AsyncWebServerRequest* req);

  void routesStatic();
  void routesApi();
};