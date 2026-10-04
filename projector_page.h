#pragma once

// =============================================================================
// TI4 Hex Riser - Projector Page (/projector)
// =============================================================================
// Full-screen live mirror of the table for a projector or TV. Display only.
// Shows the map tiles, a player list, and faction icons on claimed hexes.
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
html, body { height: 100%; background: #05070d; overflow: hidden; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }
#board { display: block; width: 100vw; height: 100vh; padding: 2vmin; }
#roster {
  position: fixed; top: 2.5vmin; left: 2.5vmin; display: flex; flex-direction: column; gap: 1vmin;
  font-size: 2vmin; color: #cbd5e1;
}
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
<svg id="board"></svg>
<div id="roster"></div>
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
