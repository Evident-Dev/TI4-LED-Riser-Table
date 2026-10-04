#pragma once
#include <math.h>
#include "config.h"
#include "led_control.h"
#include "hex_neighbors.h"

// =============================================================================
// TI4 Hex Riser - Game State Machine
// =============================================================================
// Reads top-to-bottom following the natural game flow:
//   Boot → Setup (join) → Strategy → Action → Status → Agenda → loop
//
// Key serial commands for testing (handled in TI4_HexRiser.ino):
//   kb <1-8> <0-15>   simulate a keyboard key press
//   setplayers <N>    set number of active players (4-8) for stub testing
//   phase <0-4>       force-jump to a phase
//   startgame         trigger GM start (skip waiting for all locks)
// =============================================================================

// =============================================================================
// SECTION 1: Player Array and Game State Data
// =============================================================================

Player    players[MAX_PLAYERS];

// Rulebook options, adjustable from the admin page.
struct GameOptions {
  bool agendaAfterCustodians;  // skip agenda phase until the custodians token is taken
  bool doubleCardsFor4P;       // 3-4 player rule: each player picks 2 strategy cards
  bool custodiansTaken;        // set when Mecatol Rex (hex 30) is claimed, or by admin
};
GameOptions gameOpts = { true, true, false };

struct GameState {
  GamePhase currentPhase;
  uint8_t   speakerIndex;         // index into players[] (0-7) of current speaker
  uint8_t   numActivePlayers;     // how many keyboards are active this game

  // Color selection tracking (setup phase)
  bool      colorTaken[8];        // true if COLOR_PALETTE[i] is locked by a player

  // Strategy phase
  uint8_t   strategyPickOrder[8]; // player indices in pick order (speaker first)
  uint8_t   currentPickIndex;     // index into strategyPickOrder
  uint8_t   totalPicks;           // entries in strategyPickOrder (2x players in 4P games)

  // Action phase
  uint8_t   actionOrder[8];       // player indices sorted by initiative
  uint8_t   actionOrderSize;      // number of players still in action order
  uint8_t   currentActionIndex;   // index into actionOrder

  // Battle mode
  bool      inBattle;
  uint8_t   battleAttacker;
  uint8_t   battleDefender;

  // Status phase
  uint8_t   hexSliceOwner[NUM_HEXES]; // which player index owns each hex in status
};

GameState gameState;

// =============================================================================
// SECTION 2: Grid Geometry Helpers
// =============================================================================
// Used to compute hex positions for pizza-slice assignment (status phase)
// and proximity-based board split (battle mode).

// Column layout — mirrors board_script.h
static const uint8_t GRID_COL_COUNT[9]    = { 5, 6, 7, 8, 9, 8, 7, 6, 5 };
static const uint8_t GRID_COL_START[9]    = { 0, 5, 11, 18, 26, 35, 43, 50, 56 };
static const bool    GRID_COL_TOP_START[9] = { true, false, true, false, true, false, true, false, true };

// Returns the (x, y) position of hexIdx relative to the center hex (30).
// Uses normalized units where hexRadius = 1.
static void getHexPosition(uint8_t hexIdx, float &outX, float &outY) {
  uint8_t col = 0;
  for (uint8_t c = 0; c < 9; c++) {
    if (hexIdx >= GRID_COL_START[c] && hexIdx < GRID_COL_START[c] + GRID_COL_COUNT[c]) {
      col = c;
      break;
    }
  }
  uint8_t offset = hexIdx - GRID_COL_START[col];
  uint8_t rowFromTop = GRID_COL_TOP_START[col] ? offset : (GRID_COL_COUNT[col] - 1 - offset);

  const float hexH = 1.7320508f;  // sqrt(3), hex height when radius = 1
  outX = col * 1.5f;
  float colTopY = -(GRID_COL_COUNT[col] * hexH) / 2.0f + hexH / 2.0f;
  outY = colTopY + rowFromTop * hexH;

  // Subtract center hex (30) position: col=4, rowFromTop=4
  outX -= 4 * 1.5f;
  float centerColTopY = -(9 * hexH) / 2.0f + hexH / 2.0f;
  outY -= (centerColTopY + 4 * hexH);  // = 0, but written explicitly
}

// Returns angle (0–360°) from center hex to hexIdx. 0° = right, clockwise.
static float getAngleFromCenter(uint8_t hexIdx) {
  if (hexIdx == 30) return 0.0f;
  float x, y;
  getHexPosition(hexIdx, x, y);
  float angle = atan2f(y, x) * 180.0f / (float)M_PI;
  if (angle < 0) angle += 360.0f;
  return angle;
}

// BFS-based hex distance (uses HEX_NEIGHBORS table).
static uint8_t hexDistance(uint8_t from, uint8_t to) {
  if (from == to) return 0;
  uint8_t dist[NUM_HEXES];
  memset(dist, 0xFF, sizeof(dist));  // 0xFF = unvisited
  dist[from] = 0;
  uint8_t queue[NUM_HEXES];
  uint8_t head = 0, tail = 0;
  queue[tail++] = from;
  while (head < tail) {
    uint8_t current = queue[head++];
    if (current == to) return dist[to];
    for (int dir = 0; dir < 6; dir++) {
      int neighbor = HEX_NEIGHBORS[current][dir];
      if (neighbor >= 0 && dist[neighbor] == 0xFF) {
        dist[neighbor] = dist[current] + 1;
        queue[tail++] = (uint8_t)neighbor;
      }
    }
  }
  return dist[to];
}

