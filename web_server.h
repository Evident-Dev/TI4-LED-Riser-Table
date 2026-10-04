#pragma once
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "runtime_settings.h"
#include "home_page.h"
#include "projector_page.h"
#include "board_script.h"
#include "theme_style.h"
#include "admin_page.h"
#include "play_page.h"
#include "settings_page.h"
#include "led_control.h"
#include "map_state.h"
#include "tile_script.h"

// =============================================================================
// TI4 Hex Riser - Network: WiFi + ESPAsyncWebServer + WebSocket
// Target: ESP32-S3-WROOM-1-N16R8
// =============================================================================
// Routes:
//   GET /              -> home page (links to admin, player, projector)
//   GET /projector     -> full-screen board mirror for a projector
//   GET /board.js      -> board renderer shared by projector and admin
//   GET /theme.css     -> holo button theme shared by every page
//   GET /tiles.js      -> tile and faction image loader shared by every page
//   GET /play, /player -> player seat select + phase-aware keypad
//   GET /admin         -> game master controls
//   GET /settings      -> runtime settings page
//   GET /getsettings   -> current config as JSON
//   GET /savesettings? -> update runtime config
//   GET /reboot        -> restart ESP32
//   WS  /ws            -> all live traffic (state push + commands)
//
// WebSocket, server -> client:
//   binary [0x01][366 x RGB]   side colors for hex 0 side 0 .. hex 60 side 5
//   text   {"t":"game",...}    game state JSON (phase, players, seats, turn; boot=1 while starting up; bid = boot ID)
//   text   {"t":"map",...}     tile number and rotation per hex, on connect and on change
//   text   FACTIONTAKEN        faction pick refused, another seat has it
//   text   CLAIMED:seat:token / DENIED:seat:reason   claim responses
//
// WebSocket, client -> server:
//   CLAIM:seat:token:name      claim a seat (token 0 = new claim; a matching token rejoins a reserved seat)
//   RESUME                     resume the saved game after a power loss
//   RELEASE:seat:token         give up a seat
//   FACTION:seat:token:faction:homeTile   pick a faction during setup ("" clears)
//   KEY:seat:key:token         keypad press, routed through the command queue
//   ADMIN:SETPLAYERS:n / STARTGAME / PHASE:n / BATTLE:a:d / ENDBATTLE
//        / RESET / KICK:seat / SPEAKER:seat / TURN:seat / NEWGAME
//        / TILE:hex:rotation:tile / CLEARMAP   (map edits, setup only)
//   BRIGHTNESS:n, EFFECT:name, SETHEX..., CLAIMHEX..., ALL:...  (board tools)
//
// Threading:
//   WS events run on the async_tcp task. Anything that touches the game state
//   machine is queued into webCmdQueue and drained by loop() on Core 1.
//   State broadcasting runs in its own task via wsBroadcastTick(), so the
//   browser stays live during blocking animations on Core 1 and a slow WiFi
//   client can never stall the LED task.
// =============================================================================

static AsyncWebServer _server(HTTP_PORT);
static AsyncWebSocket _ws("/ws");
static bool networkReady = false;
static volatile bool bootComplete = false;            // set once the boot animation finishes
static volatile uint32_t lastNetworkActivityMs = 0;  // last page load or socket connect, for boot settling
static uint32_t bootIdentifier = 0;                  // random per boot; pages reload when it changes

// -----------------------------------------------------------------------------
// Seats — one per player, claimed by a phone via token
// A reserved seat came back from a saved game: the phone with the matching
// token rejoins it, and anyone else can still take it.
// -----------------------------------------------------------------------------
#define FACTION_LENGTH 24

struct Seat {
  bool     claimed;
  bool     reserved;
  uint32_t token;
  char     name[17];
  char     faction[FACTION_LENGTH];       // CDN faction id, "" = none
  char     homeTile[MAP_TILE_LENGTH];     // that faction's home system tile
};
static Seat seats[MAX_PLAYERS] = {};

// Random per game. Phones store it with their seat token and only rejoin
// when it matches, so a token from an older game never grabs a seat.
static uint32_t gameIdentifier = 0;

// Starts a fresh game identity and frees every seat.
static void startNewGameIdentity() {
  gameIdentifier = esp_random();
  memset(seats, 0, sizeof(seats));
}

// -----------------------------------------------------------------------------
// Command queue — WS handlers enqueue, loop() on Core 1 executes
// -----------------------------------------------------------------------------
enum WebCmdType : uint8_t {
  WCMD_KEY,          // a = player index, b = key
  WCMD_SETPLAYERS,   // a = count
  WCMD_STARTGAME,
  WCMD_PHASE,        // a = phase
  WCMD_BATTLE,       // a = attacker, b = defender
  WCMD_ENDBATTLE,
  WCMD_RESET,
  WCMD_SPEAKER,      // a = player index (Politics resolved at the table)
  WCMD_TURN,         // a = player index to make the active player
  WCMD_RESUME,       // resume the saved game (anyone)
  WCMD_NEWGAME       // discard the saved game (admin)
};
struct WebCmd {
  uint8_t type;
  uint8_t a;
  uint8_t b;
};
static QueueHandle_t webCmdQueue = nullptr;

