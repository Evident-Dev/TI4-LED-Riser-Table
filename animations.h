#pragma once
#include "config.h"
#include "led_control.h"

// =============================================================================
// TI4 Hex Riser - Game Animations
// =============================================================================
// Boot sequence and phase-transition animations.
// Per-phase idle animations (join fade, picker pulse, etc.) live in game_state.h.
// =============================================================================

// network.h is included after this file, so forward-declare what we need.
void handleNetwork();

// -----------------------------------------------------------------------------
// animDelay() — replaces delay() inside animation code.
// handleNetwork() is a no-op with AsyncWebServer (handled in background),
// but kept here so the call site is consistent and easy to extend.
// -----------------------------------------------------------------------------
static void animDelay(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    handleNetwork();
  }
}

// -----------------------------------------------------------------------------
// Boot animation: white snake crawls through all 61 hexes in order.
// Only BOOT_ANIM_TAIL hexes are lit at a time; tail turns off as head advances.
// The LED task pushes the frames, so this must run after it is started.
// -----------------------------------------------------------------------------
void runBootAnimation() {
  if (rtCfg.debugSerial) Serial.println(F("Boot: running snake animation"));

  for (int i = 0; i < NUM_HEXES + BOOT_ANIM_TAIL; i++) {
    if (i < NUM_HEXES) {
      setHexColor(i, CRGB::White);
    }
    if (i >= BOOT_ANIM_TAIL) {
      setHexColor(i - BOOT_ANIM_TAIL, CRGB::Black);
    }
    animDelay(BOOT_ANIM_SPEED_MS);
  }

  setAllHexes(CRGB::Black);

  if (rtCfg.debugSerial) Serial.println(F("Boot: animation complete"));
}

// -----------------------------------------------------------------------------
// Phase transition ripple: one sweep across the 5 hex rings.
// Forward (center -> edge) when the game moves to the next phase; reverse
// (edge -> center) when a new round starts and play returns to strategy.
// Blocking on Core 1; the LED task on Core 0 keeps pushing frames.
// -----------------------------------------------------------------------------
void runRippleTransition(bool reverse) {
  setAllHexes(CRGB::Black);
  pushLEDs();

  for (int step = 0; step < 5; step++) {
    int ring = reverse ? 4 - step : step;
    CRGB color = CHSV(160 + ring * 18, 200, 230);

    for (int e = 0; e < 24; e++) {
      uint8_t h = RIPPLE_RINGS[ring][e];
      if (h == 255) break;
      setHexColor(h, color);
    }
    pushLEDs();
    animDelay(150);

    // Dim the ring instead of clearing so the sweep leaves a trail
    CRGB trail = color;
    trail.nscale8(70);
    for (int e = 0; e < 24; e++) {
      uint8_t h = RIPPLE_RINGS[ring][e];
      if (h == 255) break;
      setHexColor(h, trail);
    }
  }

  animDelay(250);
  setAllHexes(CRGB::Black);
  pushLEDs();
}