// =============================================================================
// SECTION 3: Helper Utilities
// =============================================================================

// Returns the ordinal position (0, 1, 2, …) of playerIndex among active players.
static uint8_t getActivePositionOf(uint8_t playerIndex) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < playerIndex; i++) {
    if (players[i].active) count++;
  }
  return count;
}

// Returns the playerIndex of the Nth active player (0-based).
static uint8_t getActivePlayerByPosition(uint8_t position) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      if (count == position) return i;
      count++;
    }
  }
  return 0;
}

// Spreads a pulsed color outward from a hex to `depth` rings of neighbors.
// `visited` must be a zeroed NUM_HEXES-byte array on first call.
static void spreadPulseToNeighbors(uint8_t hexIdx, CRGB color, uint8_t depth, uint8_t visited[]) {
  if (depth == 0) return;
  for (int dir = 0; dir < 6; dir++) {
    int neighbor = HEX_NEIGHBORS[hexIdx][dir];
    if (neighbor < 0 || visited[neighbor]) continue;
    visited[neighbor] = 1;
    CRGB fadedColor = color;
    fadedColor.nscale8((uint8_t)((255UL * depth) / EDGE_PULSE_SPREAD));
    for (int side = 0; side < 6; side++) {
      setHexSideColor((uint8_t)neighbor, side, fadedColor);
    }
    spreadPulseToNeighbors((uint8_t)neighbor, color, depth - 1, visited);
  }
}

// =============================================================================
// SECTION 4: PHASE_SETUP — Player Join Mode
// =============================================================================

// Detects which keyboards are connected via I2C (stub: uses players[].active flags
// already set by the serial 'setplayers' command or defaults to 6 players).
static void detectConnectedKeyboards() {
  // Real hardware detection lives in keyboard_control.h (IMPLEMENT HERE marker).
  // In stub mode, active flags are set externally via serial 'setplayers N'.
  gameState.numActivePlayers = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) gameState.numActivePlayers++;
  }

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: "));
    Serial.print(gameState.numActivePlayers);
    Serial.println(F(" active players detected"));
  }
}

// Assigns home hexes to all active players based on how many are active.
static void assignHomeHexes() {
  if (gameState.numActivePlayers < 4) return;  // need at least 4 for a real layout
  uint8_t position = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      players[i].homeHex = getPlayerHomeHex(position, gameState.numActivePlayers);
      position++;
    }
  }
}

// Flashes a player's home hex red 3× to indicate a color is unavailable.
static void flashUnavailableColor(uint8_t playerIndex) {
  uint8_t homeHex = players[playerIndex].homeHex;
  for (int i = 0; i < 3; i++) {
    setHexColor(homeHex, CRGB::Red);
    pushLEDs();
    animDelay(100);
    setHexColor(homeHex, CRGB::Black);
    pushLEDs();
    animDelay(100);
  }
}

// Returns true if every active player has locked their color.
static bool allPlayersLocked() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].colorLocked) return false;
  }
  return true;
}

// Called every loop() during PHASE_SETUP to animate home hexes.
// Unlocked players: smooth breathing fade (JOIN_FADE_MIN → JOIN_FADE_MAX, 1s cycle).
// Locked players:   solid at full brightness.
static void updateJoinModeDisplay() {
  uint8_t fadeBrightness = beatsin8(60, JOIN_FADE_MIN, JOIN_FADE_MAX);

  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    // Inactive players keep a stale home hex that can overlap an active one
    if (!players[i].active) continue;
    CRGB playerColor;
    playerColor.r = (players[i].colorHex >> 16) & 0xFF;
    playerColor.g = (players[i].colorHex >>  8) & 0xFF;
    playerColor.b =  players[i].colorHex        & 0xFF;

    if (players[i].colorLocked) {
      setHexColor(players[i].homeHex, playerColor);
    } else {
      CRGB fadedColor = playerColor;
      fadedColor.nscale8(fadeBrightness);
      setHexColor(players[i].homeHex, fadedColor);
    }
  }
}

// Player presses key 1–8 to preview a color during PHASE_SETUP.
static void handleColorSelection(uint8_t playerIndex, uint8_t colorKey) {
  if (!players[playerIndex].active || players[playerIndex].colorLocked) return;
  uint8_t colorIndex = colorKey - 1;  // keys 1-8 → indices 0-7
  if (colorIndex >= 8) return;

  if (gameState.colorTaken[colorIndex]) {
    flashUnavailableColor(playerIndex);
    return;
  }
  players[playerIndex].selectedColorIndex = colorIndex;
  players[playerIndex].colorHex = COLOR_PALETTE[colorIndex];
}