static void queueWebCmd(uint8_t type, uint8_t a = 0, uint8_t b = 0) {
  if (!webCmdQueue) return;
  WebCmd cmd = { type, a, b };
  xQueueSend(webCmdQueue, &cmd, 0);  // drop if full rather than block the net task
}

// Forward declarations
void parseWSCommand(const char* msg);
void parseSaveSettings(const String& query);
size_t buildLedFrame(uint8_t* buf);
int    buildGameJson(char* buf, size_t bufLen);

// Defined in game_state.h (included before this file in the .ino)
void handleGameKey(uint8_t playerIndex, uint8_t key);

// Defined in save_state.h (included after this file)
bool    isRecoveryPending();
uint8_t savedGamePhase();
uint8_t savedGamePlayerCount();
void    clearSavedGame();
void    resumeSavedGame();
void    discardSavedGame();

// -----------------------------------------------------------------------------
// drainWebCommands() — call every loop() on Core 1
// -----------------------------------------------------------------------------
void drainWebCommands() {
  if (!webCmdQueue) return;
  WebCmd cmd;
  while (xQueueReceive(webCmdQueue, &cmd, 0) == pdTRUE) {
    // While a saved game waits, only resume, discard, or a new setup get through
    if (isRecoveryPending() && cmd.type != WCMD_RESUME && cmd.type != WCMD_NEWGAME
        && cmd.type != WCMD_SETPLAYERS && cmd.type != WCMD_RESET) continue;

    switch (cmd.type) {

      case WCMD_RESUME:
        resumeSavedGame();
        break;

      case WCMD_NEWGAME:
        discardSavedGame();
        break;

      case WCMD_KEY:
        if (cmd.a < MAX_PLAYERS && players[cmd.a].active) {
          handleGameKey(cmd.a, cmd.b);
        }
        break;

      case WCMD_SETPLAYERS:
        if (cmd.a >= 4 && cmd.a <= MAX_PLAYERS) {
          clearSavedGame();
          for (uint8_t i = 0; i < MAX_PLAYERS; i++) players[i].active = (i < cmd.a);
          transitionToSetup();
          if (rtCfg.debugSerial) {
            Serial.print(F("Web: set ")); Serial.print(cmd.a); Serial.println(F(" players"));
          }
        }
        break;

      case WCMD_STARTGAME:
        for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
          if (players[i].active && !players[i].colorLocked) {
            uint8_t colorIdx = players[i].selectedColorIndex;
            if (!gameState.colorTaken[colorIdx]) {
              players[i].colorLocked         = true;
              gameState.colorTaken[colorIdx] = true;
            }
          }
        }
        selectRandomSpeaker();
        transitionToStrategy();
        break;

      case WCMD_PHASE:
        switch (cmd.a) {
          case 0: transitionToSetup();    break;
          case 1: transitionToStrategy(); break;
          case 2: transitionToAction();   break;
          case 3: transitionToStatus();   break;
          case 4: transitionToAgenda();   break;
        }
        break;

      case WCMD_BATTLE:
        if (cmd.a < MAX_PLAYERS && cmd.b < MAX_PLAYERS && cmd.a != cmd.b
            && players[cmd.a].active && players[cmd.b].active) {
          battlePending = false;
          startBattle(cmd.a, cmd.b);
        }
        break;

      case WCMD_ENDBATTLE:
        endBattle();
        battlePending = false;
        break;

      case WCMD_RESET: {
        uint8_t count = gameState.numActivePlayers;
        if (count < 4) count = 6;
        clearSavedGame();
        startNewGameIdentity();
        initGameState(count);
        stopEffect();
        transitionToSetup();
        break;
      }

      case WCMD_SPEAKER:
        if (cmd.a < MAX_PLAYERS && players[cmd.a].active) {
          gameState.speakerIndex = cmd.a;
          if (rtCfg.debugSerial) {
            Serial.print(F("Web: speaker set to P")); Serial.println(cmd.a + 1);
          }
        }
        break;

      case WCMD_TURN:
        if (cmd.a < MAX_PLAYERS && players[cmd.a].active) setActionTurn(cmd.a);
        break;
    }
  }
}

// -----------------------------------------------------------------------------
// LED side-state frame: [0x01] + 366 sides x 3 bytes RGB
// -----------------------------------------------------------------------------
#define LED_FRAME_LEN (1 + NUM_HEXES * 6 * 3)
#define GAME_JSON_LENGTH 2800
#define MAP_JSON_LENGTH  800

