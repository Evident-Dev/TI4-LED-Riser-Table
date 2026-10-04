#pragma once
#include <Preferences.h>
#include "config.h"

// =============================================================================
// TI4 Hex Riser - Runtime Settings
// =============================================================================
// Shadows config.h #defines with a live-mutable struct so the web settings
// page can change values at runtime.  Include this BEFORE led_control.h,
// keyboard_control.h, and network.h.
//
// All code that previously read a #define constant should instead read
// the matching rtCfg field.  The struct is initialized from the #defines
// in config.h, so the defaults always come from there.
// =============================================================================

struct RuntimeConfig {
  // WiFi
  char     homeSSID[64];
  char     homePass[64];
  char     apSSID[32];
  char     apPass[32];
  uint32_t homeTimeoutMs;

  // LED
  uint8_t  defaultBrightness;
  uint8_t  maxBrightness;
  uint16_t ledUpdateMs;
  uint16_t broadcastMs;
  uint8_t  sideGap;             // SVG side line inset (browser-only, served via /getsettings)

  // Debug / simulation
  bool     simulateHardware;
  bool     debugSerial;
  bool     debugWeb;
  bool     debugLed;
  bool     debugKeyboard;

  // Added later: new fields go at the end so older saves still load
  bool     thinSides;           // thinner LED side lines in the browser
  char     hostname[32];        // name.local on the network
  bool     keepAccessPoint;     // keep the board's own WiFi on after joining a network
};

// One global instance — initialized from config.h defaults at boot.
// network.h savesettings route writes into this struct; changes take
// effect immediately (some, like WiFi credentials, require a reboot).
RuntimeConfig rtCfg = {
  WIFI_HOME_SSID,
  WIFI_HOME_PASSWORD,
  WIFI_AP_SSID,
  WIFI_AP_PASSWORD,
  WIFI_HOME_TIMEOUT_MS,
  DEFAULT_BRIGHTNESS,
  MAX_BRIGHTNESS,
  LED_UPDATE_MS,
  BROADCAST_MS,
  SIDE_GAP,
  SIMULATE_HARDWARE,
  DEBUG_SERIAL,
  DEBUG_WEB_TEST,
  DEBUG_LED_TEST,
  DEBUG_KEYBOARD_TEST,
  THIN_SIDES,
  NETWORK_HOSTNAME,
  KEEP_ACCESS_POINT
};

// Bump when RuntimeConfig changes so old saved bytes are ignored
#define SETTINGS_FORMAT_VERSION 1

static Preferences settingsStorage;

// Loads saved settings over the config.h defaults. Call first in setup().
void loadRuntimeSettings() {
  if (!settingsStorage.begin("ti4settings", true)) return;  // nothing saved yet
  // A shorter save is from before fields were added; those keep their defaults
  size_t savedLength = settingsStorage.getBytesLength("config");
  if (settingsStorage.getUChar("version", 0) == SETTINGS_FORMAT_VERSION &&
      savedLength > 0 && savedLength <= sizeof(RuntimeConfig)) {
    settingsStorage.getBytes("config", &rtCfg, savedLength);
  }
  settingsStorage.end();
}

void saveRuntimeSettings() {
  settingsStorage.begin("ti4settings", false);
  settingsStorage.putBytes("config", &rtCfg, sizeof(RuntimeConfig));
  settingsStorage.putUChar("version", SETTINGS_FORMAT_VERSION);
  settingsStorage.end();
}