// Player presses Key 15 to lock in their color during PHASE_SETUP.
static void handleColorLockIn(uint8_t playerIndex) {
  if (!players[playerIndex].active || players[playerIndex].colorLocked) return;
  uint8_t colorIndex = players[playerIndex].selectedColorIndex;

  if (gameState.colorTaken[colorIndex]) {
    flashUnavailableColor(playerIndex);
    return;
  }
  players[playerIndex].colorLocked = true;
  gameState.colorTaken[colorIndex] = true;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" locked color"));
  }

  // If any other unlocked player has the same color, bump them to a free color.
  // "Free" means not taken (locked) and not currently selected by any other unlocked player.
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (i == playerIndex || !players[i].active || players[i].colorLocked) continue;
    if (players[i].selectedColorIndex != colorIndex) continue;

    // Build a set of colors currently in use by unlocked players (excluding player i)
    bool colorInUse[8];
    for (uint8_t c = 0; c < 8; c++) colorInUse[c] = gameState.colorTaken[c];
    for (uint8_t j = 0; j < MAX_PLAYERS; j++) {
      if (j == i || !players[j].active || players[j].colorLocked) continue;
      colorInUse[players[j].selectedColorIndex] = true;
    }

    // Assign the first completely free color
    for (uint8_t c = 0; c < 8; c++) {
      if (!colorInUse[c]) {
        players[i].selectedColorIndex = c;
        players[i].colorHex           = COLOR_PALETTE[c];
        if (rtCfg.debugSerial) {
          Serial.print(F("Game: Player "));
          Serial.print(i + 1);
          Serial.print(F(" auto-reassigned to color "));
          Serial.println(c + 1);
        }
        break;
      }
    }
  }
}

// Selects a random speaker with a roulette spin around the home hexes.
// The gold light starts fast and eases to a stop on the winner, leaving a
// short fading trail, then the winner flashes. The LED task pushes frames.
static void selectRandomSpeaker() {
  uint8_t activePlayers[MAX_PLAYERS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) activePlayers[count++] = i;
  }
  if (count == 0) return;

  uint8_t winnerPos      = random8(count);
  gameState.speakerIndex = activePlayers[winnerPos];

  if (rtCfg.debugSerial) {
    Serial.print(F("Roulette: landing on P"));
    Serial.println(gameState.speakerIndex + 1);
  }

  static const CRGB gold = CRGB(0xFF, 0xD7, 0x00);
  const float trailLength = 1.6f;   // steps of fading light behind the head
  float totalSteps = (float)(SPEAKER_ROULETTE_LAPS * count + winnerPos);
  uint32_t spinStart = millis();

  for (;;) {
    float progress = (millis() - spinStart) / (float)SPEAKER_ROULETTE_MS;
    if (progress > 1.0f) progress = 1.0f;
    float eased    = 1.0f - powf(1.0f - progress, 3.0f);  // fast start, gentle stop
    float position = eased * totalSteps;

    for (uint8_t slot = 0; slot < count; slot++) {
      // How far the head has moved past this slot, wrapped around the table
      float behind = fmodf(position - slot, (float)count);
      if (behind < 0) behind += count;
      float ahead = count - behind;
      // Crossfade into the next hex, fade out more slowly behind for a trail
      float brightness = 0.0f;
      if (ahead < 1.0f)          brightness = 1.0f - ahead;
      if (behind < trailLength)  brightness = fmaxf(brightness, 1.0f - behind / trailLength);
      CRGB color = gold;
      color.nscale8((uint8_t)(brightness * 255));
      setHexColor(players[activePlayers[slot]].homeHex, color);
    }

    if (progress >= 1.0f) break;
    animDelay(16);
  }

  // Only the winner stays lit for the flash
  for (uint8_t slot = 0; slot < count; slot++) {
    if (slot != winnerPos) setHexColor(players[activePlayers[slot]].homeHex, CRGB::Black);
  }

  // Winner flash
  for (int flash = 0; flash < 3; flash++) {
    setHexColor(players[gameState.speakerIndex].homeHex, CRGB::Black);
    animDelay(120);
    setHexColor(players[gameState.speakerIndex].homeHex, gold);
    animDelay(120);
  }
  animDelay(600);

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Speaker is Player "));
    Serial.println(gameState.speakerIndex + 1);
  }

  // Restore all player home hex colors
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      CRGB color;
      color.r = (players[i].colorHex >> 16) & 0xFF;
      color.g = (players[i].colorHex >>  8) & 0xFF;
      color.b =  players[i].colorHex        & 0xFF;
      setHexColor(players[i].homeHex, color);
    }
  }
}

// =============================================================================
// SECTION 5: PHASE_STRATEGY — Strategy Card Selection
// =============================================================================

// Picks each player makes this round: 2 in a 4-player game (rulebook), else 1.
static uint8_t strategyPicksRequired() {
  return (gameOpts.doubleCardsFor4P && gameState.numActivePlayers <= 4) ? 2 : 1;
}

// Builds pick order: speaker first, then in seating order wrapping around.
// In 4-player games the whole order repeats for the second pick.
static void buildStrategyPickOrder() {
  gameState.currentPickIndex = 0;
  uint8_t slot = 0;

  for (uint8_t round = 0; round < strategyPicksRequired(); round++) {
    gameState.strategyPickOrder[slot++] = gameState.speakerIndex;
    for (uint8_t offset = 1; offset < gameState.numActivePlayers; offset++) {
      uint8_t candidatePosition = (getActivePositionOf(gameState.speakerIndex) + offset)
                                  % gameState.numActivePlayers;
      gameState.strategyPickOrder[slot++] = getActivePlayerByPosition(candidatePosition);
    }
  }
  gameState.totalPicks = slot;
}