size_t buildLedFrame(uint8_t* buf) {
  buf[0] = 0x01;
  size_t pos = 1;
  for (int h = 0; h < NUM_HEXES; h++) {
    for (int s = 0; s < 6; s++) {
      int idx = HEX_MAP[h][s][0];
      CRGB c = (idx >= 0 && idx < NUM_LEDS) ? leds[idx] : CRGB(0, 0, 0);
      buf[pos++] = c.r;
      buf[pos++] = c.g;
      buf[pos++] = c.b;
    }
  }
  return pos;
}

// -----------------------------------------------------------------------------
// Game state JSON — consumed by all three pages
// -----------------------------------------------------------------------------
int buildGameJson(char* buf, size_t bufLen) {
  int picker = -1;
  if (gameState.currentPhase == PHASE_STRATEGY
      && gameState.currentPickIndex < gameState.totalPicks) {
    picker = gameState.strategyPickOrder[gameState.currentPickIndex];
  }
  int turn = -1;
  if (gameState.currentPhase == PHASE_ACTION && gameState.actionOrderSize > 0) {
    turn = gameState.actionOrder[gameState.currentActionIndex];
  }

  // One character per hex: player index, or '-' when nobody owns it
  char owners[NUM_HEXES + 1];
  for (int hexIndex = 0; hexIndex < NUM_HEXES; hexIndex++) {
    owners[hexIndex] = (hexOwner[hexIndex] >= 0) ? (char)('0' + hexOwner[hexIndex]) : '-';
  }
  owners[NUM_HEXES] = 0;

  uint8_t colorMask = 0, cardMask = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (gameState.colorTaken[i]) colorMask |= (1 << i);
  }
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    if (players[i].strategyPicksDone >= 1 && players[i].strategyCard >= 1) {
      cardMask |= (1 << (players[i].strategyCard - 1));
    }
    if (players[i].strategyPicksDone >= 2 && players[i].strategyCard2 >= 1) {
      cardMask |= (1 << (players[i].strategyCard2 - 1));
    }
  }

  int pos = snprintf(buf, bufLen,
    "{\"t\":\"game\",\"phase\":%d,\"speaker\":%d,\"n\":%d,\"picker\":%d,\"turn\":%d,"
    "\"pend\":%d,\"atk\":%d,\"def\":%d,\"cTaken\":%u,\"kTaken\":%u,"
    "\"agOpt\":%d,\"dbl\":%d,\"cust\":%d,\"boot\":%d,\"bid\":%lu,\"gid\":%lu,\"rec\":%d,\"recPhase\":%d,\"recPlayers\":%d,\"own\":\"%s\",\"players\":[",
    (int)gameState.currentPhase,
    (int)gameState.speakerIndex,
    (int)gameState.numActivePlayers,
    picker, turn,
    battlePending ? (int)battleChallenger : -1,
    gameState.inBattle ? (int)gameState.battleAttacker : -1,
    gameState.inBattle ? (int)gameState.battleDefender : -1,
    colorMask, cardMask,
    gameOpts.agendaAfterCustodians ? 1 : 0,
    gameOpts.doubleCardsFor4P ? 1 : 0,
    gameOpts.custodiansTaken ? 1 : 0,
    bootComplete ? 0 : 1,
    (unsigned long)bootIdentifier,
    (unsigned long)gameIdentifier,
    isRecoveryPending() ? 1 : 0,
    isRecoveryPending() ? (int)savedGamePhase() : -1,
    isRecoveryPending() ? (int)savedGamePlayerCount() : 0,
    owners);

  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    pos += snprintf(buf + pos, bufLen - pos,
      "%s{\"a\":%d,\"col\":\"%06lX\",\"ci\":%d,\"cl\":%d,\"sc\":%d,\"sc2\":%d,\"pk\":%d,"
      "\"sl\":%d,\"pa\":%d,\"rd\":%d,\"st\":%d,\"rs\":%d,\"nm\":\"%s\","
      "\"fa\":\"%s\",\"ht\":\"%s\",\"hh\":%d}",
      i > 0 ? "," : "",
      players[i].active ? 1 : 0,
      (unsigned long)players[i].colorHex,
      (int)players[i].selectedColorIndex,
      players[i].colorLocked ? 1 : 0,
      (int)players[i].strategyCard,
      (int)players[i].strategyCard2,
      (int)players[i].strategyPicksDone,
      players[i].strategyLocked ? 1 : 0,
      players[i].hasPassed ? 1 : 0,
      players[i].readyForNext ? 1 : 0,
      seats[i].claimed ? 1 : 0,
      seats[i].reserved ? 1 : 0,
      (seats[i].claimed || seats[i].reserved) ? seats[i].name : "",
      seats[i].faction,
      seats[i].homeTile,
      players[i].active ? (int)players[i].homeHex : -1);
    if (pos >= (int)bufLen - 2) break;
  }
  pos += snprintf(buf + pos, bufLen - pos, "]}");
  return pos;
}

