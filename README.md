# TI4 Hex Riser Firmware

ESP32-S3 firmware for a 61-hex Twilight Imperium 4 LED riser table. Players use their phones as controllers over WiFi. Dual-core architecture: Core 0 drives the LED strip, Core 1 runs the game state machine, web commands, and serial commands.

## Hardware

| Component | Spec |
|---|---|
| Board | Lonely Binary ESP32-S3-DevKitC-1 v1.6 (Gold Edition) |
| Module | ESP32-S3-WROOM-1-N16R8 |
| CPU | Dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB QSPI |
| PSRAM | 8 MB Octal SPI |
| LEDs | 915x SK6812 (RGB, GRB order), GPIO 13 |
| Power | 5V 60A PSU |

## Required Libraries

Install via Arduino IDE Library Manager:

| Library | Author | Version | Purpose |
|---|---|---|---|
| FastLED | Daniel Garcia | **3.10.3** | LED control and animations |
| ESP Async WebServer | ESP32Async | latest | Async HTTP + WebSocket server |
| Async TCP | ESP32Async | latest | Required by ESP Async WebServer |

Keep FastLED on 3.10.3. Version 3.10.5 jams the LED driver on this strip and resets the board. Skip FastLED in the IDE's "updates available" prompt.

Built against the ESP32 Arduino core 3.3.x (Espressif Systems).

## USB Ports

The board has two USB-C ports:

- **USB**: the ESP32-S3's native USB. No driver needed. Use this one with the board settings below.
- **UART**: a CH343 USB-to-serial bridge. Needs the WCH driver on Windows and macOS ([Windows](https://www.wch-ic.com/downloads/CH343SER_EXE.html), [macOS](https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html)).

## Upload Steps

1. Open `TI4_HexRiser.ino` in Arduino IDE. The folder holding the sketch must be named `TI4_HexRiser`, so clone into that folder name.
2. Install all libraries above
3. Select **Tools > Board > ESP32 Arduino > ESP32S3 Dev Module**
4. Apply these board settings:

| Setting | Value |
|---|---|
| USB CDC On Boot | Enabled |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | OPI PSRAM |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |

5. Select **Tools > Port >** the COM port for the **USB** port
6. Click **Upload**

## Core Architecture

| Core | Responsibility |
|---|---|
| Core 0 | LED task, timing-critical, isolated from WiFi jitter |
| Core 1 | `loop()`: game state machine, web command queue, serial command handler; state broadcast task |

The LED task pushes frames at most every 30 ms (~33 fps). 915 LEDs take about 28 ms to send, so faster frames would only pile up in the driver. Game state writes to shared `hexColor[]` directly; the LED task picks up changes on the next frame.

## First Boot

1. Open Serial Monitor at **115200 baud**
2. The board will attempt to join the network saved in Settings (first boot uses the defaults in `config.h`), then fall back to AP mode
3. Connect your phone or laptop to WiFi **"TI4-HexRiser"** (password: **"twilight4"**) if using AP mode
4. Open **http://ti4table.local** in a browser, or the IP shown in Serial Monitor (AP mode: `http://192.168.4.1`)
5. The hex grid should appear and sync live with the LED state via WebSocket

## Web Interface

All live pages share a single WebSocket (`/ws`). The board pushes LED
state as a binary frame (all 366 hex sides) only when something changes, plus
a game-state JSON every 300 ms — no polling.

If no game state arrives for 1 second the pages show a **Table offline** splash and keep reconnecting. While the controller boots they show **Table is booting** until the boot animation finishes, and after a restart they reload themselves so they always match the firmware.

| Page | Who | What |
|---|---|---|
| `/` | Everyone | Home page with links to Admin, Player and Projector |
| `/projector` | Projector / TV | Full-screen live board mirror with map tiles, player list, and faction icons on claimed hexes. Display only |
| `/play` (or `/player`) | Players 1-8 | Phone keypad: claim a seat, then a phase-aware pad (faction and color select, strategy cards, end turn / pass / battle, ready, agenda) |
| `/admin` | Game master | Desktop and mobile. Live board with map building and hex claiming, player count, force start, reset, phase jumps, custom rules, seat roster with kick, speaker token, battle, lighting |
| `/settings` | — | WiFi, LED, debug, and display settings at runtime |

### Playing from phones

1. Game master opens `/admin` and sets the player count (this restarts setup and assigns home hexes)
2. Each player opens `/play` on their phone, enters a name, and grabs an open seat
3. Seats survive phone screen locks and reconnects — the claim token is stored in the browser
4. During setup, players can pick a faction. Its icon shows next to their name on the admin page and projector, and its home system appears on their home hex
5. Every keypad press goes through `handleGameKey()`, the same path as the `kb` serial command

### Map tiles

Tile art and faction icons load from the [TI4 image CDN](https://evident-dev.github.io/TI4-Images/index.json). The board stores only tile numbers and faction ids. With no internet (AP mode), pages show plain hexes.

The map can only be changed during setup, from the admin page:

- **One hex:** tap a hex, type the tile number, then **Set Tile**. Hyperlanes get rotate buttons while selected.
- **Whole map:** paste a TTS map string (for example from Milty Draft) and press **Load Map**. Mecatol Rex is added when the string leaves it out, and empty home spots fill in from each player's faction.

The map is saved to flash and stays until **Clear Map**. Hex numbers show during setup and hide once the game starts.

Claiming hexes is optional. Claiming the center hex marks the custodians token taken.

### Custom rules (admin page)

Each button shows the phase it affects and its current setting. Tap to switch.

- **Agenda: After Custodians / Every Round** (default After Custodians): with After Custodians, the agenda phase is skipped after status until the custodians token leaves Mecatol Rex. Claiming the center hex on the board marks it automatically; the admin can also toggle it.
- **Strategy: 2 Cards Each / 1 Card Each** (default 2, shown only for 4 players): in 4-player games each player picks two strategy cards, going around the pick order twice. Initiative is the lowest card held.
- **Speaker token** — no card grants the speaker token; when Politics changes the speaker, use the crown button on the seat roster.
- **Turn override** — during the action phase, the Turn button on a seat makes it that player's turn. Play continues in initiative order from them.

## Power Loss Recovery

Once a game has started, the controller saves it to flash whenever something changes: a turn ends, a player passes, a card is locked, a hex is claimed, or the phase changes. A save either completes or leaves the previous one intact, so a power cut mid-save can't corrupt it.

After a power loss, every page shows **Saved game found** with the phase and player count:

- Players and the admin can **Resume**. The admin can also **Start New Game**, which discards the save.
- On resume, seats come back **reserved**. Each phone stores its seat token with the game ID and rejoins its own seat automatically. Anyone else can still take a reserved seat, so a phone that never comes back doesn't block the game.
- The active player's turn timer restarts.

The save is cleared by Start New Game, Reset Game, or changing the player count. Nothing is saved during setup, so a new setup never prompts.

## Settings Page

Open Settings from the admin page to change runtime settings. Saving writes them to flash, so they survive reboots and reflashing. LED and debug changes take effect immediately; network changes need a reboot.

Saved settings override the defaults in `config.h`. Settings go back to the `config.h` defaults only if flash is erased or the settings layout changes in a firmware update.

| Setting | Description |
|---|---|
| Network SSID / Password | WiFi network to connect to first |
| Hostname | Name the board answers to on any network, as `http://<name>.local` (default `ti4table`) |
| Keep AP On | Keep the board's own WiFi on after joining a network, so it's always reachable at `192.168.4.1` |
| AP SSID / Password | Fallback access point credentials |
| Network Timeout | How long to wait for the network before switching to AP |
| Default Brightness | Startup brightness (0-255) |
| Max Brightness | Hard cap for the brightness slider |
| LED Update Rate | Animation tick interval in ms (30 ms minimum is enforced) |
| Broadcast Rate | How often the board pushes state to the browser (ms) |
| Side Gap | Inset of the colored side lines in the browser (0 = touching, higher = more gap) |
| Side Width | Normal or Thin colored side lines in the browser |
| Simulate Hardware | Skip FastLED.show() -- use this when testing without the strip connected |
| Debug flags | Enable serial logging for various subsystems |

## Serial Commands

Open Serial Monitor at **115200 baud**. All commands are case-insensitive where noted.

### Game Simulation Commands

These simulate phone keypad presses and game flow for testing without phones.

| Command | Effect |
|---|---|
| `setplayers <4-8>` | Set how many players are active and restart the setup phase |
| `kb <1-8> <0-15>` | Simulate player N pressing key K on their phone keypad |
| `startgame` | GM force-start: locks any unlocked players and runs speaker selection |
| `phase <0-4>` | Jump directly to a phase (0=Setup, 1=Strategy, 2=Action, 3=Status, 4=Agenda) |
| `battle <P1> <P2>` | Trigger battle mode between two players (e.g. `battle 1 3`) |
| `status` | Print current phase, all player states, home hexes, and WiFi IP |

### Key Reference

The phone keypad sends these key numbers, and `kb` uses them too. What each key does depends on the current phase:

| Key | Setup phase | Strategy phase | Action phase | Status phase | Agenda phase |
|---|---|---|---|---|---|
| 1-8 | Select color (preview only, not locked yet) | Select strategy card 1-8 | -- | -- | -- |
| 13 | -- | -- | End battle mode | -- | -- |
| 14 | -- | -- | Pass this round (on your turn) | -- | -- |
| 15 | Lock in color choice | Lock in strategy card and hand off | End turn | Mark ready | End agenda (speaker only) |
| 0 | Start game (any player, only after all locked) | -- | -- | -- | -- |

### Full Round Walkthrough

The board boots with 6 players active by default. To simulate a complete round:

**Setup phase (color selection)**
```
status                  check starting state
setplayers 4            use a 4-player game for simplicity
kb 1 1                  P1 previews Red
kb 1 15                 P1 locks Red
kb 2 2                  P2 previews Blue
kb 2 15                 P2 locks Blue
kb 3 3                  P3 previews Green
kb 3 15                 P3 locks Green
kb 4 4                  P4 previews Yellow
kb 4 15                 P4 locks Yellow
kb 1 0                  GM starts game -- speaker roulette runs, then moves to Strategy
```

**Strategy phase (card selection)**
```
status                  check pick order (speaker goes first)
kb 1 1                  current picker selects card 1 (Leadership)
kb 1 15                 picker locks card, next player's home hex starts pulsing
kb 2 4                  next picker selects card 4 (Construction)
kb 2 15                 locks, next picker
kb 3 6                  selects card 6 (Warfare)
kb 3 15                 locks
kb 4 8                  selects card 8 (Imperial)
kb 4 15                 locks -- all done, center pulse plays, moves to Action
```

**Action phase (taking turns)**

Initiative order is determined by card number (lowest card = first turn).
```
status                  check initiative order
kb 1 15                 active player ends their turn, passes to next
kb 2 15                 next player ends turn
kb 3 14                 P3 passes this round (home hex dims to 50%)
kb 4 15                 P4 ends turn
kb 1 15                 P1 ends turn again (cycling through non-passed players)
kb 2 14                 P2 passes
kb 4 14                 P4 passes
kb 1 14                 P1 passes -- all passed, moves to Status automatically
```

**Status phase (end-of-round cleanup)**

The board splits into radial slices, one per player, each pulsing their color.
```
kb 1 15                 P1 marks ready (their slice goes solid)
kb 2 15                 P2 marks ready
kb 3 15                 P3 marks ready
kb 4 15                 P4 marks ready -- all ready, moves to Agenda automatically
```

**Agenda phase**
```
status                  check who the speaker is
kb 1 15                 speaker ends agenda -- round resets, moves back to Strategy
```

### Utility Commands

| Command | Effect |
|---|---|
| `effect rainbow` | Start rainbow animation |
| `effect pulse` | Pulsing glow with slow hue drift |
| `effect ripple` | Rings ripple outward from center hex |
| `effect sparkle` | Random sparkle across all hexes |
| `effect wave` | Color wave sweeping left to right |
| `effect none` | Stop animation and clear |
| `bright N` | Set brightness 0-200 |
| `clear` | Clear all hexes |
| `test` | Run LED hardware test (scans all 915 LEDs) |

## File Map

| File | Purpose |
|---|---|
| `TI4_HexRiser.ino` | Main sketch: setup, loop, serial handler, key/hex callbacks, LED task (Core 0) |
| `config.h` | Edit this -- pins, default WiFi credentials, brightness limits, debug flags, game constants |
| `runtime_settings.h` | Live config updated by the settings page, saved to flash |
| `game_state.h` | Full game state machine: all phases, player data, key dispatch |
| `animations.h` | Boot snake animation and center-out phase transition |
| `led_map.h` | HEX_MAP[61][6][3] mapping hex/side/slot to LED index (0-914) |
| `hex_neighbors.h` | HEX_NEIGHBORS[61][6] adjacency table |
| `led_control.h` | FastLED init, per-hex color management, 5 lighting effects |
| `keyboard_control.h` | Unused stub left from the dropped physical keyboards |
| `edge_map.h` | Outward-facing hex sides per player for perimeter turn lighting |
| `home_page.h` | Home page served from root, with the galaxy background |
| `theme_style.h` | Shared holo button and panel theme served at /theme.css |
| `projector_page.h` | Full-screen board page served at /projector |
| `board_script.h` | Board renderer and WebSocket client shared by projector and admin, served at /board.js |
| `play_page.h` | Player keypad page served at /play |
| `admin_page.h` | Game master page served at /admin |
| `settings_page.h` | Settings page HTML served at /settings |
| `web_server.h` | WiFi station+AP, WebSocket state push, seat claims, command queue |
| `save_state.h` | Power loss recovery: saves the running game to flash and restores it |
| `map_state.h` | Tile number and rotation per hex, saved to flash |
| `tile_script.h` | Tile and faction image loader and TTS map string parser, served at /tiles.js |

## LED Map

Each hex has 15 LEDs across 6 sides. The strip enters each hex mid-side:

```
Side 0 (top):          2 LEDs  -- slots [base+14, base+0]
Side 1 (top-right):    3 LEDs  -- slots [base+1,  base+2,  base+3]
Side 2 (bottom-right): 2 LEDs  -- slots [base+4,  base+5]
Side 3 (bottom):       3 LEDs  -- slots [base+6,  base+7,  base+8]
Side 4 (bottom-left):  2 LEDs  -- slots [base+9,  base+10]
Side 5 (top-left):     3 LEDs  -- slots [base+11, base+12, base+13]
```

Where `base = hex_index * 15`. Hex 30 is the center hex.

## Wiring Reference

```
ESP32-S3-WROOM-1          SK6812 strip
GPIO 13 ─────────────── DIN (LED 0 end)
GND     ─────────────── GND
                         5V from external PSU
```

## Power Notes

- At 50% brightness (default 128): approximately 27.5A draw from LEDs
- At max brightness (200): approximately 44A
- Minimum recommended PSU: 5V 60A
- Connect PSU ground to ESP32 ground
- Do not power the LED strip from the ESP32 3.3V or 5V pins

## Troubleshooting

**Serial port not detected:** Use the **USB** port with USB CDC On Boot set to Enabled. On the **UART** port, install the CH343 driver (see USB Ports above).

**Compile error about `setTxTimeoutMs`, or no serial output:** The Tools menu settings don't match. Re-apply the board settings in Upload Steps; the IDE resets them to defaults when the sketch folder changes.

**LEDs don't light:** Check GPIO 13 data wire, confirm PSU is powered, verify shared GND between PSU and ESP32-S3. On ESP32-S3, avoid GPIO 0, 45, 46 (strapping pins) for LED data — GPIO 13 is safe.

**Web page won't load:** Confirm you are connected to the correct WiFi; check Serial Monitor for the IP address. Older Android phones and some guest networks can't open `.local` names: use the IP, or join the board's own WiFi and open `http://192.168.4.1`.

**Moving to a new WiFi:** join the board's own WiFi (`TI4-HexRiser`), open `http://192.168.4.1/settings`, enter the new network, save, and reboot. Then open `http://ti4table.local` from the new network.

**WebSocket not connecting:** Hard-refresh the browser (Ctrl+Shift+R). If the board rebooted, the WebSocket client reconnects automatically within a few seconds.

**Animation not showing in browser:** Enable "Simulate Hardware" in Settings when testing without the strip connected.

**Lighting effects lag, then the board resets (`rmt` / `ChannelManager` errors in serial):** FastLED was updated past 3.10.3. Reinstall FastLED 3.10.3 from the Library Manager.

**Wrong colors:** Edit `LED_COLOR_ORDER` in `config.h` (try `GRB`, `RGB`, or `BGR`).

**Compile errors about ESPAsyncWebServer:** Install "ESP Async WebServer" and "Async TCP" by ESP32Async from the Library Manager. The old me-no-dev versions don't build on ESP32 core 3.x.

**Watchdog reset / core panic:** If the LED task triggers a watchdog, increase the `vTaskDelay` in `ledTask()`.

**PSRAM not detected:** Confirm **Tools > PSRAM > OPI PSRAM** is selected in Arduino IDE. The N16R8 module has 8MB Octal SPI PSRAM — if disabled, large buffers may cause heap exhaustion.
