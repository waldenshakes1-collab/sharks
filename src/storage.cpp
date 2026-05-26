#include "storage.h"
#include "cfg.h"

namespace Storage {

bool begin() {
  if (!LittleFS.begin(true)) return false;
  return ensureLayout();
}

bool ensureLayout() {
  if (!LittleFS.exists(SCRIPTS_DIR)) {
    LittleFS.mkdir(SCRIPTS_DIR);
  }
  if (!LittleFS.exists(CONFIG_FILE)) {
    AppConfig cfg;
    cfg.wifiSsid = "";
    cfg.wifiPass = "";
    cfg.requireToken = true;
    saveConfig(cfg);
  }
  return true;
}

static void macListToJson(const std::vector<String>& v, JsonArray arr) {
  for (auto& s : v) arr.add(s);
}
static void macListFromJson(std::vector<String>& v, JsonArray arr) {
  v.clear();
  for (JsonVariant x : arr) v.push_back(x.as<String>());
}

bool loadConfig(AppConfig& cfg) {
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  cfg.wifiSsid = doc["wifi"]["ssid"] | "";
  cfg.wifiPass = doc["wifi"]["pass"] | "";
  cfg.requireToken = doc["security"]["requireToken"] | true;

  if (doc["security"]["macWhitelist"].is<JsonArray>()) {
    macListFromJson(cfg.macWhitelist, doc["security"]["macWhitelist"].as<JsonArray>());
  } else {
    cfg.macWhitelist.clear();
  }
  return true;
}

bool saveConfig(const AppConfig& cfg) {
  JsonDocument doc;
  doc["wifi"]["ssid"] = cfg.wifiSsid;
  doc["wifi"]["pass"] = cfg.wifiPass;
  doc["security"]["requireToken"] = cfg.requireToken;

  JsonArray arr = doc["security"]["macWhitelist"].to<JsonArray>();
  macListToJson(cfg.macWhitelist, arr);

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  bool ok = (serializeJsonPretty(doc, f) > 0);
  f.close();
  return ok;
}

bool listDirJson(const char* path, JsonArray out) {
  File root = LittleFS.open(path, "r");
  if (!root || !root.isDirectory()) return false;

  File file = root.openNextFile();
  while (file) {
    JsonObject o = out.add<JsonObject>();
    o["name"] = String(file.name());
    o["size"] = (uint32_t)file.size();
    o["isDir"] = file.isDirectory();
    file = root.openNextFile();
  }
  return true;
}

bool deletePath(const char* path) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (f && f.isDirectory()) {
    File child = f.openNextFile();
    while (child) {
      String p = String(path) + "/" + String(child.name()).substring(String(path).length()+1);
      child = f.openNextFile();
      deletePath(p.c_str());
    }
    f.close();
    return LittleFS.rmdir(path);
  }
  f.close();
  return LittleFS.remove(path);
}

bool readFileToString(const char* path, String& out) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

bool writeStringToFile(const char* path, const String& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data);
  f.close();
  return true;
}

bool exists(const char* path) {
  return LittleFS.exists(path);
}

} // namespace Storage