// True if any player has locked in the given card this round.
static bool isCardLocked(uint8_t cardKey, uint8_t exceptPlayer = 0xFF) {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (i == exceptPlayer || !players[i].active) continue;
    if (players[i].strategyPicksDone >= 1 && players[i].strategyCard  == cardKey) return true;
    if (players[i].strategyPicksDone >= 2 && players[i].strategyCard2 == cardKey) return true;
  }
  return false;
}

// Colors a player's home hex and all its neighbors with the given color.
// Does NOT push — callers that need an immediate update must call pushLEDs() themselves.
static void colorPlayerArea(uint8_t playerIndex, CRGB color) {
  uint8_t homeHex = players[playerIndex].homeHex;
  setHexColor(homeHex, color);
  for (int dir = 0; dir < 6; dir++) {
    int neighbor = HEX_NEIGHBORS[homeHex][dir];
    if (neighbor >= 0) setHexColor((uint8_t)neighbor, color);
  }
}

// Called every loop() during PHASE_STRATEGY.
// Current picker: home hex pulses white (50%→100%, 1s cycle).
// Already picked: home hex + neighbors in strategy card color.
// Waiting:        home hex at player color, full brightness.
static void updateStrategyPickerPulse() {
  if (gameState.currentPickIndex >= gameState.totalPicks) return;
  uint8_t pulseBrightness = beatsin8(60, STRATEGY_PULSE_MIN, STRATEGY_PULSE_MAX);
  uint8_t currentPicker   = gameState.strategyPickOrder[gameState.currentPickIndex];

  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;

    if (i == currentPicker) {
      CRGB white = CRGB::White;
      white.nscale8(pulseBrightness);
      setHexColor(players[i].homeHex, white);
    } else if (players[i].strategyPicksDone > 0) {
      uint8_t cardIdx = players[i].strategyCard - 1;
      CRGB stratColor;
      stratColor.r = (STRATEGY_COLORS[cardIdx] >> 16) & 0xFF;
      stratColor.g = (STRATEGY_COLORS[cardIdx] >>  8) & 0xFF;
      stratColor.b =  STRATEGY_COLORS[cardIdx]        & 0xFF;
      colorPlayerArea(i, stratColor);
    } else {
      CRGB playerColor;
      playerColor.r = (players[i].colorHex >> 16) & 0xFF;
      playerColor.g = (players[i].colorHex >>  8) & 0xFF;
      playerColor.b =  players[i].colorHex        & 0xFF;
      playerColor.nscale8(128);  // dim while waiting
      setHexColor(players[i].homeHex, playerColor);
    }
  }
}

// Player presses key 1–8 to select a strategy card.
static void handleStrategyCardSelection(uint8_t playerIndex, uint8_t cardKey) {
  if (gameState.currentPickIndex >= gameState.totalPicks) return;
  uint8_t currentPicker = gameState.strategyPickOrder[gameState.currentPickIndex];
  if (playerIndex != currentPicker) return;  // not your turn
  if (cardKey < 1 || cardKey > 8) return;

  if (isCardLocked(cardKey)) {
    flashUnavailableColor(playerIndex);  // red flash = taken
    if (rtCfg.debugSerial) {
      Serial.print(F("Game: Card "));
      Serial.print(cardKey);
      Serial.println(F(" already taken"));
    }
    return;
  }

  if (players[playerIndex].strategyPicksDone == 0) {
    players[playerIndex].strategyCard  = cardKey;
  } else {
    players[playerIndex].strategyCard2 = cardKey;
  }

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.print(F(" previewing strategy card "));
    Serial.println(cardKey);
  }
}

// Player presses Key 15 to lock the current pick and hand off to the next picker.
// Speaker changes come from Politics being played, handled via the admin page —
// no card grants the speaker token by itself.
static void handleStrategyLockIn(uint8_t playerIndex) {
  if (gameState.currentPickIndex >= gameState.totalPicks) return;
  uint8_t currentPicker = gameState.strategyPickOrder[gameState.currentPickIndex];
  if (playerIndex != currentPicker) return;

  uint8_t pickedCard = (players[playerIndex].strategyPicksDone == 0)
                       ? players[playerIndex].strategyCard
                       : players[playerIndex].strategyCard2;
  if (pickedCard == 0) return;  // must select first

  players[playerIndex].strategyPicksDone++;
  if (players[playerIndex].strategyPicksDone >= strategyPicksRequired()) {
    players[playerIndex].strategyLocked = true;
  }

  // Initiative is the lowest card held
  if (players[playerIndex].strategyPicksDone == 1) {
    players[playerIndex].initiative = pickedCard;
  } else if (pickedCard < players[playerIndex].initiative) {
    players[playerIndex].initiative = pickedCard;
  }

  uint8_t cardIdx = players[playerIndex].strategyCard - 1;
  CRGB stratColor;
  stratColor.r = (STRATEGY_COLORS[cardIdx] >> 16) & 0xFF;
  stratColor.g = (STRATEGY_COLORS[cardIdx] >>  8) & 0xFF;
  stratColor.b =  STRATEGY_COLORS[cardIdx]        & 0xFF;
  colorPlayerArea(playerIndex, stratColor);
  pushLEDs();  // immediate feedback on lock-in

  gameState.currentPickIndex++;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.print(F(" locked strategy card "));
    Serial.println(pickedCard);
  }
}

