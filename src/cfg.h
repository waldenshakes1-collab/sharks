#pragma once
#include <Arduino.h>

// --- CC1101 pins (sparse "co 2")
static constexpr int PIN_SCK  = 4;
static constexpr int PIN_MOSI = 6;
static constexpr int PIN_MISO = 8;
static constexpr int PIN_CSN  = 10;
static constexpr int PIN_GDO0 = 12;
static constexpr int PIN_GDO2 = 14;

// --- WiFi AP fallback
static constexpr const char* AP_SSID = "ESP32-FlipperUI";
static constexpr const char* AP_PASS = "12345678";

// --- Access token for UI/API
// W UI dodawany jako ?t=TOKEN
static constexpr const char* UI_TOKEN = "change-me-very-long-token";

// --- FS paths
static constexpr const char* CONFIG_FILE = "/config.json";
static constexpr const char* SCRIPTS_DIR = "/scripts";