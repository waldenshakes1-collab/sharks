#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

struct AppConfig {
  String wifiSsid;
  String wifiPass;

  // Whitelist MAC (format: "AA:BB:CC:DD:EE:FF")
  std::vector<String> macWhitelist;

  // Jeśli true: wymuszamy token
  bool requireToken = true;
};

namespace Storage {
  bool begin();
  bool ensureLayout();

  bool loadConfig(AppConfig& cfg);
  bool saveConfig(const AppConfig& cfg);

  bool listDirJson(const char* path, JsonArray out);
  bool deletePath(const char* path);

  bool readFileToString(const char* path, String& out);
  bool writeStringToFile(const char* path, const String& data);

  bool exists(const char* path);
}