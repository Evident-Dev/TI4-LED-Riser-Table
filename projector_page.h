#pragma once

// =============================================================================
// TI4 Hex Riser - Projector Page (/projector)
// =============================================================================
// Full-screen live mirror of the table for a projector or TV. Display only.
// Shows the map tiles, a player list, and faction icons on claimed hexes,
// over the same twinkling starfield as the home page.
// =============================================================================

const char PROJECTOR_PAGE[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TI4 Projector</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html { background: #03050c; }
html, body { height: 100%; overflow: hidden; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }
body { display: flex; }
#starfield { position: fixed; inset: 0; width: 100%; height: 100%; z-index: -1; }
#roster {
  flex-shrink: 0; display: flex; flex-direction: column; gap: 1vmin; padding: 2.5vmin 0 2.5vmin 2.5vmin;
  font-size: 2vmin; color: #cbd5e1;
}
#roster:empty { display: none; }
#board { display: block; flex: 1; min-width: 0; height: 100vh; padding: 2vmin; }
.roster-row {
  display: flex; align-items: center; gap: 1.2vmin; padding: 0.8vmin 1.4vmin 0.8vmin 1.2vmin;
  border-left: 0.6vmin solid var(--player-color); background: #0a1726cc;
}
.roster-row.active { background: #0e3550e6; color: #ffffff; }
.roster-row.passed { opacity: 0.45; }
.roster-row img { width: 3.2vmin; height: 3.2vmin; object-fit: contain; }
.roster-row .speaker { color: #fbbf24; font-size: 1.5vmin; letter-spacing: 0.15em; text-transform: uppercase; }
</style>
<script src="/tiles.js"></script>
<script src="/board.js"></script>
</head>
<body>
<canvas id="starfield"></canvas>
<div id="roster"></div>
<svg id="board"></svg>
<script>
var board = createHexBoard(document.getElementById('board'), { claimIcons: true });
var tableData = null;
var game = null;
loadTableData().then(function (data) { tableData = data; renderRoster(); });

connectTable({
  onFrame: board.renderLedFrame,
  onMap: board.setMap,
  onGame: function (newGame) {
    game = newGame;
    showRecoveryPrompt(game);
    board.setGame(game);
    renderRoster();
  }
});

// Twinkling starfield, matching the home page background
(function () {
  var canvas  = document.getElementById('starfield');
  var context = canvas.getContext('2d');
  var reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var STAR_COUNT = 300;
  var width = 0, height = 0;
  var stars = [];

  function random(minimum, maximum) { return minimum + Math.random() * (maximum - minimum); }

  for (var star = 0; star < STAR_COUNT; star++) {
    stars.push({ x: Math.random(), y: Math.random(), size: random(0.5, 1.6),
                 twinkleSpeed: random(0.0006, 0.002), phase: random(0, Math.PI * 2) });
  }

  function resize() {
    var scale = Math.min(window.devicePixelRatio || 1, 1.5);
    width  = window.innerWidth;
    height = window.innerHeight;
    canvas.width  = width * scale;
    canvas.height = height * scale;
    context.setTransform(scale, 0, 0, scale, 0, 0);
  }

  function drawFrame(time) {
    context.fillStyle = '#03050c';
    context.fillRect(0, 0, width, height);
    stars.forEach(function (star) {
      var twinkle = reducedMotion ? 0.7 : 0.4 + 0.6 * Math.abs(Math.sin(time * star.twinkleSpeed + star.phase));
      context.fillStyle = 'rgba(220,235,255,' + (twinkle * 0.85) + ')';
      context.fillRect(star.x * width, star.y * height, star.size, star.size);
    });
    if (!reducedMotion) requestAnimationFrame(drawFrame);
  }

  resize();
  window.addEventListener('resize', function () { resize(); if (reducedMotion) drawFrame(0); });
  requestAnimationFrame(drawFrame);
})();

function renderRoster() {
  var roster = document.getElementById('roster');
  roster.innerHTML = '';
  if (!game) return;
  var activePlayer = game.phase === 1 ? game.picker : game.phase === 2 ? game.turn : -1;
  game.players.forEach(function (player, playerIndex) {
    if (!player.a) return;
    var row = document.createElement('div');
    row.className = 'roster-row' + (playerIndex === activePlayer ? ' active' : '') + (game.phase === 2 && player.pa ? ' passed' : '');
    row.style.setProperty('--player-color', '#' + player.col);
    var iconUrl = factionIconUrl(tableData, player.fa);
    if (iconUrl) {
      var icon = document.createElement('img');
      icon.src = iconUrl;
      icon.alt = '';
      row.appendChild(icon);
    }
    var name = document.createElement('span');
    name.textContent = (player.st || player.rs) && player.nm ? player.nm : 'Seat ' + (playerIndex + 1);
    row.appendChild(name);
    if (game.phase > 0 && playerIndex === game.speaker) {
      var speaker = document.createElement('span');
      speaker.className = 'speaker';
      speaker.textContent = 'Speaker';
      row.appendChild(speaker);
    }
    roster.appendChild(row);
  });
}
</script>
</body>
</html>
)=====";
