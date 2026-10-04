#pragma once

// =============================================================================
// TI4 Hex Riser - Player Page (/play)
// =============================================================================
// Phone keypad for players 1-8. Claim a seat, then get a phase-aware pad:
//   Setup    — color swatches (keys 1-8), Lock In (15), Start Game (0)
//   Strategy — strategy cards (keys 1-8), Lock In (15)
//   Action   — End Turn (15), Pass (14), Battle (13 then opponent 1-8)
//   Status   — Ready (15)
//   Agenda   — speaker only: End Agenda (15)
//
// Seat claims persist across reconnects via a token and game ID in
// localStorage, so a locked phone screen or a resumed game keeps the seat.
// All key presses route through the same handleGameKey() path the physical
// keyboards will use.
// =============================================================================

const char PLAY_PAGE[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>TI4 Player Pad</title>
<link rel="stylesheet" href="/theme.css">
<style>
* { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
:root {
  --border: #38d6ff1f;
  --text: #cbd5e1; --muted: #6b8aa6; --accent: #38d6ff; --gold: #fbbf24;
}
html, body { height: 100%; }
body {
  color: var(--text);
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  display: flex; flex-direction: column; overflow: hidden;
  touch-action: manipulation;
}
#topbar {
  display: flex; align-items: center; gap: 10px;
  padding: 10px 14px; flex-shrink: 0;
}
.dot { width: 9px; height: 9px; border-radius: 50%; background: #7f1d1d; transition: background 0.3s; flex-shrink: 0; }
.dot.on { background: #22c55e; }
#topbar h1 { font-size: 0.85rem; letter-spacing: 0.2em; text-transform: uppercase; color: var(--accent); flex: 1; text-shadow: 0 0 12px #38d6ff55; }
#my-chip { display: none; align-items: center; gap: 7px; font-size: 0.8rem; font-weight: 600; }
#my-dot { width: 14px; height: 14px; border-radius: 50%; border: 2px solid #ffffff33; }
#btn-leave { --cut: 6px; font-size: 0.68rem; padding: 6px 12px; }
#phase-strip {
  display: flex; justify-content: center; gap: 5px; padding: 8px 10px;
  background: #060e19cc; border-bottom: 1px solid var(--border); flex-shrink: 0;
}
.phase-step { --edge: transparent; --edge-dim: transparent; --fill: transparent; color: #4a6680; }
.phase-step.now { --edge: #fbbf24; --edge-dim: #fbbf2444; --fill: #241604; color: var(--gold); }
#content { flex: 1; overflow-y: auto; padding: 16px 14px 26px; display: flex; flex-direction: column; gap: 14px; }
h2 { font-size: 0.95rem; color: #e2e8f0; }
.hint { font-size: 0.8rem; color: var(--muted); line-height: 1.45; }
input[type=text] { width: 100%; padding: 12px; font-size: 1rem; }
.seat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.seat-card {
  padding: 16px 10px; display: flex; flex-direction: column; align-items: center; gap: 6px;
  font-size: 0.9rem; font-weight: 600;
}
.seat-card .who { font-size: 0.72rem; font-weight: 400; color: var(--muted); text-transform: none; letter-spacing: 0; }
.seat-card.mine { --edge: #7fe6ff; --edge-dim: #38d6ff99; --fill: linear-gradient(180deg, #0e3550, #0a2236); }
.seat-card.taken { --edge: #f8717177; --edge-dim: #7f1d1d55; }
.swatch-grid { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 10px; }
.swatch {
  --edge: #ffffff55; --edge-dim: #ffffff22; --edge-width: 2px;
  aspect-ratio: 1;
  display: flex; align-items: flex-end; justify-content: center;
  font-size: 0.62rem; font-weight: 700; color: #000000aa; padding-bottom: 5px;
}
.swatch.sel { --edge: #ffffff; --edge-dim: #ffffffcc; --edge-width: 4px; }
.swatch.gone { opacity: 0.22; }
.card-list { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.strat-card {
  --edge: var(--card); --edge-dim: var(--card); --edge-width: 2px;
  padding: 14px 8px; display: flex; flex-direction: column;
  align-items: center; gap: 3px;
}
.strat-card .num { font-size: 1.25rem; font-weight: 800; }
.strat-card .nm  { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.06em; }
.strat-card .who { font-size: 0.62rem; color: var(--muted); text-transform: none; letter-spacing: 0; }
.strat-card.sel { --edge: #ffffff; --edge-dim: #ffffffcc; --edge-width: 3px; }
.strat-card.gone { opacity: 0.3; }
.big-btn {
  --cut: 14px; padding: 20px; font-size: 1rem; font-weight: 700; letter-spacing: 0.12em; width: 100%;
}
.turn-banner { --cut: 10px; text-align: center; padding: 14px; font-size: 1rem; font-weight: 700; }
.turn-banner.you { --edge: #4ade80; --edge-dim: #4ade8044; --fill: linear-gradient(180deg, #06331a, #04220f); color: #86efac; }
.toast {
  --cut: 8px; --edge: #f87171; --edge-dim: #ef444455; --fill: #2a0707;
  position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
  color: #fecaca; padding: 10px 18px; font-size: 0.85rem; display: none; z-index: 10;
}
#table-splash {
  position: fixed; inset: 0; z-index: 1000; display: none; flex-direction: column;
  align-items: center; justify-content: center; gap: 14px; background: #05070dee;
}
#table-splash.visible { display: flex; }
#table-splash .spinner {
  width: 54px; height: 54px; border-radius: 50%; border: 4px solid #0e2a40; margin-bottom: 8px;
  border-top-color: #fbbf24; animation: splash-spin 0.9s linear infinite;
}
#table-splash.offline .spinner { border-top-color: #ef4444; animation-duration: 1.6s; }
#table-splash .title { color: #cbd5e1; font-size: 1.05rem; letter-spacing: 0.3em; text-transform: uppercase; padding-left: 0.3em; }
#table-splash .subtitle { color: #64748b; font-size: 0.85rem; letter-spacing: 0.1em; }
@keyframes splash-spin { to { transform: rotate(360deg); } }
#recovery-prompt {
  position: fixed; inset: 0; z-index: 950; display: none; align-items: center; justify-content: center;
  padding: 16px; background: #05070de6;
}
#recovery-prompt.visible { display: flex; }
#recovery-prompt .card { --edge: #fbbf24; --edge-dim: #fbbf2433; width: 100%; max-width: 420px; padding: 26px; text-align: center; }
#recovery-prompt .title { color: #fbbf24; font-size: 1.1rem; letter-spacing: 0.2em; text-transform: uppercase; margin-bottom: 10px; }
#recovery-prompt .detail { color: #cbd5e1; font-size: 0.95rem; margin-bottom: 20px; }
#recovery-prompt button { min-height: 48px; padding: 10px 28px; font-size: 0.95rem; }
</style>
</head>
<body>

<div id="table-splash"><div class="spinner"></div><div class="title"></div><div class="subtitle"></div></div>
<div id="recovery-prompt"><div class="card panel">
  <div class="title">Saved game found</div>
  <div class="detail" id="recovery-detail"></div>
  <button class="gold" onclick="send('RESUME')">Resume</button>
</div></div>

<div id="topbar" class="holo-bar">
  <div class="dot" id="ws-dot"></div>
  <h1>Player Pad</h1>
  <div id="my-chip"><div id="my-dot"></div><span id="my-name"></span></div>
  <button id="btn-leave" onclick="leaveSeat()" style="display:none">Leave</button>
</div>

<div id="phase-strip">
  <span class="tag phase-step" data-ph="0">Setup</span>
  <span class="tag phase-step" data-ph="1">Strategy</span>
  <span class="tag phase-step" data-ph="2">Action</span>
  <span class="tag phase-step" data-ph="3">Status</span>
  <span class="tag phase-step" data-ph="4">Agenda</span>
</div>

<div id="content"></div>
<div class="panel toast" id="toast"></div>

<script>
// Mirrors COLOR_PALETTE / STRATEGY_COLORS in config.h
var COLORS = [
  {hex:'#FF0000', nm:'Red'},    {hex:'#0000FF', nm:'Blue'},
  {hex:'#00FF00', nm:'Green'},  {hex:'#FFFF00', nm:'Yellow'},
  {hex:'#FF00FF', nm:'Purple'}, {hex:'#FF8000', nm:'Orange'},
  {hex:'#00FFFF', nm:'Cyan'},   {hex:'#FFFFFF', nm:'White'}
];
var CARDS = [
  {hex:'#8B4513', nm:'Leadership'},   {hex:'#2E8B57', nm:'Diplomacy'},
  {hex:'#4169E1', nm:'Politics'},     {hex:'#8B0000', nm:'Construction'},
  {hex:'#FF8C00', nm:'Trade'},        {hex:'#9370DB', nm:'Warfare'},
  {hex:'#20B2AA', nm:'Technology'},   {hex:'#FFD700', nm:'Imperial'}
];
var PHASE_NAMES = ['Setup','Strategy','Action','Status','Agenda'];

var mySeat  = parseInt(localStorage.getItem('ti4Seat'));
var myToken = localStorage.getItem('ti4Token') || '0';
var myName  = localStorage.getItem('ti4Name') || '';
var myGame  = localStorage.getItem('ti4Game') || '';  // game ID the seat token belongs to
if (isNaN(mySeat)) mySeat = -1;
var rejoinSent = false;       // rejoin claim already sent on this connection
var seated  = false;          // server confirmed our claim
var game    = null;           // last game JSON
var battleArming = false;     // local: tapped Battle, choosing opponent
var _lastGameStr = '';        // raw JSON of the last render, to skip no-change pushes
var _renderPending = false;   // a render was skipped while the name field had focus

// ============================================================
// WebSocket
// ============================================================
var _ws = null;
// A restart drops the connection without telling the browser, so silence
// longer than 1 s (game state arrives every 300 ms) means offline. While
// offline the page only retries; it resumes on the first game state.
var STALE_AFTER_MS = 1000;
var RETRY_AFTER_MS = 500;
var offline = false;
var retryPending = false;
var lastMessageTime = 0;
var firstBootIdentifier = null;

// Full-screen splash. state: 'booting', 'offline' or null to hide.
function setTableSplash(state) {
  var splash = document.getElementById('table-splash');
  splash.classList.toggle('visible', !!state);
  splash.classList.toggle('offline', state === 'offline');
  if (!state) return;
  splash.querySelector('.title').textContent    = state === 'offline' ? 'Table offline' : 'Table is booting';
  splash.querySelector('.subtitle').textContent = state === 'offline' ? 'Reconnecting…' : '';
}

function goOffline() {
  if (_ws) {
    _ws.onclose = null;
    _ws.onerror = null;
    _ws.onmessage = null;
    _ws.close();
    _ws = null;
  }
  if (!offline) {
    offline = true;
    seated = false;
    _lastGameStr = '';
    setTableSplash('offline');
    document.getElementById('ws-dot').classList.remove('on');
  }
  if (!retryPending) {
    retryPending = true;
    setTimeout(function () { retryPending = false; connect(); }, RETRY_AFTER_MS);
  }
}

setInterval(function () {
  if (_ws && Date.now() - lastMessageTime >= STALE_AFTER_MS) goOffline();
}, 250);

var RECOVERY_PHASE_NAMES = ['Setup', 'Strategy', 'Action', 'Status', 'Agenda'];
function showRecoveryPrompt(currentGame) {
  document.getElementById('recovery-prompt').classList.toggle('visible', !!currentGame.rec);
  if (!currentGame.rec) return;
  document.getElementById('recovery-detail').textContent =
    (RECOVERY_PHASE_NAMES[currentGame.recPhase] || '') + ' phase \u00b7 ' + currentGame.recPlayers + ' players';
}

// Rejoins our seat once per connection, but only for the game the token came
// from. A token from another game is forgotten.
function reconcileSeat(currentGame) {
  if (currentGame.rec || currentGame.boot) return;
  if (mySeat < 0 || myToken === '0') return;
  if (myGame !== String(currentGame.gid)) { forgetSeat(); return; }
  if (seated || rejoinSent) return;
  rejoinSent = true;
  _ws.send('CLAIM:' + mySeat + ':' + myToken + ':' + myName);
}

function connect() {
  _ws = new WebSocket('ws://' + location.host + '/ws');
  lastMessageTime = Date.now();
  rejoinSent = false;
  _ws.onopen = function () {
    document.getElementById('ws-dot').classList.add('on');
  };
  _ws.onclose = goOffline;
  _ws.onerror = goOffline;
  _ws.onmessage = function (ev) {
    lastMessageTime = Date.now();
    if (typeof ev.data !== 'string') return;
    if (ev.data.charAt(0) === '{') {
      try {
        var g = JSON.parse(ev.data);
        if (g.t !== 'game') return;
        // Controller restarted: reload so the page matches its firmware
        if (firstBootIdentifier === null) firstBootIdentifier = g.bid;
        else if (g.bid !== firstBootIdentifier) { location.reload(); return; }
        offline = false;
        setTableSplash(g.boot ? 'booting' : null);
        showRecoveryPrompt(g);
        reconcileSeat(g);
        if (ev.data === _lastGameStr) return;  // nothing changed, keep the DOM alone
        _lastGameStr = ev.data; game = g;
        render();
      } catch (e) {}
      return;
    }
    if (ev.data.indexOf('CLAIMED:') === 0) {
      var parts = ev.data.split(':');
      mySeat  = parseInt(parts[1]);
      myToken = parts[2];
      seated  = true;
      myGame  = game ? String(game.gid) : '';
      localStorage.setItem('ti4Seat', mySeat);
      localStorage.setItem('ti4Token', myToken);
      localStorage.setItem('ti4Game', myGame);
      render();
    } else if (ev.data.indexOf('DENIED:') === 0) {
      seated = false;
      myToken = '0';
      localStorage.removeItem('ti4Token');
      toast('Seat already taken');
      render();
    }
  };
}
function send(msg) {
  if (!offline && _ws && _ws.readyState === WebSocket.OPEN) _ws.send(msg);
}
function key(k) {
  if (!seated) return;
  send('KEY:' + mySeat + ':' + k + ':' + myToken);
  if (navigator.vibrate) navigator.vibrate(12);
}
function claimSeat(idx) {
  var nameEl = document.getElementById('name-input');
  if (nameEl) {
    myName = nameEl.value.trim().replace(/[:"\\]/g, '').slice(0, 16);
    localStorage.setItem('ti4Name', myName);
  }
  send('CLAIM:' + idx + ':0:' + myName);
}
function leaveSeat() {
  if (seated) send('RELEASE:' + mySeat + ':' + myToken);
  forgetSeat();
}
function forgetSeat() {
  seated  = false;
  mySeat  = -1;
  myToken = '0';
  myGame  = '';
  localStorage.removeItem('ti4Seat');
  localStorage.removeItem('ti4Token');
  localStorage.removeItem('ti4Game');
  if (game) render();
}

var _toastTimer = null;
function toast(msg) {
  var el = document.getElementById('toast');
  el.textContent = msg;
  el.style.display = 'block';
  if (_toastTimer) clearTimeout(_toastTimer);
  _toastTimer = setTimeout(function () { el.style.display = 'none'; }, 2500);
}

// ============================================================
// Rendering
// ============================================================
function el(tag, cls, html) {
  var e = document.createElement(tag);
  if (cls) e.className = cls;
  if (html != null) e.innerHTML = html;
  return e;
}
function playerLabel(i) {
  var p = game.players[i];
  return (p && (p.st || p.rs) && p.nm) ? p.nm : 'P' + (i + 1);
}

function render() {
  if (!game) return;
  // Rebuilding the page while typing would tear down the name field and close
  // the phone keyboard — hold the render until the field loses focus.
  var focused = document.activeElement;
  if (focused && focused.id === 'name-input') {
    _renderPending = true;
    return;
  }
  _renderPending = false;
  var c = document.getElementById('content');
  c.innerHTML = '';

  // Phase strip
  document.querySelectorAll('.phase-step').forEach(function (s) {
    s.classList.toggle('now', parseInt(s.getAttribute('data-ph')) === game.phase);
  });

  var me = (seated && mySeat >= 0) ? game.players[mySeat] : null;

  // Header chip
  var chip = document.getElementById('my-chip');
  var leaveBtn = document.getElementById('btn-leave');
  if (me) {
    chip.style.display = 'flex';
    leaveBtn.style.display = '';
    document.getElementById('my-dot').style.background = '#' + me.col;
    document.getElementById('my-name').textContent = playerLabel(mySeat);
  } else {
    chip.style.display = 'none';
    leaveBtn.style.display = 'none';
  }

  if (!me) { renderSeatSelect(c); return; }
  if (!me.a) {
    c.appendChild(el('div', 'panel turn-banner', 'Your seat is not in this game.<br>' +
      '<span class="hint">The game master set the player count to ' + game.n + '.</span>'));
    return;
  }

  switch (game.phase) {
    case 0: renderSetup(c, me);    break;
    case 1: renderStrategy(c, me); break;
    case 2: renderAction(c, me);   break;
    case 3: renderStatus(c, me);   break;
    case 4: renderAgenda(c, me);   break;
  }
}

// ------------------------------------------------------------
function renderSeatSelect(c) {
  c.appendChild(el('h2', null, 'Pick your seat'));

  var input = el('input');
  input.type = 'text';
  input.id = 'name-input';
  input.placeholder = 'Your name (optional)';
  input.maxLength = 16;
  input.value = myName;
  input.autocomplete = 'off';
  // Keep the typed value across rebuilds, and apply any render held while typing
  input.oninput = function () { myName = input.value; };
  input.onblur  = function () { if (_renderPending) render(); };
  c.appendChild(input);

  var grid = el('div', 'seat-grid');
  for (var i = 0; i < 8; i++) {
    var p = game.players[i];
    if (!p || !p.a) continue;  // seats not in this game
    var card = el('button', 'seat-card');
    var claimed = p.st;
    var reserved = p.rs && !p.st;
    var dot = el('div');
    dot.style.cssText = 'width:18px;height:18px;border-radius:50%;background:#' + p.col + ';border:2px solid #ffffff22';
    card.appendChild(dot);
    card.appendChild(el('div', null, 'Seat ' + (i + 1)));
    var who = el('div', 'who');
    if (claimed)       who.textContent = '\ud83d\udd12 ' + (p.nm || 'Taken');
    else if (reserved) who.textContent = 'Reserved \u00b7 ' + (p.nm || 'Player');
    else               who.textContent = 'Open';
    card.appendChild(who);
    if (claimed) card.classList.add('taken');
    card.disabled = claimed;
    card.onclick = (function (idx) { return function () { claimSeat(idx); }; })(i);
    grid.appendChild(card);
  }
  c.appendChild(grid);
}

// ------------------------------------------------------------
function renderSetup(c, me) {
  c.appendChild(el('h2', null, me.cl ? 'Color locked in!' : 'Choose your color'));

  var grid = el('div', 'swatch-grid');
  for (var i = 0; i < 8; i++) {
    var sw = el('button', 'swatch', COLORS[i].nm);
    sw.style.setProperty('--fill', COLORS[i].hex);
    var takenByOther = (game.cTaken & (1 << i)) && !(me.cl && me.ci === i);
    if (takenByOther) sw.classList.add('gone');
    if (me.ci === i)  sw.classList.add('sel');
    sw.disabled = me.cl || takenByOther;
    sw.onclick = (function (k) { return function () { key(k); }; })(i + 1);
    grid.appendChild(sw);
  }
  c.appendChild(grid);

  var lock = el('button', 'big-btn primary', me.cl ? '&#10003; Locked In' : 'Lock In Color');
  lock.disabled = me.cl;
  lock.onclick = function () { key(15); };
  c.appendChild(lock);

  var allLocked = game.players.every(function (p) { return !p.a || p.cl; });
  if (allLocked) {
    var start = el('button', 'big-btn gold', '&#9733; Start Game');
    start.onclick = function () { key(0); };
    c.appendChild(start);
    c.appendChild(el('div', 'hint', 'Everyone is locked in. Any player can start the game — the speaker roulette will run on the table.'));
  } else {
    var waiting = game.players.filter(function (p) { return p.a && !p.cl; }).length;
    c.appendChild(el('div', 'hint', 'Waiting on ' + waiting + ' player' + (waiting === 1 ? '' : 's') + ' to lock in&hellip;'));
  }
}

// ------------------------------------------------------------
function renderStrategy(c, me) {
  var myTurn = (game.picker === mySeat);
  // In 4-player games each player locks two cards; pk = picks locked so far
  var pendingCard = (me.pk === 0) ? me.sc : me.sc2;

  if (me.sl) {
    var picked = CARDS[me.sc - 1].nm + (me.pk > 1 && me.sc2 > 0 ? '</b> &amp; <b>' + CARDS[me.sc2 - 1].nm : '');
    c.appendChild(el('div', 'panel turn-banner', 'You picked <b>' + picked + '</b>'));
  } else if (myTurn) {
    c.appendChild(el('div', 'panel turn-banner you', me.pk === 1 ? 'Pick your second card!' : 'Your pick!'));
  } else if (game.picker >= 0) {
    c.appendChild(el('div', 'panel turn-banner', playerLabel(game.picker) + ' is picking&hellip;'));
  }

  c.appendChild(el('h2', null, 'Strategy cards'));
  var grid = el('div', 'card-list');
  for (var i = 0; i < 8; i++) {
    var taken = (game.kTaken & (1 << i)) !== 0;
    var card = el('button', 'strat-card');
    card.style.setProperty('--card', CARDS[i].hex);
    card.appendChild(el('div', 'num', String(i + 1)));
    card.appendChild(el('div', 'nm', CARDS[i].nm));
    if (taken) {
      var owner = -1;
      game.players.forEach(function (p, pi) {
        if (!p.a) return;
        if ((p.pk >= 1 && p.sc === i + 1) || (p.pk >= 2 && p.sc2 === i + 1)) owner = pi;
      });
      card.appendChild(el('div', 'who', owner >= 0 ? playerLabel(owner) : 'Taken'));
      card.classList.add('gone');
    }
    if (!me.sl && !taken && pendingCard === i + 1) card.classList.add('sel');
    card.disabled = !myTurn || me.sl || taken;
    card.onclick = (function (k) { return function () { key(k); }; })(i + 1);
    grid.appendChild(card);
  }
  c.appendChild(grid);

  if (myTurn && !me.sl) {
    var lock = el('button', 'big-btn primary', 'Lock In Card');
    lock.disabled = (pendingCard === 0);
    lock.onclick = function () { key(15); };
    c.appendChild(lock);
  }
}

// ------------------------------------------------------------
function renderAction(c, me) {
  // Battle states take over the pad
  if (game.atk >= 0) {
    c.appendChild(el('div', 'panel turn-banner', '&#9876; <b>' + playerLabel(game.atk) + '</b> vs <b>' + playerLabel(game.def) + '</b>'));
    var end = el('button', 'big-btn danger', 'End Battle');
    end.onclick = function () { key(13); };
    c.appendChild(end);
    return;
  }
  if (game.pend === mySeat || (battleArming && game.pend < 0)) {
    c.appendChild(el('h2', null, 'Choose your opponent'));
    var grid = el('div', 'seat-grid');
    game.players.forEach(function (p, i) {
      if (!p.a || i === mySeat) return;
      var b = el('button', 'seat-card');
      var dot = el('div');
      dot.style.cssText = 'width:18px;height:18px;border-radius:50%;background:#' + p.col;
      b.appendChild(dot);
      b.appendChild(el('div', null, playerLabel(i)));
      b.onclick = (function (k) { return function () { battleArming = false; key(k); }; })(i + 1);
      grid.appendChild(b);
    });
    c.appendChild(grid);
    var cancel = el('button', 'big-btn', 'Cancel');
    cancel.onclick = function () { battleArming = false; key(13); };
    c.appendChild(cancel);
    return;
  }
  battleArming = false;

  var myTurn = (game.turn === mySeat);
  if (me.pa) {
    c.appendChild(el('div', 'panel turn-banner', 'You passed this round.'));
  } else if (myTurn) {
    c.appendChild(el('div', 'panel turn-banner you', 'Your turn!'));
  } else if (game.turn >= 0) {
    c.appendChild(el('div', 'panel turn-banner', playerLabel(game.turn) + '’s turn'));
  }

  if (!me.pa) {
    var endTurn = el('button', 'big-btn primary', 'End Turn');
    endTurn.disabled = !myTurn;
    endTurn.onclick = function () { key(15); };
    c.appendChild(endTurn);

    var pass = el('button', 'big-btn gold', 'Pass for the Round');
    pass.disabled = !myTurn;
    pass.onclick = function () { key(14); };
    c.appendChild(pass);
  }

  var battle = el('button', 'big-btn danger', '&#9876; Battle');
  battle.onclick = function () { battleArming = true; key(13); render(); };
  c.appendChild(battle);
}

// ------------------------------------------------------------
function renderStatus(c, me) {
  c.appendChild(el('h2', null, 'Status phase'));
  c.appendChild(el('div', 'hint', 'Score objectives, refresh cards, repair units. Tap Ready when your cleanup is done.'));
  var ready = el('button', 'big-btn primary', me.rd ? '&#10003; Ready' : 'Ready');
  ready.disabled = me.rd;
  ready.onclick = function () { key(15); };
  c.appendChild(ready);
  var waiting = game.players.filter(function (p) { return p.a && !p.rd; }).length;
  if (waiting > 0) c.appendChild(el('div', 'hint', 'Waiting on ' + waiting + ' player' + (waiting === 1 ? '' : 's') + '&hellip;'));
}

// ------------------------------------------------------------
function renderAgenda(c, me) {
  c.appendChild(el('h2', null, 'Agenda phase'));
  if (game.speaker === mySeat) {
    c.appendChild(el('div', 'panel turn-banner you', 'You are the Speaker'));
    c.appendChild(el('div', 'hint', 'Resolve both agendas, then start the next round.'));
    var end = el('button', 'big-btn gold', 'End Agenda / Next Round');
    end.onclick = function () { key(15); };
    c.appendChild(end);
  } else {
    c.appendChild(el('div', 'panel turn-banner', playerLabel(game.speaker) + ' is the Speaker'));
    c.appendChild(el('div', 'hint', 'Vote on agendas at the table. The speaker ends the phase.'));
  }
}

connect();
</script>
</body>
</html>
)=====";
