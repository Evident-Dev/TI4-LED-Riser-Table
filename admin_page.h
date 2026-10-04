#pragma once

// =============================================================================
// TI4 Hex Riser - Admin Page (/admin)
// =============================================================================
// Game master controls for desktop and mobile: live board with map building
// and hex claiming, player count, seat roster, phase flow, rules, battle and
// lighting.
// Desktop shows the board beside the controls; mobile stacks them.
// =============================================================================

const char ADMIN_PAGE[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TI4 Admin</title>
<link rel="stylesheet" href="/theme.css">
<style>
* { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
:root {
  --border: #38d6ff1f;
  --text: #cbd5e1; --muted: #6b8aa6; --accent: #38d6ff; --gold: #fbbf24;
  --topbar-height: 52px;
}
body {
  color: var(--text); min-height: 100vh;
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
}

/* Top bar */
#topbar {
  position: sticky; top: 0; z-index: 10; height: var(--topbar-height);
  display: flex; align-items: center; gap: 12px; padding: 0 16px;
}
.dot { width: 9px; height: 9px; border-radius: 50%; background: #7f1d1d; transition: background 0.3s; flex-shrink: 0; }
.dot.on { background: #22c55e; }
#topbar h1 { font-size: 0.85rem; letter-spacing: 0.2em; text-transform: uppercase; color: var(--gold); white-space: nowrap; text-shadow: 0 0 12px #fbbf2455; }
#turn-info { flex: 1; font-size: 0.8rem; color: var(--muted); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
#turn-info b { color: var(--text); }
.top-nav { display: flex; gap: 14px; }
.top-nav a { color: var(--muted); font-size: 0.72rem; letter-spacing: 0.12em; text-transform: uppercase; text-decoration: none; white-space: nowrap; }
.top-nav a:hover { color: var(--accent); }
.top-nav a.reboot:hover, .footer-nav a.reboot:hover { color: #f87171; }

/* Layout */
#layout { display: grid; grid-template-columns: 1fr; gap: 16px; padding: 16px; max-width: 1800px; margin: 0 auto; }
#controls { display: grid; grid-template-columns: 1fr; gap: 16px; align-content: start; }
.section { padding: 16px; }
.hint { font-size: 0.72rem; color: var(--muted); margin-top: 8px; line-height: 1.4; }

/* Board panel */
#board-panel { display: flex; flex-direction: column; gap: 12px; }
#board { display: block; width: 100%; aspect-ratio: 1.06; max-height: 70vh; }
.tool-row { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
#selected-hex { font-size: 0.85rem; color: var(--muted); min-width: 90px; }
#selected-hex b { color: #7fe6ff; }
.swatch {
  --cut: 7px; --edge: #33415599; --edge-dim: #33415555;
  display: flex; align-items: center; gap: 7px; padding: 7px 12px; min-height: 38px;
  font-size: 0.78rem; cursor: pointer; text-transform: none; letter-spacing: 0.02em;
}
.swatch.selected { --edge: #ffffff; --edge-dim: #ffffffaa; --edge-width: 2px; }
.swatch-dot { width: 13px; height: 13px; border-radius: 50%; flex-shrink: 0; }
.tool-buttons { display: grid; grid-template-columns: repeat(2, minmax(110px, 1fr)); gap: 8px; }
#map-tools { display: flex; flex-direction: column; gap: 10px; }
#map-tools input[type=text] { flex: 1; min-width: 0; min-height: 40px; padding: 8px 10px; font-size: 0.85rem; }
#map-tools .tool-row { flex-wrap: nowrap; }
#map-error { font-size: 0.75rem; color: #f87171; }
.seat-icon { width: 22px; height: 22px; object-fit: contain; flex-shrink: 0; }

/* Controls */
button { min-height: 40px; font-size: 0.75rem; padding: 9px 12px; }
.row { display: flex; gap: 8px; }
.row > * { flex: 1; }
.count-grid { display: grid; grid-template-columns: repeat(5, 1fr); gap: 8px; }
.phase-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.fx-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
.seat-row {
  display: flex; flex-wrap: wrap; align-items: center; gap: 8px; padding: 8px 2px;
  border-bottom: 1px solid var(--border); font-size: 0.85rem;
}
.seat-row:last-child { border-bottom: none; }
.seat-dot { width: 14px; height: 14px; border-radius: 50%; border: 2px solid #ffffff22; flex-shrink: 0; }
.seat-name { flex: 1; min-width: 120px; }
.seat-name .sub { font-size: 0.7rem; color: var(--muted); }
.small { min-height: 32px; padding: 4px 10px; font-size: 0.72rem; }
.bright-row { display: flex; align-items: center; gap: 10px; }
input[type=range] { flex: 1; accent-color: var(--accent); height: 28px; }
#bright-val { font-size: 0.8rem; color: var(--muted); min-width: 28px; text-align: right; }
select { min-height: 40px; font-size: 0.85rem; padding: 8px; }
.footer-nav { display: none; text-align: center; font-size: 0.85rem; padding: 4px 0 20px; }
.footer-nav a { color: var(--muted); text-decoration: none; margin: 0 10px; line-height: 2.2; }

/* Desktop: board beside the controls */
@media (min-width: 1000px) {
  #layout { grid-template-columns: minmax(0, 1.25fr) minmax(380px, 1fr); align-items: start; }
  #board-panel { position: sticky; top: calc(var(--topbar-height) + 16px); }
  #board { max-height: calc(100vh - var(--topbar-height) - 200px); }
}
@media (min-width: 1500px) {
  #controls { grid-template-columns: 1fr 1fr; }
}

/* Phone */
@media (max-width: 700px) {
  #topbar { gap: 10px; padding: 0 12px; }
  .top-nav { display: none; }
  #turn-info { display: none; }
  #turn-info-mobile:not(:empty) { display: block; }
  #layout { padding: 12px; gap: 12px; }
  .footer-nav { display: block; }
  .phase-grid { grid-template-columns: repeat(2, 1fr); }
}
#turn-info-mobile { display: none; padding: 8px 12px 0; font-size: 0.82rem; color: var(--muted); }
#turn-info-mobile b { color: var(--text); }
</style>
<script src="/tiles.js"></script>
<script src="/board.js"></script>
</head>
<body>

<div id="topbar" class="holo-bar">
  <div class="dot" id="ws-dot"></div>
  <h1>Game Master</h1>
  <div id="phase-pill" class="tag gold">&mdash;</div>
  <div id="turn-info"></div>
  <div class="top-nav">
    <a href="/">Home</a>
    <a href="/projector">Projector</a>
    <a href="/play">Player Pad</a>
    <a href="/settings">&#9881; Settings</a>
    <a href="#" class="reboot" onclick="rebootController(); return false;">&#x21bb; Reboot</a>
  </div>
</div>
<div id="turn-info-mobile"></div>

<div id="layout">

  <div class="panel section" id="board-panel">
    <svg id="board"></svg>
    <div id="map-tools" hidden>
      <div class="panel-label">Map</div>
      <div class="tool-row">
        <div id="map-selected-hex">No hex selected</div>
        <input type="text" id="tile-input" placeholder="Tile number" maxlength="7" autocomplete="off">
        <button onclick="setSelectedTile()">Set Tile</button>
        <button onclick="clearSelectedTile()">Clear</button>
      </div>
      <div class="tool-row" id="rotate-row" hidden>
        <button onclick="rotateSelectedTile(-1)">&#8634; Rotate Left</button>
        <button onclick="rotateSelectedTile(1)">Rotate Right &#8635;</button>
      </div>
      <div class="tool-row">
        <input type="text" id="map-string-input" placeholder="TTS map string" autocomplete="off">
        <button onclick="loadMapString()">Load Map</button>
        <button class="danger" onclick="clearWholeMap()">Clear Map</button>
      </div>
      <div id="map-error" hidden></div>
    </div>
    <div id="claim-tools" hidden>
      <div class="tool-row" id="player-swatches"></div>
      <div class="tool-row" style="margin-top:12px">
        <div id="selected-hex">No hex selected</div>
        <div class="tool-buttons">
          <button onclick="claimHex()">Claim Hex</button>
          <button onclick="clearHex()">Clear Hex</button>
        </div>
      </div>
    </div>
  </div>

  <div id="controls">

    <div class="panel section" id="section-players" hidden>
      <div class="panel-label">Players</div>
      <div class="count-grid" id="count-grid"></div>
      <div class="hint">Setting the player count restarts the setup phase and assigns home hexes. Players then join from their phones at <b>/play</b>.</div>
    </div>

    <div class="panel section">
      <div class="panel-label">Seats</div>
      <div id="seat-list"></div>
    </div>

    <div class="panel section">
      <div class="panel-label">Game Flow</div>
      <div class="row">
        <button class="gold" id="force-start" hidden onclick="confirmDo('Force start the game? Unlocked players get their previewed colors.', function(){ table.send('ADMIN:STARTGAME'); }, { confirmLabel: 'Force Start' })">&#9733; Force Start</button>
        <button class="danger" onclick="confirmDo('Reset the whole game back to setup?', function(){ table.send('ADMIN:RESET'); }, { confirmLabel: 'Reset Game', danger: true })">Reset Game</button>
      </div>
      <div id="game-controls" hidden>
        <div class="phase-grid" id="phase-grid" style="margin-top:8px"></div>
        <div class="row" style="margin-top:8px">
          <button id="opt-cust" onclick="toggleCustodians()">Custodians taken</button>
        </div>
        <div class="hint">Phase jump skips the normal flow — use it to fix mistakes or demo the table.</div>
      </div>
    </div>

    <div class="panel section" id="section-rules" hidden>
      <div class="panel-label">Custom Rules</div>
      <div class="row">
        <button id="opt-ag" onclick="toggleOption('AG')">Agenda: After Custodians</button>
        <button id="opt-dbl" onclick="toggleOption('DBL')">Strategy: 2 Cards Each</button>
      </div>
      <div class="hint" id="agenda-hint">The agenda phase is skipped until the custodians token leaves Mecatol Rex. Claiming the center hex marks it automatically.</div>
    </div>

    <div class="panel section" id="section-battle" hidden>
      <div class="panel-label">Battle</div>
      <div class="row">
        <select id="battle-attacker"></select>
        <select id="battle-defender"></select>
      </div>
      <div class="row" style="margin-top:8px">
        <button class="gold" onclick="startBattle()">&#9876; Start Battle</button>
        <button class="danger" onclick="table.send('ENDBATTLE')">End Battle</button>
      </div>
    </div>

    <div class="panel section" id="section-lighting" hidden>
      <div class="panel-label">Lighting</div>
      <div class="bright-row">
        <input type="range" id="brightness" min="0" max="200" value="128">
        <span id="bright-val">128</span>
      </div>
      <div class="fx-grid" style="margin-top:10px">
        <button id="fx-RAINBOW" onclick="effect('RAINBOW')">Rainbow</button>
        <button id="fx-PULSE"   onclick="effect('PULSE')">Pulse</button>
        <button id="fx-RIPPLE"  onclick="effect('RIPPLE')">Ripple</button>
        <button id="fx-SPARKLE" onclick="effect('SPARKLE')">Sparkle</button>
        <button id="fx-WAVE"    onclick="effect('WAVE')">Wave</button>
        <button class="danger"  onclick="effect('NONE')">&#9632; Stop</button>
      </div>
    </div>

    <div class="footer-nav">
      <a href="/">Home</a>
      <a href="/projector">Projector</a>
      <a href="/play">Player Pad</a>
      <a href="/settings">&#9881; Settings</a>
      <a href="#" class="reboot" onclick="rebootController(); return false;">&#x21bb; Reboot</a>
    </div>

  </div>
</div>

<script>
var PHASE_NAMES = ['Setup','Strategy','Action','Status','Agenda'];
var EFFECT_NAMES = ['RAINBOW','PULSE','RIPPLE','SPARKLE','WAVE'];
var game = null;
var map = null;
var tableData = null;
var selectedHex = -1;
var selectedPlayer = -1;

function confirmDo(message, action, options) { confirmModal(message, action, options); }
function rebootController() {
  confirmDo('Reboot the table controller?', function () {
    fetch('/reboot').catch(function () {});
  }, { confirmLabel: 'Reboot', danger: true });
}

var board = createHexBoard(document.getElementById('board'), { onHexClick: selectHex });
loadTableData().then(function (data) { tableData = data; if (game) render(); });
var table = connectTable({
  onFrame: board.renderLedFrame,
  onMap: function (newMap) {
    map = newMap;
    board.setMap(map);
    renderMapTools();
  },
  onGame: function (newGame) {
    game = newGame;
    board.setGame(game);
    showRecoveryPrompt(game, {
      onResume: function () { table.send('RESUME'); },
      onNewGame: function () {
        confirmDo('Discard the saved game and start a new one?', function () { table.send('ADMIN:NEWGAME'); },
                  { confirmLabel: 'Start New Game', danger: true });
      }
    });
    render();
  },
  onOnline: function (online) { document.getElementById('ws-dot').classList.toggle('on', online); }
});

// ============================================================
// Board tools
// ============================================================
function selectHex(hexIndex) {
  selectedHex = hexIndex;
  board.setSelectedHex(hexIndex);
  document.getElementById('selected-hex').innerHTML = 'Hex <b>' + hexIndex + '</b>';
  document.getElementById('map-selected-hex').innerHTML = 'Hex <b>' + hexIndex + '</b>';
  var placed = board.tileAt(hexIndex);
  document.getElementById('tile-input').value = placed ? placed.tileNumber : '';
  showMapError('');
  renderMapTools();
}
function selectPlayer(playerIndex) {
  selectedPlayer = playerIndex;
  document.querySelectorAll('.swatch').forEach(function (swatch) {
    swatch.classList.toggle('selected', +swatch.getAttribute('data-player') === playerIndex);
  });
}
function claimHex() {
  if (selectedHex < 0 || selectedPlayer < 0) return;
  table.send('CLAIMHEX:' + selectedHex + ':' + selectedPlayer);
}
function clearHex() {
  if (selectedHex < 0) return;
  table.send('CLAIMHEX:' + selectedHex + ':255');
}

// ============================================================
// Map
// ============================================================
function showMapError(message) {
  var error = document.getElementById('map-error');
  error.textContent = message;
  error.hidden = !message;
}
function sendTile(hexIndex, tileNumber, rotation) {
  table.send('ADMIN:TILE:' + hexIndex + ':' + (rotation || 0) + ':' + tileNumber);
}
// Index lookups only work with the CDN; offline any tile number is accepted
function knownTileNumber(tileNumber) {
  if (!tableData) return tileNumber;
  var tile = tileInfo(tableData, tileNumber);
  if (!tile) return null;
  return tile.image.replace(/^.*ST_|\.png$/g, '');
}
function setSelectedTile() {
  if (selectedHex < 0) return;
  var typed = document.getElementById('tile-input').value.trim();
  if (!typed) { clearSelectedTile(); return; }
  var tileNumber = knownTileNumber(typed);
  if (!tileNumber) { showMapError('Tile ' + typed + ' not found'); return; }
  showMapError('');
  sendTile(selectedHex, tileNumber, 0);
}
function clearSelectedTile() {
  if (selectedHex < 0) return;
  document.getElementById('tile-input').value = '';
  sendTile(selectedHex, '', 0);
}
function rotateSelectedTile(direction) {
  if (selectedHex < 0 || !map || !map.tiles[selectedHex]) return;
  var rotation = (+map.rot.charAt(selectedHex) + direction + 6) % 6;
  sendTile(selectedHex, map.tiles[selectedHex], rotation);
}
document.getElementById('tile-input').addEventListener('keydown', function (event) {
  if (event.key === 'Enter') setSelectedTile();
});

function mapHasTiles() {
  return !!map && map.tiles.some(function (tileNumber) { return !!tileNumber; });
}
function loadMapString() {
  var parsed = parseTtsMapString(document.getElementById('map-string-input').value);
  if (parsed.tiles.length === 0) { showMapError('Paste a TTS map string first'); return; }
  var unknown = [];
  function resolve(entry) {
    if (!entry.tileNumber) return entry;
    var tileNumber = knownTileNumber(entry.tileNumber);
    if (!tileNumber) { unknown.push(entry.tileNumber); return { tileNumber: '', rotation: 0 }; }
    return { tileNumber: tileNumber, rotation: entry.rotation };
  }
  var center = parsed.center ? resolve(parsed.center) : { tileNumber: '18', rotation: 0 };
  var tiles = parsed.tiles.slice(0, board.ttsOrder.length - 1).map(resolve);

  function apply() {
    table.send('ADMIN:CLEARMAP');
    sendTile(board.ttsOrder[0], center.tileNumber, center.rotation);
    tiles.forEach(function (entry, position) {
      if (entry.tileNumber) sendTile(board.ttsOrder[position + 1], entry.tileNumber, entry.rotation);
    });
    showMapError(unknown.length ? 'Skipped unknown tiles: ' + unknown.join(', ') : '');
    document.getElementById('map-string-input').value = '';
  }
  if (mapHasTiles()) confirmDo('Replace the current map?', apply, { confirmLabel: 'Replace Map' });
  else apply();
}
function clearWholeMap() {
  confirmDo('Clear every tile from the map?', function () { table.send('ADMIN:CLEARMAP'); },
            { confirmLabel: 'Clear Map', danger: true });
}

function renderMapTools() {
  var tile = (selectedHex >= 0 && map) ? tileInfo(tableData, map.tiles[selectedHex]) : null;
  document.getElementById('rotate-row').hidden = !(tile && tile.type === 'hyperlane');
}

// ============================================================
// Game controls
// ============================================================
function setPlayers(count) {
  confirmDo('Set ' + count + ' players and restart setup?', function () { table.send('ADMIN:SETPLAYERS:' + count); }, { confirmLabel: 'Restart Setup' });
}
function jumpPhase(phase) {
  confirmDo('Jump to the ' + PHASE_NAMES[phase] + ' phase?', function () { table.send('ADMIN:PHASE:' + phase); }, { confirmLabel: 'Jump' });
}
function startBattle() {
  var attacker = document.getElementById('battle-attacker').value;
  var defender = document.getElementById('battle-defender').value;
  if (attacker === defender) return;
  table.send('ADMIN:BATTLE:' + attacker + ':' + defender);
}
function kick(seat) {
  confirmDo('Kick this player from their seat? Their phone loses control until they claim again.', function () {
    table.send('ADMIN:KICK:' + seat);
  }, { confirmLabel: 'Kick', danger: true });
}
function makeTurn(seat) {
  confirmDo("Make it " + playerLabel(seat) + "'s turn?", function () { table.send('ADMIN:TURN:' + seat); }, { confirmLabel: 'Give Turn' });
}
function makeSpeaker(seat) {
  confirmDo('Give the speaker token to ' + playerLabel(seat) + '?', function () { table.send('ADMIN:SPEAKER:' + seat); }, { confirmLabel: 'Give Token' });
}
function toggleOption(which) {
  if (!game) return;
  var current = which === 'AG' ? game.agOpt : game.dbl;
  table.send('ADMIN:OPT:' + which + ':' + (current ? 0 : 1));
}
function toggleCustodians() {
  if (!game) return;
  table.send('ADMIN:CUST:' + (game.cust ? 0 : 1));
}
function effect(name) {
  table.send('EFFECT:' + name);
  EFFECT_NAMES.forEach(function (effectName) {
    document.getElementById('fx-' + effectName).classList.toggle('on', effectName === name);
  });
}
var brightnessSlider = document.getElementById('brightness');
brightnessSlider.addEventListener('input', function () {
  document.getElementById('bright-val').textContent = brightnessSlider.value;
  table.send('BRIGHTNESS:' + brightnessSlider.value);
});

// ============================================================
// Rendering
// ============================================================
function element(tag, className, html) {
  var created = document.createElement(tag);
  if (className) created.className = className;
  if (html != null) created.innerHTML = html;
  return created;
}
function playerLabel(playerIndex) {
  var player = game && game.players[playerIndex];
  return (player && (player.st || player.rs) && player.nm) ? player.nm : 'P' + (playerIndex + 1);
}

(function () {
  var grid = document.getElementById('count-grid');
  for (var count = 4; count <= 8; count++) {
    var button = element('button', null, String(count));
    button.id = 'count-' + count;
    button.onclick = (function (playerCount) { return function () { setPlayers(playerCount); }; })(count);
    grid.appendChild(button);
  }
})();

// Setup is not listed; Reset Game is the way back there
(function () {
  var grid = document.getElementById('phase-grid');
  PHASE_NAMES.forEach(function (name, phase) {
    if (phase === 0) return;
    var button = element('button', null, name);
    button.id = 'phase-' + phase;
    button.onclick = function () { jumpPhase(phase); };
    grid.appendChild(button);
  });
})();

function renderTurnInfo() {
  var info = '';
  if (game.phase === 1 && game.picker >= 0) info = 'Picking: <b>' + playerLabel(game.picker) + '</b>';
  else if (game.phase === 2 && game.atk >= 0) info = '&#9876; <b>' + playerLabel(game.atk) + '</b> vs <b>' + playerLabel(game.def) + '</b>';
  else if (game.phase === 2 && game.turn >= 0) info = 'Turn: <b>' + playerLabel(game.turn) + '</b>';
  if (game.phase > 0) info += (info ? ' &nbsp;&middot;&nbsp; ' : '') + 'Speaker: ' + playerLabel(game.speaker);
  document.getElementById('turn-info').innerHTML = info;
  document.getElementById('turn-info-mobile').innerHTML = info;
}

function renderSwatches() {
  var container = document.getElementById('player-swatches');
  container.innerHTML = '';
  game.players.forEach(function (player, playerIndex) {
    if (!player.a) return;
    var swatch = element('div', 'holo swatch' + (playerIndex === selectedPlayer ? ' selected' : ''));
    swatch.setAttribute('data-player', playerIndex);
    var dot = element('div', 'swatch-dot');
    dot.style.background = '#' + player.col;
    swatch.appendChild(dot);
    swatch.appendChild(document.createTextNode(playerLabel(playerIndex)));
    swatch.onclick = function () { selectPlayer(playerIndex); };
    container.appendChild(swatch);
  });
  if (selectedPlayer >= 0 && !game.players[selectedPlayer].a) selectedPlayer = -1;
}

function renderSeats() {
  var list = document.getElementById('seat-list');
  list.innerHTML = '';
  game.players.forEach(function (player, seat) {
    var row = element('div', 'seat-row');
    var dot = element('div', 'seat-dot');
    dot.style.background = player.a ? ('#' + player.col) : '#222';
    row.appendChild(dot);

    var iconUrl = player.a ? factionIconUrl(tableData, player.fa) : null;
    if (iconUrl) {
      var icon = element('img', 'seat-icon');
      icon.src = iconUrl;
      icon.alt = '';
      row.appendChild(icon);
    }

    var name = element('div', 'seat-name');
    if (!player.a) {
      name.innerHTML = '<span style="color:#334155">Seat ' + (seat + 1) + '</span> <span class="sub">not in game</span>';
    } else {
      var nameText = document.createElement('span');
      if (player.st || player.rs) nameText.textContent = player.nm;
      else nameText.innerHTML = '<span style="color:#475569">Seat ' + (seat + 1) + ' open</span>';
      name.appendChild(nameText);
      var seatStatus = player.st ? ' &middot; phone connected' : player.rs ? ' &middot; reserved, waiting for phone' : ' &middot; /play to join';
      var faction = player.fa && tableData ? tableData.factions[player.fa] : null;
      if (faction) seatStatus = ' &middot; ' + faction.name + seatStatus;
      name.appendChild(element('div', 'sub', 'Seat ' + (seat + 1) + seatStatus));
    }
    row.appendChild(name);

    if (player.a) {
      if (seat === game.speaker && game.phase > 0) row.appendChild(element('span', 'tag gold', 'Speaker'));
      if (game.phase === 0 && player.cl) row.appendChild(element('span', 'tag green', 'Locked'));
      if (player.pk > 0 && player.sc > 0) {
        row.appendChild(element('span', 'tag', 'Card ' + player.sc + (player.pk > 1 && player.sc2 > 0 ? ' &amp; ' + player.sc2 : '')));
      }
      if (player.pa) row.appendChild(element('span', 'tag', 'Passed'));
      if (game.phase === 3 && player.rd) row.appendChild(element('span', 'tag green', 'Ready'));
      if (game.phase === 2 && game.turn === seat) row.appendChild(element('span', 'tag green', 'Turn'));
      if (game.phase === 2 && seat !== game.turn && !player.pa) {
        var turnButton = element('button', 'small', '&#9654; Turn');
        turnButton.title = 'Make it their turn';
        turnButton.onclick = function () { makeTurn(seat); };
        row.appendChild(turnButton);
      }
      if (game.phase > 0 && seat !== game.speaker) {
        var speakerButton = element('button', 'small', '&#128081;');
        speakerButton.title = 'Make speaker';
        speakerButton.onclick = function () { makeSpeaker(seat); };
        row.appendChild(speakerButton);
      }
      if (player.st || player.rs) {
        var kickButton = element('button', 'small danger', 'Kick');
        kickButton.onclick = function () { kick(seat); };
        row.appendChild(kickButton);
      }
    }
    list.appendChild(row);
  });
}

function renderBattleSelects() {
  ['battle-attacker', 'battle-defender'].forEach(function (id) {
    var select = document.getElementById(id);
    var previous = select.value;
    select.innerHTML = '';
    game.players.forEach(function (player, playerIndex) {
      if (!player.a) return;
      var option = document.createElement('option');
      option.value = playerIndex;
      option.textContent = playerLabel(playerIndex);
      select.appendChild(option);
    });
    if (previous !== '') select.value = previous;
  });
}

function render() {
  if (!game) return;
  document.getElementById('phase-pill').textContent = PHASE_NAMES[game.phase] || '—';
  for (var count = 4; count <= 8; count++) {
    document.getElementById('count-' + count).classList.toggle('on', game.n === count);
  }
  PHASE_NAMES.forEach(function (name, phase) {
    var button = document.getElementById('phase-' + phase);
    if (button) button.classList.toggle('on', game.phase === phase);
  });
  document.getElementById('opt-ag').classList.toggle('on', !!game.agOpt);
  document.getElementById('opt-ag').textContent = game.agOpt ? 'Agenda: After Custodians' : 'Agenda: Every Round';
  document.getElementById('agenda-hint').hidden = !game.agOpt;
  document.getElementById('opt-dbl').classList.toggle('on', !!game.dbl);
  document.getElementById('opt-dbl').textContent = game.dbl ? 'Strategy: 2 Cards Each' : 'Strategy: 1 Card Each';
  document.getElementById('opt-cust').classList.toggle('on', !!game.cust);

  // Only the sections that matter in this phase
  var inSetup = game.phase === 0, inAction = game.phase === 2;
  document.getElementById('section-players').hidden  = !inSetup;
  document.getElementById('section-rules').hidden    = !inSetup;
  document.getElementById('opt-dbl').hidden          = game.n !== 4;
  document.getElementById('section-lighting').hidden = !inSetup;
  document.getElementById('force-start').hidden      = !inSetup;
  document.getElementById('game-controls').hidden    = inSetup;
  document.getElementById('section-battle').hidden   = !inAction;
  document.getElementById('claim-tools').hidden      = !inAction;
  document.getElementById('map-tools').hidden        = !inSetup;
  renderMapTools();

  renderTurnInfo();
  renderSwatches();
  renderSeats();
  renderBattleSelects();
}
</script>
</body>
</html>
)=====";
