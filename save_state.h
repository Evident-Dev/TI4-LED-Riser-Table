#pragma once
#include <Preferences.h>
#include "config.h"
#include "game_state.h"
#include "web_server.h"

// =============================================================================
// TI4 Hex Riser - Power Loss Recovery
// =============================================================================
// Keeps a copy of the running game in NVS flash so a game survives a power cut.
// An NVS write either lands whole or leaves the previous save intact, so a cut
// mid-write can't corrupt it.
//
//   Saving:   once the game has started, updateSaveState() compares the game
//             with the last save a few times a second and writes on any change
//             (turn ends, passes, card locks, claims, phase changes).
//   Boot:     initSaveState() loads a save if one exists and holds the game in
//             recovery until someone resumes or the admin starts a new game.
//   Clearing: a new setup, a reset, or Start New Game wipes the save.
//
// Seats come back reserved: the phone holding the matching token rejoins, and
// anyone else can still take the seat.
// =============================================================================

#define SAVE_FORMAT_VERSION  2        // bump when SavedGame layout changes
#define SAVE_CHECK_MS        250

struct SavedGame {
  uint32_t    formatVersion;
  uint32_t    gameIdentifier;
  Player      players[MAX_PLAYERS];
  GameState   gameState;
  GameOptions gameOptions;
  int8_t      hexOwner[NUM_HEXES];
  Seat        seats[MAX_PLAYERS];
};

static Preferences saveStorage;
static SavedGame   pendingSavedGame;   // loaded at boot, waiting for resume or discard
static SavedGame   lastWrittenGame;    // what's on flash now, to skip identical writes
static bool        recoveryPending = false;

bool    isRecoveryPending()     { return recoveryPending; }
uint8_t savedGamePhase()        { return (uint8_t)pendingSavedGame.gameState.currentPhase; }
uint8_t savedGamePlayerCount()  { return pendingSavedGame.gameState.numActivePlayers; }

// Copies the live game into snapshot. Timers are left out so they don't
// count as changes, and claimed seats are stored as reserved.
static void buildSnapshot(SavedGame& snapshot) {
  memset(&snapshot, 0, sizeof(snapshot));
  snapshot.formatVersion  = SAVE_FORMAT_VERSION;
  snapshot.gameIdentifier = gameIdentifier;
  memcpy(snapshot.players, players, sizeof(players));
  for (uint8_t playerIndex = 0; playerIndex < MAX_PLAYERS; playerIndex++) {
    snapshot.players[playerIndex].turnStartMs = 0;
  }
  snapshot.gameState   = gameState;
  snapshot.gameOptions = gameOpts;
  memcpy(snapshot.hexOwner, hexOwner, sizeof(hexOwner));
  for (uint8_t seat = 0; seat < MAX_PLAYERS; seat++) {
    if (!seats[seat].claimed && !seats[seat].reserved) continue;
    snapshot.seats[seat]          = seats[seat];
    snapshot.seats[seat].claimed  = false;
    snapshot.seats[seat].reserved = true;
  }
}

void clearSavedGame() {
  saveStorage.remove("game");
  memset(&lastWrittenGame, 0, sizeof(lastWrittenGame));
  recoveryPending = false;
  if (rtCfg.debugSerial) Serial.println(F("Save: cleared"));
}

// Call once in setup() before the game starts.
void initSaveState() {
  saveStorage.begin("ti4save", false);
  if (saveStorage.getBytesLength("game") != sizeof(SavedGame)) return;
  saveStorage.getBytes("game", &pendingSavedGame, sizeof(SavedGame));
  if (pendingSavedGame.formatVersion != SAVE_FORMAT_VERSION) {
    clearSavedGame();
    return;
  }
  lastWrittenGame = pendingSavedGame;
  recoveryPending = true;
  if (rtCfg.debugSerial) Serial.println(F("Save: saved game found, waiting for resume"));
}

// Call every loop(). Writes the game to flash whenever it changes.
void updateSaveState() {
  if (!bootComplete || recoveryPending) return;
  if (gameState.currentPhase == PHASE_SETUP) return;  // only games that have started

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < SAVE_CHECK_MS) return;
  lastCheck = millis();

  static SavedGame snapshot;
  buildSnapshot(snapshot);
  if (memcmp(&snapshot, &lastWrittenGame, sizeof(SavedGame)) == 0) return;

  saveStorage.putBytes("game", &snapshot, sizeof(SavedGame));
  lastWrittenGame = snapshot;
  if (rtCfg.debugSerial) Serial.println(F("Save: game saved"));
}

// Restores the saved game. Seats a phone already reclaimed stay with it.
void resumeSavedGame() {
  if (!recoveryPending) return;

  memcpy(players, pendingSavedGame.players, sizeof(players));
  gameState      = pendingSavedGame.gameState;
  gameOpts       = pendingSavedGame.gameOptions;
  gameIdentifier = pendingSavedGame.gameIdentifier;
  memcpy(hexOwner, pendingSavedGame.hexOwner, sizeof(hexOwner));
  for (uint8_t seat = 0; seat < MAX_PLAYERS; seat++) {
    bool sameHolder = seats[seat].claimed && seats[seat].token == pendingSavedGame.seats[seat].token;
    if (!sameHolder) seats[seat] = pendingSavedGame.seats[seat];
  }

  gameState.inBattle = false;
  battlePending      = false;
  if (gameState.currentPhase == PHASE_ACTION && gameState.actionOrderSize > 0) {
    players[gameState.actionOrder[gameState.currentActionIndex]].turnStartMs = millis();
  }

  setAllHexes(CRGB::Black);  // the phase display redraws everything next loop
  recoveryPending = false;
  if (rtCfg.debugSerial) Serial.println(F("Save: game resumed"));
}

// Admin chose Start New Game: drop the save and begin a fresh setup.
void discardSavedGame() {
  clearSavedGame();
  startNewGameIdentity();
  transitionToSetup();
}