// -----------------------------------------------------------------------------
// wsBroadcastTick() — called from the broadcast task.
// Pushes the LED frame on change (rate-limited) and game JSON on a cadence.
// -----------------------------------------------------------------------------
void wsBroadcastTick() {
  if (!networkReady) return;

  static uint32_t lastLedCheck = 0, lastLedForce = 0, lastJson = 0, lastCleanup = 0;
  uint32_t now = millis();

  if (now - lastCleanup >= 1000) {
    lastCleanup = now;
    _ws.cleanupClients();
  }
  if (_ws.count() == 0) return;

  if (now - lastLedCheck >= 50) {
    lastLedCheck = now;
    static uint8_t frame[LED_FRAME_LEN];
    static uint8_t prevFrame[LED_FRAME_LEN];
    size_t len = buildLedFrame(frame);
    if (memcmp(frame, prevFrame, len) != 0 || now - lastLedForce >= 2000) {
      memcpy(prevFrame, frame, len);
      lastLedForce = now;
      _ws.binaryAll(frame, len);
    }
  }

  static uint32_t lastMapRevision = 0;
  if (lastMapRevision != mapRevision) {
    lastMapRevision = mapRevision;
    static char mapJson[MAP_JSON_LENGTH];
    buildMapJson(mapJson, sizeof(mapJson));
    _ws.textAll(mapJson);
  }

  if (now - lastJson >= 300) {
    lastJson = now;
    static char json[GAME_JSON_LENGTH];
    buildGameJson(json, sizeof(json));
    _ws.textAll(json);
  }
}

// -----------------------------------------------------------------------------
// Seat claim / release / key — WS text handlers (async_tcp task)
// -----------------------------------------------------------------------------
static void sanitizeName(const char* src, char* dst, size_t dstLen) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j < dstLen - 1; i++) {
    char c = src[i];
    if (c >= 32 && c != '"' && c != '\\' && c != ':') dst[j++] = c;
  }
  dst[j] = 0;
}

static void handleClaim(AsyncWebSocketClient* client, const char* args) {
  // args: seat:token:name
  int seat = atoi(args);
  const char* p = strchr(args, ':');
  if (!p || seat < 0 || seat >= MAX_PLAYERS) return;
  uint32_t token = strtoul(p + 1, nullptr, 10);
  const char* namePtr = strchr(p + 1, ':');

  char reply[48];
  bool sameHolder = (seats[seat].claimed || seats[seat].reserved)
                    && token != 0 && seats[seat].token == token;
  if (seats[seat].claimed && !sameHolder) {
    snprintf(reply, sizeof(reply), "DENIED:%d:taken", seat);
    client->text(reply);
    return;
  }
  if (!sameHolder) {
    // New holder, or someone taking over a reserved seat: fresh token voids the old one
    seats[seat].token = esp_random();
    if (seats[seat].token == 0) seats[seat].token = 1;
    seats[seat].name[0]     = 0;
    seats[seat].faction[0]  = 0;
    seats[seat].homeTile[0] = 0;
  }
  seats[seat].claimed  = true;
  seats[seat].reserved = false;
  if (namePtr && namePtr[1]) {
    sanitizeName(namePtr + 1, seats[seat].name, sizeof(seats[seat].name));
  }
  if (seats[seat].name[0] == 0) {
    snprintf(seats[seat].name, sizeof(seats[seat].name), "Player %d", seat + 1);
  }
  snprintf(reply, sizeof(reply), "CLAIMED:%d:%lu", seat, (unsigned long)seats[seat].token);
  client->text(reply);
}

static void handleRelease(const char* args) {
  int seat = atoi(args);
  const char* p = strchr(args, ':');
  if (!p || seat < 0 || seat >= MAX_PLAYERS) return;
  uint32_t token = strtoul(p + 1, nullptr, 10);
  if (seats[seat].claimed && seats[seat].token == token) {
    seats[seat] = Seat{};
  }
}