static bool checkStrategyComplete() {
  return (gameState.currentPickIndex >= gameState.totalPicks);
}

// =============================================================================
// SECTION 6: PHASE_ACTION — Main Turn Sequence
// =============================================================================

// Two-step battle initiation state (Key 13 → Key 1-8).
// battlePending: a player pressed Key 13 and is waiting to select an opponent.
// battleChallenger: the playerIndex who initiated the challenge.
static bool    battlePending    = false;
static uint8_t battleChallenger = 0xFF;

// Sorts active, non-passed players by initiative (strategy card) ascending.
static void buildActionOrder() {
  uint8_t sortedPlayers[MAX_PLAYERS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].hasPassed) sortedPlayers[count++] = i;
  }

  // Bubble sort by initiative (lower = higher priority)
  for (uint8_t i = 0; i < count - 1; i++) {
    for (uint8_t j = 0; j < count - i - 1; j++) {
      if (players[sortedPlayers[j]].initiative > players[sortedPlayers[j + 1]].initiative) {
        uint8_t temp       = sortedPlayers[j];
        sortedPlayers[j]   = sortedPlayers[j + 1];
        sortedPlayers[j+1] = temp;
      }
    }
  }

  gameState.actionOrderSize = count;
  for (uint8_t i = 0; i < count; i++) gameState.actionOrder[i] = sortedPlayers[i];
}

// Resting color of a hex in the action phase: home hexes in player color
// (dimmed once passed), claimed hexes in owner color, everything else dark.
static CRGB actionPhaseHexColor(uint8_t hexIndex) {
  for (uint8_t playerIndex = 0; playerIndex < MAX_PLAYERS; playerIndex++) {
    if (!players[playerIndex].active || players[playerIndex].homeHex != hexIndex) continue;
    CRGB homeColor;
    homeColor.r = (players[playerIndex].colorHex >> 16) & 0xFF;
    homeColor.g = (players[playerIndex].colorHex >>  8) & 0xFF;
    homeColor.b =  players[playerIndex].colorHex        & 0xFF;
    if (players[playerIndex].hasPassed) homeColor.nscale8((uint8_t)((255UL * PASSED_DIM_PERCENT) / 100));
    return homeColor;
  }

  int8_t owner = hexOwner[hexIndex];
  if (owner < 0 || owner >= MAX_PLAYERS) return CRGB::Black;
  CRGB ownerColor;
  ownerColor.r = (players[owner].colorHex >> 16) & 0xFF;
  ownerColor.g = (players[owner].colorHex >>  8) & 0xFF;
  ownerColor.b =  players[owner].colorHex        & 0xFF;
  return ownerColor;
}

// Called every loop() during PHASE_ACTION.
// The active player's stretch of the board perimeter (outer sides of the edge
// hexes in their slice) breathes white, or red once their turn runs past
// TURN_WARNING_MS. Every LED is written exactly once per pass so the LED task
// on Core 0 never pushes a half-drawn frame.
static void updateActionPhaseDisplay() {
  if (gameState.actionOrderSize == 0) return;

  uint8_t  activePlayer = gameState.actionOrder[gameState.currentActionIndex];
  uint32_t turnElapsed  = millis() - players[activePlayer].turnStartMs;
  bool     overTime     = (turnElapsed >= TURN_WARNING_MS);

  CRGB breathColor = overTime ? CRGB::Red : CRGB::White;
  breathColor.nscale8(beatsin8(30, 60, 255));

  for (uint8_t hexIndex = 0; hexIndex < NUM_HEXES; hexIndex++) {
    CRGB restingColor = actionPhaseHexColor(hexIndex);
    hexColor[hexIndex] = restingColor;
    bool inActiveSlice = (gameState.hexSliceOwner[hexIndex] == activePlayer);
    for (int side = 0; side < 6; side++) {
      bool turnEdge = inActiveSlice && HEX_NEIGHBORS[hexIndex][side] < 0;
      setHexSideColor(hexIndex, side, turnEdge ? breathColor : restingColor);
    }
  }
}

// Player presses Key 14 on their own turn to pass — removed from initiative order this round.
static void handlePlayerPass(uint8_t playerIndex) {
  if (!players[playerIndex].active || players[playerIndex].hasPassed) return;
  if (gameState.actionOrderSize == 0) return;
  if (gameState.actionOrder[gameState.currentActionIndex] != playerIndex) return;
  players[playerIndex].hasPassed = true;

  CRGB playerColor;
  playerColor.r = (players[playerIndex].colorHex >> 16) & 0xFF;
  playerColor.g = (players[playerIndex].colorHex >>  8) & 0xFF;
  playerColor.b =  players[playerIndex].colorHex        & 0xFF;
  playerColor.nscale8((uint8_t)((255UL * PASSED_DIM_PERCENT) / 100));
  setHexColor(players[playerIndex].homeHex, playerColor);
  pushLEDs();

  buildActionOrder();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" passed"));
  }

  // If this was the current active player, keep currentActionIndex valid
  if (gameState.actionOrderSize > 0) {
    if (gameState.currentActionIndex >= gameState.actionOrderSize) {
      gameState.currentActionIndex = 0;
    }
    players[gameState.actionOrder[gameState.currentActionIndex]].turnStartMs = millis();
  }
}

