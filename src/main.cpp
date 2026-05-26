#include <Arduino.h>
#include <WiFi.h>

#include "cfg.h"
#include "storage.h"
#include "web.h"
#include "radio_cc1101.h"

static AppConfig gCfg;
static WebUI gWeb;
static RadioCC1101 gRadio;

static void startWiFi() {
  WiFi.mode(WIFI_STA);

  if (gCfg.wifiSsid.length() > 0) {
    WiFi.begin(gCfg.wifiSsid.c_str(), gCfg.wifiPass.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
      delay(200);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi STA OK: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi STA failed -> starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!Storage::begin()) {
    Serial.println("LittleFS mount failed");
  }

  Storage::loadConfig(gCfg);

  startWiFi();

  bool radioOk = gRadio.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);
  Serial.printf("CC1101: %s\n", radioOk ? "OK" : "NOT DETECTED");

  gWeb.begin(&gCfg, &gRadio);

  Serial.println("Open UI:");
  Serial.print("http://");
  Serial.print((WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP());
  Serial.print("/?t=");
  Serial.println(UI_TOKEN);
  Serial.println("If MAC whitelist enabled, send X-Client-MAC header from your client.");
}

void loop() {
  delay(50);
}