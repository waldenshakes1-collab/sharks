#include "web.h"
#include "cfg.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static String normalizeMac(String s) {
  s.trim(); s.toUpperCase();
  return s;
}

bool WebUI::tokenOk(AsyncWebServerRequest* req) {
  if (!_cfg->requireToken) return true;
  if (req->hasParam("t")) {
    String t = req->getParam("t")->value();
    return t == UI_TOKEN;
  }
  // allow token in header
  if (req->hasHeader("X-Token")) {
    return req->getHeader("X-Token")->value() == UI_TOKEN;
  }
  return false;
}

bool WebUI::checkAuth(AsyncWebServerRequest* req) {
  if (!tokenOk(req)) return false;

  // MAC whitelist: in AP mode we can usually resolve station list by esp_wifi APIs
  // Here we keep it simple: if whitelist empty -> allow.
  if (_cfg->macWhitelist.empty()) return true;

  // Basic heuristic: when in AP mode, require client to pass its MAC in header
  // (JS can’t read it automatically; user can store it once in settings).
  // This is not cryptographic security, but acts as "only my device".
  if (req->hasHeader("X-Client-MAC")) {
    String mac = normalizeMac(req->getHeader("X-Client-MAC")->value());
    for (auto& m : _cfg->macWhitelist) {
      if (normalizeMac(m) == mac) return true;
    }
    return false;
  }

  // If missing -> deny when whitelist non-empty
  return false;
}

void WebUI::routesStatic() {
  _server.on("/", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->requestAuthentication();
    req->send(LittleFS, "/index.html", "text/html");
  });

  _server.serveStatic("/app.css", LittleFS, "/app.css");
  _server.serveStatic("/app.js", LittleFS, "/app.js");
}

void WebUI::routesApi() {
  _server.on("/api/ping", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");
    req->send(200, "application/json", R"({"ok":true})");
  });

  _server.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");
    JsonDocument doc;
    doc["wifi"]["ssid"] = _cfg->wifiSsid;
    doc["security"]["requireToken"] = _cfg->requireToken;
    JsonArray arr = doc["security"]["macWhitelist"].to<JsonArray>();
    for (auto& m : _cfg->macWhitelist) arr.add(m);

    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  _server.on("/api/config", HTTP_POST, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");
    req->send(200, "application/json", R"({"ok":true})");
  }, nullptr, [&](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) return;

    if (doc["wifi"]["ssid"].is<String>()) _cfg->wifiSsid = doc["wifi"]["ssid"].as<String>();
    if (doc["wifi"]["pass"].is<String>()) _cfg->wifiPass = doc["wifi"]["pass"].as<String>();
    if (doc["security"]["requireToken"].is<bool>()) _cfg->requireToken = doc["security"]["requireToken"].as<bool>();

    if (doc["security"]["macWhitelist"].is<JsonArray>()) {
      _cfg->macWhitelist.clear();
      for (JsonVariant v : doc["security"]["macWhitelist"].as<JsonArray>()) {
        _cfg->macWhitelist.push_back(v.as<String>());
      }
    }

    Storage::saveConfig(*_cfg);
  });

  // list scripts
  _server.on("/api/scripts", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");

    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    Storage::listDirJson(SCRIPTS_DIR, arr);

    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // delete script file
  _server.on("/api/scripts/delete", HTTP_POST, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");
    if (!req->hasParam("path", true)) return req->send(400, "application/json", R"({"ok":false,"err":"missing path"})");
    String path = req->getParam("path", true)->value();
    if (!path.startsWith(String(SCRIPTS_DIR) + "/")) return req->send(400, "application/json", R"({"ok":false,"err":"bad path"})");

    bool ok = Storage::deletePath(path.c_str());
    req->send(200, "application/json", ok ? R"({"ok":true})" : R"({"ok":false})");
  });

  // upload script file (multipart)
  _server.on("/api/scripts/upload", HTTP_POST, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");
    req->send(200, "application/json", R"({"ok":true})");
  }, [&](AsyncWebServerRequest* req, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!checkAuth(req)) return;

    if (!filename.startsWith("/")) filename = "/" + filename;
    String path = String(SCRIPTS_DIR) + filename;

    if (index == 0) {
      req->_tempFile = LittleFS.open(path, "w");
    }
    if (req->_tempFile) {
      req->_tempFile.write(data, len);
    }
    if (final) {
      if (req->_tempFile) req->_tempFile.close();
    }
  });

  // download script file
  _server.on("/api/scripts/download", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "text/plain", "forbidden");
    if (!req->hasParam("path")) return req->send(400, "text/plain", "missing path");
    String path = req->getParam("path")->value();
    if (!path.startsWith(String(SCRIPTS_DIR) + "/")) return req->send(400, "text/plain", "bad path");
    if (!LittleFS.exists(path)) return req->send(404, "text/plain", "not found");
    req->send(LittleFS, path, "application/octet-stream");
  });

  // radio scan endpoint (simple RSSI sweep)
  _server.on("/api/radio/sweep", HTTP_GET, [&](AsyncWebServerRequest* req){
    if (!checkAuth(req)) return req->send(403, "application/json", R"({"ok":false})");

    uint32_t startHz = req->hasParam("start") ? req->getParam("start")->value().toInt() : 433050000;
    uint32_t stopHz  = req->hasParam("stop")  ? req->getParam("stop")->value().toInt()  : 434790000;
    uint32_t stepHz  = req->hasParam("step")  ? req->getParam("step")->value().toInt()  : 100000;

    JsonDocument doc;
    JsonArray arr = doc["points"].to<JsonArray>();

    _radio->sweep(startHz, stopHz, stepHz, [&](uint32_t f, float rssi){
      JsonObject p = arr.add<JsonObject>();
      p["f"] = f;
      p["r"] = rssi;
    });

    doc["ok"] = true;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });
}

void WebUI::begin(AppConfig* cfg, RadioCC1101* radio) {
  _cfg = cfg;
  _radio = radio;

  routesStatic();
  routesApi();

  _server.onNotFound([&](AsyncWebServerRequest* req){
    req->send(404, "text/plain", "Not found");
  });

  _server.begin();
}