// Player presses Key 15 to end their turn and pass to the next.
static void handleEndTurn(uint8_t playerIndex) {
  if (gameState.actionOrderSize == 0) return;
  if (gameState.actionOrder[gameState.currentActionIndex] != playerIndex) return;

  gameState.currentActionIndex = (gameState.currentActionIndex + 1) % gameState.actionOrderSize;
  players[gameState.actionOrder[gameState.currentActionIndex]].turnStartMs = millis();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.print(F(" ended turn — now Player "));
    Serial.println(gameState.actionOrder[gameState.currentActionIndex] + 1);
  }
}

// Admin override: makes playerIndex the active player. Passed players are skipped.
static void setActionTurn(uint8_t playerIndex) {
  if (gameState.currentPhase != PHASE_ACTION) return;
  for (uint8_t orderIndex = 0; orderIndex < gameState.actionOrderSize; orderIndex++) {
    if (gameState.actionOrder[orderIndex] != playerIndex) continue;
    if (orderIndex == gameState.currentActionIndex) return;
    gameState.currentActionIndex = orderIndex;
    players[playerIndex].turnStartMs = millis();
    if (rtCfg.debugSerial) {
      Serial.print(F("Game: admin set turn to P")); Serial.println(playerIndex + 1);
    }
    return;
  }
}

static bool checkActionComplete() {
  return (gameState.actionOrderSize == 0);
}

// Battle mode: splits the board by proximity to each combatant's home hex.
static void startBattle(uint8_t attackerIndex, uint8_t defenderIndex) {
  gameState.inBattle       = true;
  gameState.battleAttacker = attackerIndex;
  gameState.battleDefender = defenderIndex;

  uint8_t hex1 = players[attackerIndex].homeHex;
  uint8_t hex2 = players[defenderIndex].homeHex;
  CRGB color1, color2;
  color1.r = (players[attackerIndex].colorHex >> 16) & 0xFF;
  color1.g = (players[attackerIndex].colorHex >>  8) & 0xFF;
  color1.b =  players[attackerIndex].colorHex        & 0xFF;
  color2.r = (players[defenderIndex].colorHex >> 16) & 0xFF;
  color2.g = (players[defenderIndex].colorHex >>  8) & 0xFF;
  color2.b =  players[defenderIndex].colorHex        & 0xFF;

  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    setHexColor(h, (hexDistance(h, hex1) <= hexDistance(h, hex2)) ? color1 : color2);
  }
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Battle — P"));
    Serial.print(attackerIndex + 1);
    Serial.print(F(" vs P"));
    Serial.println(defenderIndex + 1);
  }
}

static void endBattle() {
  gameState.inBattle = false;
  if (rtCfg.debugSerial) Serial.println(F("Game: Battle ended"));
}

// =============================================================================
// SECTION 7: PHASE_STATUS — End-of-Round Cleanup
// =============================================================================

// Assigns each hex to the active player whose home hex is angularly closest.
// This matches the actual seating positions around the board rather than
// mapping by player index order, which caused cross-player slice assignment.
static void assignSlicesToPlayers() {
  if (gameState.numActivePlayers == 0) return;

  // Cache each active player's home-hex angle
  float   homeAngle[MAX_PLAYERS];
  uint8_t activeList[MAX_PLAYERS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    activeList[count] = i;
    homeAngle[count]  = getAngleFromCenter(players[i].homeHex);
    count++;
  }

  // Each hex → nearest player by circular angular distance
  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    float   angle   = getAngleFromCenter(h);
    float   minDist = 361.0f;
    uint8_t owner   = activeList[0];
    for (uint8_t p = 0; p < count; p++) {
      float diff = fabsf(angle - homeAngle[p]);
      if (diff > 180.0f) diff = 360.0f - diff;
      if (diff < minDist) { minDist = diff; owner = activeList[p]; }
    }
    gameState.hexSliceOwner[h] = owner;
  }
}

// Called every loop() during PHASE_STATUS.
// Each player's slice pulses their color (20%→50%, 1s).
// When a player is ready: slice shows full brightness, solid.
static void updateStatusPulse() {
  uint8_t pulseBrightness = beatsin8(60, STATUS_PULSE_MIN, STATUS_PULSE_MAX);

  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    uint8_t owner = gameState.hexSliceOwner[h];
    CRGB color;
    color.r = (players[owner].colorHex >> 16) & 0xFF;
    color.g = (players[owner].colorHex >>  8) & 0xFF;
    color.b =  players[owner].colorHex        & 0xFF;

    if (!players[owner].readyForNext) {
      color.nscale8(pulseBrightness);
    }
    setHexColor(h, color);
  }
}