// args: seat:token:faction:homeTile. Setup only; one seat per faction.
static void handleFaction(AsyncWebSocketClient* client, const char* args) {
  if (gameState.currentPhase != PHASE_SETUP) return;
  int seat = atoi(args);
  const char* tokenStart = strchr(args, ':');
  if (!tokenStart || seat < 0 || seat >= MAX_PLAYERS) return;
  uint32_t token = strtoul(tokenStart + 1, nullptr, 10);
  if (!seats[seat].claimed || seats[seat].token != token) return;

  const char* factionStart = strchr(tokenStart + 1, ':');
  if (!factionStart) return;
  factionStart++;
  const char* homeTileStart = strchr(factionStart, ':');

  char faction[FACTION_LENGTH] = {};
  char homeTile[MAP_TILE_LENGTH] = {};
  size_t factionLength = homeTileStart ? (size_t)(homeTileStart - factionStart) : strlen(factionStart);
  if (factionLength >= sizeof(faction)) return;
  memcpy(faction, factionStart, factionLength);
  for (size_t index = 0; index < factionLength; index++) {
    if (!isalnum((unsigned char)faction[index]) && faction[index] != '_') return;
  }
  if (homeTileStart) {
    strncpy(homeTile, homeTileStart + 1, sizeof(homeTile) - 1);
    if (!isValidTileNumber(homeTile)) homeTile[0] = 0;
  }

  if (faction[0]) {
    for (uint8_t otherSeat = 0; otherSeat < MAX_PLAYERS; otherSeat++) {
      if (otherSeat == seat || !players[otherSeat].active) continue;
      if (strcmp(seats[otherSeat].faction, faction) == 0) {
        client->text("FACTIONTAKEN");
        return;
      }
    }
  }
  memcpy(seats[seat].faction, faction, sizeof(faction));
  memcpy(seats[seat].homeTile, faction[0] ? homeTile : "", faction[0] ? sizeof(homeTile) : 1);
}

static void handleKey(const char* args) {
  // args: seat:key:token
  int seat = atoi(args);
  const char* p1 = strchr(args, ':');
  if (!p1 || seat < 0 || seat >= MAX_PLAYERS) return;
  int key = atoi(p1 + 1);
  const char* p2 = strchr(p1 + 1, ':');
  if (!p2 || key < 0 || key > 15) return;
  uint32_t token = strtoul(p2 + 1, nullptr, 10);
  if (seats[seat].claimed && seats[seat].token == token) {
    queueWebCmd(WCMD_KEY, (uint8_t)seat, (uint8_t)key);
  }
}

static void handleAdmin(const char* args) {
  if (strncmp(args, "SETPLAYERS:", 11) == 0) {
    queueWebCmd(WCMD_SETPLAYERS, (uint8_t)atoi(args + 11));
  } else if (strcmp(args, "STARTGAME") == 0) {
    queueWebCmd(WCMD_STARTGAME);
  } else if (strncmp(args, "PHASE:", 6) == 0) {
    queueWebCmd(WCMD_PHASE, (uint8_t)atoi(args + 6));
  } else if (strncmp(args, "BATTLE:", 7) == 0) {
    int a = atoi(args + 7);
    const char* p = strchr(args + 7, ':');
    if (p) queueWebCmd(WCMD_BATTLE, (uint8_t)a, (uint8_t)atoi(p + 1));
  } else if (strcmp(args, "ENDBATTLE") == 0) {
    queueWebCmd(WCMD_ENDBATTLE);
  } else if (strcmp(args, "RESET") == 0) {
    queueWebCmd(WCMD_RESET);
  } else if (strcmp(args, "NEWGAME") == 0) {
    queueWebCmd(WCMD_NEWGAME);
  } else if (strncmp(args, "SPEAKER:", 8) == 0) {
    queueWebCmd(WCMD_SPEAKER, (uint8_t)atoi(args + 8));
  } else if (strncmp(args, "TURN:", 5) == 0) {
    queueWebCmd(WCMD_TURN, (uint8_t)atoi(args + 5));
  } else if (strncmp(args, "OPT:AG:", 7) == 0) {
    // Rules lock in once the game starts
    if (gameState.currentPhase == PHASE_SETUP) gameOpts.agendaAfterCustodians = (args[7] == '1');
  } else if (strncmp(args, "OPT:DBL:", 8) == 0) {
    if (gameState.currentPhase == PHASE_SETUP) gameOpts.doubleCardsFor4P = (args[8] == '1');
  } else if (strncmp(args, "CUST:", 5) == 0) {
    gameOpts.custodiansTaken = (args[5] == '1');
  } else if (strncmp(args, "TILE:", 5) == 0) {
    // TILE:hex:rotation:tile — an empty tile clears the hex
    if (gameState.currentPhase != PHASE_SETUP) return;
    int hexIndex = atoi(args + 5);
    const char* rotationStart = strchr(args + 5, ':');
    if (!rotationStart) return;
    int rotation = atoi(rotationStart + 1);
    const char* tileStart = strchr(rotationStart + 1, ':');
    if (!tileStart) return;
    setMapTile(hexIndex, tileStart + 1, (uint8_t)constrain(rotation, 0, 5));
  } else if (strcmp(args, "CLEARMAP") == 0) {
    if (gameState.currentPhase == PHASE_SETUP) clearMap();
  } else if (strncmp(args, "KICK:", 5) == 0) {
    int seat = atoi(args + 5);
    if (seat >= 0 && seat < MAX_PLAYERS) seats[seat] = Seat{};
  }
}

