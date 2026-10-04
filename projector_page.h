#pragma once

// =============================================================================
// TI4 Hex Riser - Projector Page (/projector)
// =============================================================================
// Full-screen live mirror of the table for a projector or TV. Display only.
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
html, body { height: 100%; background: #05070d; overflow: hidden; }
#board { display: block; width: 100vw; height: 100vh; padding: 2vmin; }
</style>
<script src="/board.js"></script>
</head>
<body>
<svg id="board"></svg>
<script>
var board = createHexBoard(document.getElementById('board'));
connectTable({
  onFrame: board.renderLedFrame,
  onGame: function (game) { showRecoveryPrompt(game); }
});
</script>
</body>
</html>
)=====";