// Player presses Key 15 in status phase to mark themselves ready.
static void handleStatusReady(uint8_t playerIndex) {
  if (!players[playerIndex].active) return;
  players[playerIndex].readyForNext = true;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" ready (status)"));
  }
}

static bool checkStatusComplete() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].readyForNext) return false;
  }
  return true;
}

// =============================================================================
// SECTION 8: PHASE_AGENDA — Speaker Actions
// =============================================================================

// Called every loop() during PHASE_AGENDA.
// Entire board pulses white gently (20%→50%, 1s cycle).
static void updateAgendaPulse() {
  uint8_t pulseBrightness = beatsin8(60, AGENDA_PULSE_MIN, AGENDA_PULSE_MAX);
  CRGB white = CRGB::White;
  white.nscale8(pulseBrightness);
  setAllHexes(white);
}

// Resets all per-round player flags so the next round starts clean.
static void resetRoundState() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].hasPassed         = false;
    players[i].readyForNext      = false;
    players[i].strategyCard      = 0;
    players[i].strategyCard2     = 0;
    players[i].strategyPicksDone = 0;
    players[i].strategyLocked    = false;
  }
}

// =============================================================================
// SECTION 9: Phase Transitions
// =============================================================================

void transitionToSetup() {
  gameState.currentPhase = PHASE_SETUP;
  memset(gameState.colorTaken, 0, sizeof(gameState.colorTaken));

  detectConnectedKeyboards();
  assignHomeHexes();

  // Assign default colors (first N from palette, in player order)
  uint8_t colorSlot = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    players[i].selectedColorIndex = colorSlot % 8;
    players[i].colorHex           = COLOR_PALETTE[colorSlot % 8];
    players[i].colorLocked        = false;
    colorSlot++;
  }

  setAllHexes(CRGB::Black);
  for (int h = 0; h < NUM_HEXES; h++) hexOwner[h] = -1;
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: SETUP — players selecting colors"));
    Serial.println(F("  Keys 1-8: select color  |  Key 15: lock color  |  Key 0: start game (GM)"));
  }
}

void transitionToStrategy() {
  gameState.currentPhase = PHASE_STRATEGY;
  resetRoundState();  // admin phase jumps skip the agenda reset
  buildStrategyPickOrder();

  setAllHexes(CRGB::Black);
  for (int h = 0; h < NUM_HEXES; h++) hexOwner[h] = -1;
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: STRATEGY — players selecting strategy cards"));
    Serial.println(F("  Keys 1-8: select card  |  Key 15: lock card"));
    Serial.print(F("  Pick order: "));
    for (uint8_t i = 0; i < gameState.totalPicks; i++) {
      Serial.print(F("P")); Serial.print(gameState.strategyPickOrder[i] + 1);
      if (i < gameState.totalPicks - 1) Serial.print(F(" → "));
    }
    Serial.println();
  }
}

void transitionToAction() {
  gameState.currentPhase      = PHASE_ACTION;
  gameState.currentActionIndex = 0;
  gameState.inBattle           = false;
  battlePending                = false;

  for (uint8_t playerIndex = 0; playerIndex < MAX_PLAYERS; playerIndex++) players[playerIndex].hasPassed = false;
  buildActionOrder();
  assignSlicesToPlayers();  // perimeter slices for turn lighting
  if (gameState.actionOrderSize > 0) {
    players[gameState.actionOrder[0]].turnStartMs = millis();
  }

  // All hexes go dark; ownership cleared
  setAllHexes(CRGB::Black);
  for (int h = 0; h < NUM_HEXES; h++) hexOwner[h] = -1;

  // Seed home hexes in player color
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    CRGB color;
    color.r = (players[i].colorHex >> 16) & 0xFF;
    color.g = (players[i].colorHex >>  8) & 0xFF;
    color.b =  players[i].colorHex        & 0xFF;
    setHexColor(players[i].homeHex, color);
  }
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: ACTION — initiative order:"));
    for (uint8_t i = 0; i < gameState.actionOrderSize; i++) {
      Serial.print(F("  P")); Serial.print(gameState.actionOrder[i] + 1);
      Serial.print(F(" (card ")); Serial.print(players[gameState.actionOrder[i]].strategyCard);
      Serial.println(F(")"));
    }
    Serial.println(F("  Key 14: pass  |  Key 15: end turn  |  Key 13: battle mode"));
  }
}

void transitionToStatus() {
  gameState.currentPhase = PHASE_STATUS;
  gameState.inBattle     = false;
  battlePending          = false;
  for (uint8_t playerIndex = 0; playerIndex < MAX_PLAYERS; playerIndex++) players[playerIndex].readyForNext = false;
  assignSlicesToPlayers();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: STATUS — all players ready up"));
    Serial.println(F("  Key 15: ready"));
  }
}

void transitionToAgenda() {
  gameState.currentPhase = PHASE_AGENDA;

  setAllHexes(CRGB::Black);
  for (int h = 0; h < NUM_HEXES; h++) hexOwner[h] = -1;
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.print(F("Phase: AGENDA — speaker is P"));
    Serial.println(gameState.speakerIndex + 1);
    Serial.println(F("  Key 15 (speaker): end agenda / start next round"));
  }
}

