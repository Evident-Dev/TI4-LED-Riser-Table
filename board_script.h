#pragma once

// =============================================================================
// TI4 Hex Riser - Shared Board Script (/board.js)
// =============================================================================
// Hex table renderer and WebSocket client shared by the projector and admin
// pages. Flat-top hexes, columns 5-6-7-8-9-8-7-6-5, one SVG line per hex side
// driven by the binary LED frame [0x01][366 x RGB].
//
//   createHexBoard(svgElement, { onHexClick: function (hexIndex) {} })
//     -> { renderLedFrame(bytes), setSelectedHex(hexIndex) }
//   connectTable({ onFrame, onGame, onText, onOnline }) -> { send(message) }
//     also shows a full-screen splash while the table is booting or offline
//   confirmModal(message, action, { confirmLabel, danger })
//     in-page confirmation used instead of the browser's confirm()
//   showRecoveryPrompt(game, { onResume, onNewGame })
//     saved-game prompt after a power loss; buttons only for the handlers given
// =============================================================================

const char BOARD_SCRIPT[] = R"=====(
(function () {
  var SVG_NAMESPACE  = 'http://www.w3.org/2000/svg';
  var HEX_COUNT      = 61;
  var HEX_RADIUS     = 36;
  var HEX_HEIGHT     = HEX_RADIUS * Math.sqrt(3);
  var COLUMN_SPACING = 1.5 * HEX_RADIUS;
  var COLUMNS = [
    { count: 5, start:  0, topStart: true  },
    { count: 6, start:  5, topStart: false },
    { count: 7, start: 11, topStart: true  },
    { count: 8, start: 18, topStart: false },
    { count: 9, start: 26, topStart: true  },
    { count: 8, start: 35, topStart: false },
    { count: 7, start: 43, topStart: true  },
    { count: 6, start: 50, topStart: false },
    { count: 5, start: 56, topStart: true  }
  ];
  var MAX_COLUMN_HEIGHT = 9;

  var boardStyle = document.createElement('style');
  boardStyle.textContent =
    '.hex { fill: #0a1726; stroke: #38d6ff26; stroke-width: 1.5; }' +
    '.hex-board.clickable .hex { cursor: pointer; }' +
    '.hex-board.clickable .hex:hover { stroke: #7fe6ff; stroke-width: 2; }' +
    '.hex.selected { fill: #0e3550; stroke: #7fe6ff; stroke-width: 2; }' +
    '.hex-side { stroke: transparent; stroke-width: 5; stroke-linecap: round; fill: none; pointer-events: none; }' +
    '.hex-board text { font-family: system-ui, sans-serif; font-size: 12px; font-weight: 500;' +
    '  fill: #47556988; text-anchor: middle; dominant-baseline: central; pointer-events: none; }';
  document.head.appendChild(boardStyle);

  var splashStyle = document.createElement('style');
  splashStyle.textContent =
    '#table-splash { position: fixed; inset: 0; z-index: 1000; display: none; flex-direction: column;' +
    '  align-items: center; justify-content: center; gap: 14px; background: #05070dee;' +
    '  font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }' +
    '#table-splash.visible { display: flex; }' +
    '#table-splash .spinner { width: 54px; height: 54px; border-radius: 50%; border: 4px solid #0e2a40;' +
    '  border-top-color: #fbbf24; animation: splash-spin 0.9s linear infinite; margin-bottom: 8px; }' +
    '#table-splash.offline .spinner { border-top-color: #ef4444; animation-duration: 1.6s; }' +
    '#table-splash .title { color: #cbd5e1; font-size: 1.05rem; letter-spacing: 0.3em; text-transform: uppercase; padding-left: 0.3em; }' +
    '#table-splash .subtitle { color: #64748b; font-size: 0.85rem; letter-spacing: 0.1em; }' +
    '@keyframes splash-spin { to { transform: rotate(360deg); } }';
  document.head.appendChild(splashStyle);

  var modalStyle = document.createElement('style');
  modalStyle.textContent =
    '#confirm-modal { position: fixed; inset: 0; z-index: 960; display: none; align-items: center; justify-content: center;' +
    '  padding: 16px; background: #05070dcc; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }' +
    '#confirm-modal.visible { display: flex; }' +
    '#confirm-modal .card { width: 100%; max-width: 380px; padding: 22px; }' +
    '#confirm-modal .message { color: #e2e8f0; font-size: 0.95rem; line-height: 1.45; margin-bottom: 18px; }' +
    '#confirm-modal .actions { display: flex; gap: 10px; justify-content: flex-end; }' +
    '#confirm-modal button { min-height: 40px; padding: 9px 18px; font-size: 0.8rem; }';
  document.head.appendChild(modalStyle);

  window.confirmModal = function (message, action, options) {
    options = options || {};
    var modal = document.getElementById('confirm-modal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'confirm-modal';
      modal.innerHTML = '<div class="card panel"><div class="message"></div><div class="actions">' +
        '<button class="cancel">Cancel</button><button class="confirm"></button></div></div>';
      document.body.appendChild(modal);
    }
    var confirmButton = modal.querySelector('.confirm');
    var cancelButton  = modal.querySelector('.cancel');
    modal.querySelector('.message').textContent = message;
    confirmButton.textContent = options.confirmLabel || 'Confirm';
    confirmButton.classList.toggle('danger', !!options.danger);
    confirmButton.classList.toggle('on', !options.danger);

    function close() {
      modal.classList.remove('visible');
      document.removeEventListener('keydown', onKey);
    }
    function onKey(event) {
      if (event.key === 'Escape') close();
      else if (event.key === 'Enter') { event.preventDefault(); close(); action(); }
    }
    confirmButton.onclick = function () { close(); action(); };
    cancelButton.onclick  = close;
    modal.onclick = function (event) { if (event.target === modal) close(); };
    document.addEventListener('keydown', onKey);
    modal.classList.add('visible');
    confirmButton.focus();
  };

  var recoveryStyle = document.createElement('style');
  recoveryStyle.textContent =
    '#recovery-prompt { position: fixed; inset: 0; z-index: 950; display: none; align-items: center; justify-content: center;' +
    '  padding: 16px; background: #05070de6; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }' +
    '#recovery-prompt.visible { display: flex; }' +
    '#recovery-prompt .card { --edge: #fbbf24; --edge-dim: #fbbf2433; width: 100%; max-width: 420px; padding: 26px; text-align: center; }' +
    '#recovery-prompt .title { color: #fbbf24; font-size: 1.1rem; letter-spacing: 0.2em; text-transform: uppercase; margin-bottom: 10px; }' +
    '#recovery-prompt .detail { color: #cbd5e1; font-size: 0.95rem; margin-bottom: 20px; }' +
    '#recovery-prompt .actions { display: flex; gap: 10px; justify-content: center; }' +
    '#recovery-prompt .actions:empty { display: none; }' +
    '#recovery-prompt button { min-height: 44px; padding: 10px 20px; font-size: 0.85rem; }';
  document.head.appendChild(recoveryStyle);

  var RECOVERY_PHASE_NAMES = ['Setup', 'Strategy', 'Action', 'Status', 'Agenda'];

  window.showRecoveryPrompt = function (game, actions) {
    actions = actions || {};
    var prompt = document.getElementById('recovery-prompt');
    if (!game.rec) {
      if (prompt) prompt.classList.remove('visible');
      return;
    }
    if (!prompt) {
      prompt = document.createElement('div');
      prompt.id = 'recovery-prompt';
      prompt.innerHTML = '<div class="card panel"><div class="title">Saved game found</div>' +
        '<div class="detail"></div><div class="actions"></div></div>';
      document.body.appendChild(prompt);
      var buttons = prompt.querySelector('.actions');
      if (actions.onNewGame) {
        var newGameButton = document.createElement('button');
        newGameButton.className = 'danger';
        newGameButton.textContent = 'Start New Game';
        newGameButton.onclick = actions.onNewGame;
        buttons.appendChild(newGameButton);
      }
      if (actions.onResume) {
        var resumeButton = document.createElement('button');
        resumeButton.className = 'gold';
        resumeButton.textContent = 'Resume';
        resumeButton.onclick = actions.onResume;
        buttons.appendChild(resumeButton);
      }
    }
    prompt.querySelector('.detail').textContent =
      (RECOVERY_PHASE_NAMES[game.recPhase] || '') + ' phase \u00b7 ' + game.recPlayers + ' players';
    prompt.classList.add('visible');
  };

  // Full-screen splash. state: 'booting', 'offline' or null to hide.
  function setTableSplash(state) {
    var splash = document.getElementById('table-splash');
    if (!splash) {
      if (!state) return;
      splash = document.createElement('div');
      splash.id = 'table-splash';
      splash.innerHTML = '<div class="spinner"></div><div class="title"></div><div class="subtitle"></div>';
      document.body.appendChild(splash);
    }
    splash.classList.toggle('visible', !!state);
    splash.classList.toggle('offline', state === 'offline');
    if (!state) return;
    splash.querySelector('.title').textContent    = state === 'offline' ? 'Table offline' : 'Table is booting';
    splash.querySelector('.subtitle').textContent = state === 'offline' ? 'Reconnecting…' : '';
  }

  function getCorners(centerX, centerY, radius) {
    var corners = [];
    for (var corner = 0; corner < 6; corner++) {
      var angle = Math.PI / 180 * (60 * corner + 240);
      corners.push({ x: centerX + radius * Math.cos(angle), y: centerY + radius * Math.sin(angle) });
    }
    return corners;
  }

  window.createHexBoard = function (svg, options) {
    options = options || {};
    var sideElements   = new Array(HEX_COUNT * 6).fill(null);
    var previousColors = new Int32Array(HEX_COUNT * 6).fill(-1);
    var hexPolygons    = new Array(HEX_COUNT).fill(null);
    var selectedHex    = -1;
    var lastFrame      = null;

    var viewWidth   = (COLUMNS.length - 1) * COLUMN_SPACING + 2 * HEX_RADIUS + 20;
    var viewHeight  = MAX_COLUMN_HEIGHT * HEX_HEIGHT + 30;
    var firstColumnX = HEX_RADIUS + 10;
    var gridCenterY  = 10 + (MAX_COLUMN_HEIGHT * HEX_HEIGHT) / 2;
    svg.setAttribute('viewBox', '0 0 ' + viewWidth + ' ' + viewHeight);
    svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');
    svg.classList.add('hex-board');
    if (options.onHexClick) svg.classList.add('clickable');

    function build(sideGap) {
      while (svg.lastChild) svg.removeChild(svg.lastChild);
      sideElements.fill(null);
      previousColors.fill(-1);

      COLUMNS.forEach(function (column, columnIndex) {
        var centerX   = firstColumnX + columnIndex * COLUMN_SPACING;
        var columnTop = gridCenterY - (column.count * HEX_HEIGHT) / 2 + HEX_HEIGHT / 2;
        for (var row = 0; row < column.count; row++) {
          var hexIndex = column.topStart ? column.start + row : column.start + (column.count - 1 - row);
          var centerY  = columnTop + row * HEX_HEIGHT;
          var group    = document.createElementNS(SVG_NAMESPACE, 'g');

          var polygon = document.createElementNS(SVG_NAMESPACE, 'polygon');
          polygon.setAttribute('points', getCorners(centerX, centerY, HEX_RADIUS - 1)
            .map(function (point) { return point.x.toFixed(2) + ',' + point.y.toFixed(2); }).join(' '));
          polygon.setAttribute('class', 'hex' + (hexIndex === selectedHex ? ' selected' : ''));
          hexPolygons[hexIndex] = polygon;
          group.appendChild(polygon);
          if (options.onHexClick) {
            group.addEventListener('click', (function (index) {
              return function () { options.onHexClick(index); };
            })(hexIndex));
          }

          var sideCorners = getCorners(centerX, centerY, HEX_RADIUS - sideGap);
          for (var side = 0; side < 6; side++) {
            var start = sideCorners[side], end = sideCorners[(side + 1) % 6];
            var line = document.createElementNS(SVG_NAMESPACE, 'line');
            line.setAttribute('class', 'hex-side');
            line.setAttribute('x1', start.x.toFixed(2)); line.setAttribute('y1', start.y.toFixed(2));
            line.setAttribute('x2', end.x.toFixed(2));   line.setAttribute('y2', end.y.toFixed(2));
            sideElements[hexIndex * 6 + side] = line;
            group.appendChild(line);
          }

          var label = document.createElementNS(SVG_NAMESPACE, 'text');
          label.setAttribute('x', centerX.toFixed(2));
          label.setAttribute('y', centerY.toFixed(2));
          label.textContent = hexIndex;
          group.appendChild(label);
          svg.appendChild(group);
        }
      });
      if (lastFrame) renderLedFrame(lastFrame);
    }

    function renderLedFrame(bytes) {
      lastFrame = bytes;
      var position = 1;
      for (var sideIndex = 0; sideIndex < HEX_COUNT * 6 && position + 2 < bytes.length; sideIndex++) {
        var red = bytes[position], green = bytes[position + 1], blue = bytes[position + 2];
        position += 3;
        var element = sideElements[sideIndex];
        if (!element) continue;
        var packed = (red << 16) | (green << 8) | blue;
        if (previousColors[sideIndex] === packed) continue;
        previousColors[sideIndex] = packed;
        element.style.stroke = (packed === 0) ? 'transparent' : 'rgb(' + red + ',' + green + ',' + blue + ')';
      }
    }

    function setSelectedHex(hexIndex) {
      if (selectedHex >= 0 && hexPolygons[selectedHex]) hexPolygons[selectedHex].classList.remove('selected');
      selectedHex = hexIndex;
      if (hexIndex >= 0 && hexPolygons[hexIndex]) hexPolygons[hexIndex].classList.add('selected');
    }

    fetch('/getsettings', { cache: 'no-store' })
      .then(function (response) { return response.json(); })
      .then(function (settings) { build(settings.sideGap != null ? +settings.sideGap : 4); })
      .catch(function () { build(4); });

    return { renderLedFrame: renderLedFrame, setSelectedHex: setSelectedHex };
  };

  // Live connection to the table with auto-reconnect.
  // A restart drops the connection without telling the browser, so silence
  // longer than STALE_AFTER_MS (game state arrives every 300 ms) means offline.
  // While offline the page only retries; frames and game updates resume once
  // the first game state arrives on the new connection.
  var STALE_AFTER_MS = 1000;
  var RETRY_AFTER_MS = 500;

  window.connectTable = function (handlers) {
    var socket = null;
    var offline = false;
    var retryPending = false;
    var lastGameText = '';
    var lastMessageTime = 0;
    var firstBootIdentifier = null;
    var pendingFrame = null;  // newest frame received while confirming the reconnect

    function goOffline() {
      if (socket) {
        socket.onclose = null;
        socket.onerror = null;
        socket.onmessage = null;
        socket.close();
        socket = null;
      }
      if (!offline) {
        offline = true;
        lastGameText = '';
        setTableSplash('offline');
        if (handlers.onOnline) handlers.onOnline(false);
      }
      if (!retryPending) {
        retryPending = true;
        setTimeout(function () { retryPending = false; open(); }, RETRY_AFTER_MS);
      }
    }

    function handleGame(game, text) {
      // Controller restarted: reload so the page matches its firmware
      if (firstBootIdentifier === null) firstBootIdentifier = game.bid;
      else if (game.bid !== firstBootIdentifier) { location.reload(); return; }

      if (offline) {
        offline = false;
        if (handlers.onOnline) handlers.onOnline(true);
        if (pendingFrame && handlers.onFrame) handlers.onFrame(pendingFrame);
        pendingFrame = null;
      }
      setTableSplash(game.boot ? 'booting' : null);
      if (text === lastGameText) return;  // unchanged, skip the render
      lastGameText = text;
      if (handlers.onGame) handlers.onGame(game);
    }

    function open() {
      socket = new WebSocket('ws://' + location.host + '/ws');
      socket.binaryType = 'arraybuffer';
      lastMessageTime = Date.now();
      socket.onopen = function () { if (!offline && handlers.onOnline) handlers.onOnline(true); };
      socket.onclose = goOffline;
      socket.onerror = goOffline;
      socket.onmessage = function (event) {
        lastMessageTime = Date.now();
        if (event.data instanceof ArrayBuffer) {
          var bytes = new Uint8Array(event.data);
          if (bytes.length < 2 || bytes[0] !== 1) return;
          if (offline) pendingFrame = bytes;
          else if (handlers.onFrame) handlers.onFrame(bytes);
        } else if (typeof event.data === 'string' && event.data.charAt(0) === '{') {
          try {
            var game = JSON.parse(event.data);
            if (game.t === 'game') handleGame(game, event.data);
          } catch (error) {}
        } else if (!offline && handlers.onText) {
          handlers.onText(event.data);
        }
      };
    }
    open();

    setInterval(function () {
      if (socket && Date.now() - lastMessageTime >= STALE_AFTER_MS) goOffline();
    }, 250);

    return {
      send: function (message) {
        if (!offline && socket && socket.readyState === WebSocket.OPEN) socket.send(message);
      }
    };
  };
})();
)=====";
