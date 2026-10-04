#pragma once
#include <Preferences.h>
#include "config.h"

// =============================================================================
// TI4 Hex Riser - Map Tiles
// =============================================================================
// Which TI4 system tile sits on each hex, by tile number (for example "18",
// "83A", "4215"), plus a rotation in 60 degree steps for hyperlanes. Only the
// numbers live here; pages load the tile images from the CDN.
//
// The admin page edits the map during setup. It's saved to flash a second
// after the last change, so a map survives reboots until it's cleared.
// =============================================================================

#define MAP_TILE_LENGTH      8      // longest tile number plus the terminator
#define MAP_FORMAT_VERSION   1      // bump when the stored map layout changes
#define MAP_SAVE_DELAY_MS    1000

struct MapState {
  char    tile[NUM_HEXES][MAP_TILE_LENGTH];  // "" = no tile
  uint8_t rotation[NUM_HEXES];               // 0-5, clockwise
};

static MapState          mapState = {};
static volatile uint32_t mapRevision = 1;     // changes on every edit so pages can refresh
static uint32_t          mapSavedRevision = 1;
static uint32_t          mapChangedMs = 0;
static Preferences       mapStorage;

// Tile numbers are letters, digits and underscores only
static bool isValidTileNumber(const char* tileNumber) {
  size_t length = strlen(tileNumber);
  if (length >= MAP_TILE_LENGTH) return false;
  for (size_t index = 0; index < length; index++) {
    char character = tileNumber[index];
    if (!isalnum((unsigned char)character) && character != '_') return false;
  }
  return true;
}

// Sets one hex. An empty tile number clears it.
bool setMapTile(int hexIndex, const char* tileNumber, uint8_t rotation) {
  if (hexIndex < 0 || hexIndex >= NUM_HEXES) return false;
  if (!isValidTileNumber(tileNumber)) return false;
  strncpy(mapState.tile[hexIndex], tileNumber, MAP_TILE_LENGTH - 1);
  mapState.tile[hexIndex][MAP_TILE_LENGTH - 1] = 0;
  mapState.rotation[hexIndex] = tileNumber[0] ? rotation % 6 : 0;
  mapRevision++;
  mapChangedMs = millis();
  return true;
}

void clearMap() {
  memset(&mapState, 0, sizeof(mapState));
  mapRevision++;
  mapChangedMs = millis();
}

// Call once in setup()
void initMapState() {
  if (!mapStorage.begin("ti4map", false)) return;
  if (mapStorage.getUChar("version", 0) == MAP_FORMAT_VERSION &&
      mapStorage.getBytesLength("map") == sizeof(MapState)) {
    mapStorage.getBytes("map", &mapState, sizeof(MapState));
  }
}

// Call every loop(). Saves the map once edits have settled.
void updateMapStorage() {
  if (mapRevision == mapSavedRevision) return;
  if (millis() - mapChangedMs < MAP_SAVE_DELAY_MS) return;
  mapSavedRevision = mapRevision;
  mapStorage.putBytes("map", &mapState, sizeof(MapState));
  mapStorage.putUChar("version", MAP_FORMAT_VERSION);
  if (rtCfg.debugSerial) Serial.println(F("Map: saved"));
}

// {"t":"map","tiles":["18","",...],"rot":"000200..."}
int buildMapJson(char* buffer, size_t bufferLength) {
  int position = snprintf(buffer, bufferLength, "{\"t\":\"map\",\"tiles\":[");
  for (int hexIndex = 0; hexIndex < NUM_HEXES && position < (int)bufferLength - 1; hexIndex++) {
    position += snprintf(buffer + position, bufferLength - position, "%s\"%s\"",
                         hexIndex > 0 ? "," : "", mapState.tile[hexIndex]);
  }
  position += snprintf(buffer + position, bufferLength - position, "],\"rot\":\"");
  for (int hexIndex = 0; hexIndex < NUM_HEXES && position < (int)bufferLength - 1; hexIndex++) {
    buffer[position++] = '0' + mapState.rotation[hexIndex];
  }
  position += snprintf(buffer + position, bufferLength - position, "\"}");
  return position;
}