// =============================================================================
// SECTION 10: Key Dispatch
// =============================================================================
// Called from onKeyPressed() in TI4_HexRiser.ino (keyboard hardware)
// and also from the serial 'kb' command (simulation).

void handleGameKey(uint8_t playerIndex, uint8_t key) {
  if (rtCfg.debugSerial) {
    Serial.print(F("Key: P")); Serial.print(playerIndex + 1);
    Serial.print(F(" key=")); Serial.println(key);
  }

  switch (gameState.currentPhase) {

    case PHASE_SETUP:
      if (key >= 1 && key <= 8) {
        handleColorSelection(playerIndex, key);
      } else if (key == 15) {
        handleColorLockIn(playerIndex);
      } else if (key == 0) {
        // Key 0 = GM "Start Game" — any player can trigger if all are locked
        if (allPlayersLocked()) {
          selectRandomSpeaker();
          transitionToStrategy();
        }
      }
      break;

    case PHASE_STRATEGY:
      if (key >= 1 && key <= 8) {
        handleStrategyCardSelection(playerIndex, key);
      } else if (key == 15) {
        handleStrategyLockIn(playerIndex);
      }
      break;

    case PHASE_ACTION:
      if (key == 13) {
        if (gameState.inBattle) {
          // Any player pressing Key 13 ends the battle
          endBattle();
          battlePending = false;
        } else if (battlePending && battleChallenger == playerIndex) {
          // Same player presses Key 13 again — cancel the challenge
          battlePending = false;
          if (rtCfg.debugSerial) Serial.println(F("Game: Battle challenge cancelled"));
        } else {
          // First press: enter challenge mode
          battlePending    = true;
          battleChallenger = playerIndex;
          if (rtCfg.debugSerial) {
            Serial.print(F("Game: P")); Serial.print(playerIndex + 1);
            Serial.println(F(" challenging — press opponent number (1-8)"));
          }
        }
      } else if (battlePending && playerIndex == battleChallenger && key >= 1 && key <= 8) {
        // Second press: challenger picks opponent by player number
        uint8_t defenderIdx = key - 1;  // key 1 → player index 0
        if (defenderIdx != battleChallenger && players[defenderIdx].active) {
          battlePending = false;
          startBattle(battleChallenger, defenderIdx);
        } else {
          if (rtCfg.debugSerial) Serial.println(F("Game: Invalid battle target"));
        }
      } else if (key == 15) {
        handleEndTurn(playerIndex);
      } else if (key == 14) {
        handlePlayerPass(playerIndex);
      }
      break;

    case PHASE_STATUS:
      if (key == 15) {
        handleStatusReady(playerIndex);
      }
      break;

    case PHASE_AGENDA:
      if (key == 15 && playerIndex == gameState.speakerIndex) {
        resetRoundState();
        runRippleTransition(true);  // new round — ripple back inward
        transitionToStrategy();
      }
      break;
  }
}

// =============================================================================
// SECTION 11: Main State Machine Update (called every loop())
// =============================================================================

void updateGameState() {
  bool effectActive = (currentEffect != ANIM_NONE);

  switch (gameState.currentPhase) {

    case PHASE_SETUP:
      if (!effectActive) updateJoinModeDisplay();
      break;

    case PHASE_STRATEGY:
      if (!effectActive) updateStrategyPickerPulse();
      if (checkStrategyComplete()) {
        runRippleTransition(false);
        transitionToAction();
      }
      break;

    case PHASE_ACTION:
      if (checkActionComplete()) {
        runRippleTransition(false);
        transitionToStatus();
      } else if (!gameState.inBattle && !effectActive) {
        updateActionPhaseDisplay();
      }
      break;

    case PHASE_STATUS:
      if (!effectActive) updateStatusPulse();
      if (checkStatusComplete()) {
        // Agenda phase only exists once the custodians token has left Mecatol Rex
        if (gameOpts.agendaAfterCustodians && !gameOpts.custodiansTaken) {
          resetRoundState();
          runRippleTransition(true);  // new round — ripple back inward
          transitionToStrategy();
        } else {
          runRippleTransition(false);
          transitionToAgenda();
        }
      }
      break;

    case PHASE_AGENDA:
      if (!effectActive) updateAgendaPulse();
      break;
  }
}

// =============================================================================
// SECTION 12: initGameState() — call once in setup()
// =============================================================================

void initGameState(uint8_t defaultPlayerCount) {
  // Initialize all player slots
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].id                 = i + 1;
    players[i].active             = (i < defaultPlayerCount);
    players[i].initiative         = i + 1;
    players[i].colorHex           = COLOR_PALETTE[i];
    players[i].hasPassed          = false;
    players[i].selectedColorIndex = i;
    players[i].colorLocked        = false;
    players[i].strategyCard       = 0;
    players[i].strategyCard2      = 0;
    players[i].strategyPicksDone  = 0;
    players[i].strategyLocked     = false;
    players[i].readyForNext       = false;
    players[i].turnStartMs        = 0;
    players[i].homeHex            = 30;  // will be set properly in transitionToSetup()
  }

  memset(&gameState, 0, sizeof(gameState));
  gameState.currentPhase   = PHASE_SETUP;
  gameOpts.custodiansTaken = false;
}