static void handleWsMessage(AsyncWebSocketClient* client, char* msg) {
  if (rtCfg.debugWeb) { Serial.print(F("WS: ")); Serial.println(msg); }

  if      (strncmp(msg, "CLAIM:",   6) == 0) handleClaim(client, msg + 6);
  else if (strncmp(msg, "RELEASE:", 8) == 0) handleRelease(msg + 8);
  else if (strncmp(msg, "KEY:",     4) == 0) handleKey(msg + 4);
  else if (strncmp(msg, "FACTION:", 8) == 0) handleFaction(client, msg + 8);
  else if (strncmp(msg, "ADMIN:",   6) == 0) handleAdmin(msg + 6);
  else if (strcmp(msg, "RESUME") == 0)       queueWebCmd(WCMD_RESUME);
  else parseWSCommand(msg);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    lastNetworkActivityMs = millis();
    // Immediate snapshot so the page renders without waiting for the next tick
    static uint8_t frame[LED_FRAME_LEN];
    size_t frameLen = buildLedFrame(frame);
    client->binary(frame, frameLen);
    static char json[GAME_JSON_LENGTH];
    buildGameJson(json, sizeof(json));
    client->text(json);
    static char mapJson[MAP_JSON_LENGTH];
    buildMapJson(mapJson, sizeof(mapJson));
    client->text(mapJson);

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      char msg[128];
      size_t copyLen = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
      memcpy(msg, data, copyLen);
      msg[copyLen] = 0;
      handleWsMessage(client, msg);
    }
  }
}

// -----------------------------------------------------------------------------
// initNetwork()
// -----------------------------------------------------------------------------
void initNetwork() {
  webCmdQueue = xQueueCreate(32, sizeof(WebCmd));
  bootIdentifier = esp_random();
  startNewGameIdentity();

  // ------------------------------------------------------------------
  // 1. Try home network (station mode)
  // ------------------------------------------------------------------
  bool stationOK = false;

  if (strlen(rtCfg.homeSSID) > 0) {
    if (rtCfg.debugSerial) {
      Serial.print(F("WiFi: connecting to '"));
      Serial.print(rtCfg.homeSSID);
      Serial.println(F("'..."));
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(rtCfg.homeSSID, rtCfg.homePass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < rtCfg.homeTimeoutMs) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      stationOK    = true;
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: connected to '"));
        Serial.print(rtCfg.homeSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.localIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: home network unavailable, falling back to AP"));
      WiFi.disconnect(true);
      delay(500);
    }
  }

  // ------------------------------------------------------------------
  // 2. Fall back to Access Point mode
  // ------------------------------------------------------------------
  if (!stationOK) {
    if (rtCfg.debugSerial) {
      Serial.print(F("WiFi: starting AP '"));
      Serial.print(rtCfg.apSSID);
      Serial.println(F("'"));
    }

    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(rtCfg.apSSID, rtCfg.apPass)) {
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: AP ready — connect to '"));
        Serial.print(rtCfg.apSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.softAPIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: AP start FAILED"));
    }
  }

  // ------------------------------------------------------------------
  // 3. Register routes
  // ------------------------------------------------------------------
  _ws.onEvent(onWsEvent);
  _server.addHandler(&_ws);
  _server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    lastNetworkActivityMs = millis();
    next();
  });

  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/html"), HOME_PAGE);
  });

  _server.on("/projector", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/html"), PROJECTOR_PAGE);
  });

  _server.on("/board.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("application/javascript"), BOARD_SCRIPT);
  });

  _server.on("/theme.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/css"), THEME_STYLE);
  });

  _server.on("/tiles.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("application/javascript"), TILE_SCRIPT);
  });

  _server.on("/play", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/html"), PLAY_PAGE);
  });

  _server.on("/player", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/html"), PLAY_PAGE);
  });

  _server.on("/admin", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, F("text/html"), ADMIN_PAGE);
  });

  // GetSettings — return current rtCfg as JSON
  _server.on("/getsettings", HTTP_GET, [](AsyncWebServerRequest* request) {
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"homeSSID\":\"%s\","
      "\"homePass\":\"%s\","
      "\"apSSID\":\"%s\","
      "\"apPass\":\"%s\","
      "\"homeTimeoutMs\":%lu,"
      "\"defaultBrightness\":%d,"
      "\"maxBrightness\":%d,"
      "\"ledUpdateMs\":%d,"
      "\"broadcastMs\":%d,"
      "\"sideGap\":%d,"
      "\"simulateHardware\":%s,"
      "\"debugSerial\":%s,"
      "\"debugWeb\":%s,"
      "\"debugLed\":%s,"
      "\"debugKeyboard\":%s,"
      "\"thinSides\":%s}",
      rtCfg.homeSSID, rtCfg.homePass,
      rtCfg.apSSID,   rtCfg.apPass,
      (unsigned long)rtCfg.homeTimeoutMs,
      rtCfg.defaultBrightness, rtCfg.maxBrightness,
      rtCfg.ledUpdateMs, rtCfg.broadcastMs, rtCfg.sideGap,
      rtCfg.simulateHardware ? "true" : "false",
      rtCfg.debugSerial      ? "true" : "false",
      rtCfg.debugWeb         ? "true" : "false",
      rtCfg.debugLed         ? "true" : "false",
      rtCfg.debugKeyboard    ? "true" : "false",
      rtCfg.thinSides        ? "true" : "false"
    );
    request->send(200, "application/json", buf);
  });

  // SaveSettings — rebuild query string from decoded params, pass to parser
  _server.on("/savesettings", HTTP_GET, [](AsyncWebServerRequest* request) {
    String qs = "";
    for (size_t i = 0; i < request->params(); i++) {
      if (i > 0) qs += "&";
      qs += request->getParam(i)->name();
      qs += "=";
      qs += request->getParam(i)->value();
    }
    parseSaveSettings(qs);
    request->send(204);
  });

  // Reboot
  _server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(204);
    delay(100);
    ESP.restart();
  });

  // Settings page
  _server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", SETTINGS_PAGE);
  });

  _server.begin();

  if (rtCfg.debugSerial) {
    Serial.print(F("HTTP: AsyncWebServer + WebSocket started on port "));
    Serial.println(HTTP_PORT);
  }
}

// -----------------------------------------------------------------------------
// handleNetwork() — no-op with AsyncWebServer
// Kept for compatibility with loop() and animDelay() in animations.h.
// -----------------------------------------------------------------------------
void handleNetwork() {}

// Prints the current WiFi mode and browsing address.
void printNetworkInfo() {
  if (WiFi.getMode() == WIFI_AP) {
    Serial.print(F("WiFi: AP '"));
    Serial.print(rtCfg.apSSID);
    Serial.print(F("' (password '"));
    Serial.print(rtCfg.apPass);
    Serial.print(F("') -> http://"));
    Serial.println(WiFi.softAPIP());
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi: '"));
    Serial.print(rtCfg.homeSSID);
    Serial.print(F("' -> http://"));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("WiFi: not connected"));
  }
}

// -----------------------------------------------------------------------------
// parseWSCommand — board page tools (colors, effects, brightness)
// Runs on the async_tcp task; game state commands go through the queue.
// -----------------------------------------------------------------------------
void parseWSCommand(const char* msg) {
  if (strncmp(msg, "BRIGHTNESS:", 11) == 0) {
    int b = atoi(msg + 11);
    setBrightness((uint8_t)constrain(b, 0, rtCfg.maxBrightness));

  } else if (strncmp(msg, "EFFECT:", 7) == 0) {
    const char* name = msg + 7;
    if      (strcmp(name, "RAINBOW") == 0) startEffect(ANIM_RAINBOW);
    else if (strcmp(name, "PULSE")   == 0) startEffect(ANIM_PULSE);
    else if (strcmp(name, "RIPPLE")  == 0) startEffect(ANIM_RIPPLE);
    else if (strcmp(name, "SPARKLE") == 0) startEffect(ANIM_SPARKLE);
    else if (strcmp(name, "WAVE")    == 0) startEffect(ANIM_WAVE);
    else if (strcmp(name, "NONE")    == 0) stopEffect();

  } else if (strncmp(msg, "SETHEX:", 7) == 0) {
    int hex = atoi(msg + 7);
    const char* colorStr = strchr(msg + 7, ':');
    if (colorStr) {
      colorStr++;
      uint32_t rgb = strtoul(colorStr, nullptr, 16);
      CRGB color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      setHexColor(hex, color);
    }

  } else if (strncmp(msg, "SETHEXSIDE:", 11) == 0) {
    int hex = atoi(msg + 11);
    const char* sidePtr = strchr(msg + 11, ':');
    if (sidePtr) {
      sidePtr++;
      int side = atoi(sidePtr);
      const char* colorStr = strchr(sidePtr, ':');
      if (colorStr) {
        colorStr++;
        uint32_t rgb = strtoul(colorStr, nullptr, 16);
        CRGB color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        setHexSideColor(hex, side, color);
      }
    }

  } else if (strncmp(msg, "ALL:", 4) == 0) {
    uint32_t rgb = strtoul(msg + 4, nullptr, 16);
    setAllHexes(CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));

  } else if (strncmp(msg, "BATTLE:", 7) == 0) {
    int attacker = atoi(msg + 7);
    const char* defPtr = strchr(msg + 7, ':');
    if (defPtr) queueWebCmd(WCMD_BATTLE, (uint8_t)attacker, (uint8_t)atoi(defPtr + 1));

  } else if (strcmp(msg, "ENDBATTLE") == 0) {
    queueWebCmd(WCMD_ENDBATTLE);

  } else if (strncmp(msg, "CLAIMHEX:", 9) == 0) {
    // CLAIMHEX:hexIdx:playerIdx  (playerIdx 255 = unclaim)
    int hexIdx = atoi(msg + 9);
    const char* pPtr = strchr(msg + 9, ':');
    if (pPtr && hexIdx >= 0 && hexIdx < NUM_HEXES) {
      int pIdx = atoi(pPtr + 1);
      if (pIdx < 0 || pIdx >= MAX_PLAYERS || !players[pIdx].active) {
        hexOwner[hexIdx] = -1;
        setHexColor(hexIdx, CRGB::Black);
      } else {
        hexOwner[hexIdx] = (int8_t)pIdx;
        CRGB c;
        c.r = (players[pIdx].colorHex >> 16) & 0xFF;
        c.g = (players[pIdx].colorHex >>  8) & 0xFF;
        c.b =  players[pIdx].colorHex        & 0xFF;
        setHexColor(hexIdx, c);
        // Claiming Mecatol Rex (center hex) means the custodians token is taken
        if (hexIdx == 30) gameOpts.custodiansTaken = true;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// URL decode helpers
// -----------------------------------------------------------------------------
static char urlDecodeChar(const char*& src) {
  if (*src == '%' && src[1] && src[2]) {
    char hex[3] = { src[1], src[2], 0 };
    src += 3;
    return (char)strtol(hex, nullptr, 16);
  }
  if (*src == '+') { src++; return ' '; }
  return *src++;
}

static void urlDecode(const char* src, char* dst, size_t dstLen) {
  size_t i = 0;
  while (*src && i < dstLen - 1) {
    dst[i++] = urlDecodeChar(src);
  }
  dst[i] = 0;
}

// -----------------------------------------------------------------------------
// parseSaveSettings — update rtCfg from /savesettings query string
// -----------------------------------------------------------------------------
void parseSaveSettings(const String& query) {
  int pos = 0;
  while (pos < (int)query.length()) {
    int eqPos  = query.indexOf('=', pos);
    if (eqPos < 0) break;
    int ampPos = query.indexOf('&', eqPos + 1);
    if (ampPos < 0) ampPos = query.length();

    String key = query.substring(pos, eqPos);
    String val = query.substring(eqPos + 1, ampPos);

    char decoded[128];
    urlDecode(val.c_str(), decoded, sizeof(decoded));

    if      (key == "homeSSID")          strncpy(rtCfg.homeSSID,  decoded, sizeof(rtCfg.homeSSID)  - 1);
    else if (key == "homePass")          strncpy(rtCfg.homePass,  decoded, sizeof(rtCfg.homePass)  - 1);
    else if (key == "apSSID")            strncpy(rtCfg.apSSID,    decoded, sizeof(rtCfg.apSSID)    - 1);
    else if (key == "apPass")            strncpy(rtCfg.apPass,    decoded, sizeof(rtCfg.apPass)    - 1);
    else if (key == "homeTimeoutMs")     rtCfg.homeTimeoutMs     = (uint32_t)atol(decoded);
    else if (key == "defaultBrightness") rtCfg.defaultBrightness = (uint8_t)constrain(atoi(decoded), 0, 255);
    else if (key == "maxBrightness")     rtCfg.maxBrightness     = (uint8_t)constrain(atoi(decoded), 0, 255);
    else if (key == "ledUpdateMs")       rtCfg.ledUpdateMs       = (uint16_t)constrain(atoi(decoded), 1, 1000);
    else if (key == "broadcastMs")       rtCfg.broadcastMs       = (uint16_t)constrain(atoi(decoded), 50, 5000);
    else if (key == "sideGap")           rtCfg.sideGap           = (uint8_t)constrain(atoi(decoded), 0, 20);
    else if (key == "simulateHardware")  rtCfg.simulateHardware  = (decoded[0] == '1');
    else if (key == "debugSerial")       rtCfg.debugSerial       = (decoded[0] == '1');
    else if (key == "debugWeb")          rtCfg.debugWeb          = (decoded[0] == '1');
    else if (key == "debugLed")          rtCfg.debugLed          = (decoded[0] == '1');
    else if (key == "debugKeyboard")     rtCfg.debugKeyboard     = (decoded[0] == '1');
    else if (key == "thinSides")         rtCfg.thinSides         = (decoded[0] == '1');

    pos = ampPos + 1;
  }

  FastLED.setBrightness(rtCfg.defaultBrightness);
  saveRuntimeSettings();
  if (rtCfg.debugSerial) Serial.println(F("Settings: updated OK"));
}
