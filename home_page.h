#pragma once

// =============================================================================
// TI4 Hex Riser - Home Page (/)
// =============================================================================
// Landing page with links to the admin, player and projector pages, over a
// slowly turning holographic galaxy drawn on a canvas.
// =============================================================================

const char HOME_PAGE[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TI4 LED Table</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
:root { --cyan: #38d6ff; --cyan-dim: #38d6ff55; --gold: #fbbf24; }
html, body { min-height: 100%; background: #03050c; }
body {
  min-height: 100vh; display: flex; flex-direction: column; align-items: center; justify-content: center;
  gap: 52px; padding: 32px 16px; overflow: hidden;
  color: #cbd5e1; font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
}
#galaxy { position: fixed; inset: 0; width: 100%; height: 100%; z-index: 0; }
#grid {
  position: fixed; inset: 0; z-index: 0; pointer-events: none;
  background-image:
    linear-gradient(#38d6ff0a 1px, transparent 1px),
    linear-gradient(90deg, #38d6ff0a 1px, transparent 1px);
  background-size: 48px 48px;
  mask-image: radial-gradient(ellipse at center, #000 30%, transparent 80%);
  -webkit-mask-image: radial-gradient(ellipse at center, #000 30%, transparent 80%);
}
.title, nav, footer { position: relative; z-index: 1; }
footer {
  position: fixed; left: 0; right: 0; bottom: 18px; text-align: center;
  font-size: 0.78rem; letter-spacing: 0.2em; text-transform: uppercase; color: #6b8aa6;
  opacity: 0; animation: rise 0.9s ease-out 2s forwards;
}
footer a { color: #a5c8e4; text-decoration: none; border-bottom: 1px solid #38d6ff55; }
footer a:hover { color: #7fe6ff; border-bottom-color: #7fe6ff; }

.title { text-align: center; position: relative; }
.title::before {
  content: ""; position: absolute; inset: -60px -90px; z-index: -1; pointer-events: none;
  background: radial-gradient(ellipse at center, #03050ccc 0%, #03050c88 45%, transparent 72%);
}
.title > * { opacity: 0; animation: rise 1s ease-out forwards; }
.title .eyebrow {
  font-size: clamp(0.8rem, 2.4vw, 1.05rem); letter-spacing: 0.55em; text-transform: uppercase;
  color: #a5c8e4; margin-bottom: 14px; padding-left: 0.55em; animation-delay: 0.9s;
  text-shadow: 0 0 12px #000;
}
.title h1 {
  font-size: clamp(2.4rem, 8.5vw, 5rem); font-weight: 800; letter-spacing: 0.08em; line-height: 1.05;
  text-transform: uppercase; animation-delay: 1.1s;
  background: linear-gradient(180deg, #fff3c4 0%, #fbbf24 48%, #b45309 100%);
  -webkit-background-clip: text; background-clip: text; color: transparent;
  filter: drop-shadow(0 0 18px #fbbf2466) drop-shadow(0 2px 10px #000);
}
.title .subtitle {
  margin-top: 16px; font-size: clamp(0.85rem, 2.6vw, 1.15rem); letter-spacing: 0.4em;
  text-transform: uppercase; color: #7fe6ff; font-weight: 600; padding-left: 0.4em; animation-delay: 1.3s;
  text-shadow: 0 0 14px #38d6ffaa, 0 0 4px #000;
}

nav { display: grid; grid-template-columns: repeat(3, minmax(0, 220px)); gap: 20px; width: 100%; justify-content: center; }
nav a {
  --cut: 12px;
  position: relative; display: flex; align-items: center; justify-content: center; gap: 12px;
  padding: 22px 16px; text-decoration: none; opacity: 0; animation: rise 0.9s ease-out forwards;
  font-size: 1.05rem; font-weight: 600; letter-spacing: 0.14em; text-transform: uppercase; color: #e6f6ff;
  background: linear-gradient(180deg, #0b2236cc, #06121fcc);
  clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut));
  transition: transform 0.2s, filter 0.2s;
}
nav a:nth-child(1) { animation-delay: 1.5s; }
nav a:nth-child(2) { animation-delay: 1.62s; }
nav a:nth-child(3) { animation-delay: 1.74s; }
/* Border drawn as an inset layer so it follows the cut corners */
nav a::before {
  content: ""; position: absolute; inset: 0; padding: 1px; pointer-events: none;
  background: linear-gradient(135deg, var(--cyan), #38d6ff33 40%, #38d6ff33 60%, var(--cyan));
  clip-path: inherit;
  -webkit-mask: linear-gradient(#000 0 0) content-box, linear-gradient(#000 0 0);
  -webkit-mask-composite: xor; mask-composite: exclude;
}
nav a:hover { transform: translateY(-2px); filter: drop-shadow(0 0 14px #38d6ff88); }
nav a svg { width: 24px; height: 24px; flex-shrink: 0; stroke: var(--gold); fill: none; stroke-width: 1.8; stroke-linecap: round; stroke-linejoin: round; }

@keyframes rise { from { opacity: 0; transform: translateY(12px); } to { opacity: 1; transform: none; } }
@media (max-width: 640px) {
  nav { grid-template-columns: minmax(0, 340px); }
}
@media (prefers-reduced-motion: reduce) {
  .title > *, nav a, footer { animation: none; opacity: 1; }
}
</style>
</head>
<body>

<canvas id="galaxy"></canvas>
<div id="grid"></div>

<div class="title">
  <div class="eyebrow">Twilight Imperium</div>
  <h1>LED Table</h1>
  <div class="subtitle">&amp; Game Manager</div>
</div>

<nav>
  <a href="/admin">
    <svg viewBox="0 0 24 24"><path d="M12 3l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 16.4 6.8 19.1l1-5.8L3.5 9.2l5.9-.9z"/></svg>
    Admin
  </a>
  <a href="/play">
    <svg viewBox="0 0 24 24"><rect x="7" y="2.5" width="10" height="19" rx="2"/><path d="M11 18.5h2"/></svg>
    Player
  </a>
  <a href="/projector">
    <svg viewBox="0 0 24 24"><rect x="2.5" y="4" width="19" height="13" rx="1.5"/><path d="M8 21h8M12 17v4"/></svg>
    Projector
  </a>
</nav>

<footer>Created by <a href="https://github.com/Evident-Dev" target="_blank" rel="noopener">Evident</a></footer>

<script>
// Holographic spiral galaxy. The face-on galaxy is drawn once into an
// offscreen canvas, then each frame it's rotated and tilted into place over
// a twinkling starfield, so the per-frame cost stays tiny.
(function () {
  var canvas  = document.getElementById('galaxy');
  var context = canvas.getContext('2d');
  var reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  var GALAXY_SIZE      = 1100;     // offscreen render size in pixels
  var MAJOR_ARMS       = 2;
  var ARM_WINDING      = 2.8;      // how tightly the logarithmic arms wind
  var INNER_RADIUS     = 0.05;     // where the arms start, as a fraction of the radius
  var BACKGROUND_STARS = 300;
  var ROTATION_SPEED   = 0.00003;  // radians per millisecond
  var TILT             = 0.58;     // vertical squash for the tilted disc
  var DISC_ANGLE       = -0.22;    // radians the disc is turned on screen
  var FADE_IN_MS       = 2400;

  var width = 0, height = 0, galaxyRadius = 0;
  var stars = [];
  var galaxyImage = document.createElement('canvas');

  function random(minimum, maximum) { return minimum + Math.random() * (maximum - minimum); }

  // Bell-shaped value between -1 and 1, dense near 0
  function spread() { return (Math.random() + Math.random() + Math.random() - 1.5) / 1.5; }

  // Angle of an arm at a given distance (logarithmic spiral)
  function armAngle(armOffset, distance) {
    return armOffset + ARM_WINDING * Math.log(distance / INNER_RADIUS);
  }

  function dot(drawContext, x, y, size, color) {
    drawContext.fillStyle = color;
    drawContext.fillRect(x - size / 2, y - size / 2, size, size);
  }

  function glow(drawContext, x, y, glowRadius, color) {
    var gradient = drawContext.createRadialGradient(x, y, 0, x, y, glowRadius);
    gradient.addColorStop(0, color);
    gradient.addColorStop(1, 'rgba(0,0,0,0)');
    drawContext.fillStyle = gradient;
    drawContext.fillRect(x - glowRadius, y - glowRadius, glowRadius * 2, glowRadius * 2);
  }

  // Draws the face-on galaxy once.
  function renderGalaxy() {
    galaxyImage.width = galaxyImage.height = GALAXY_SIZE;
    var drawContext = galaxyImage.getContext('2d');
    var center = GALAXY_SIZE / 2;
    var radius = GALAXY_SIZE / 2 * 0.96;
    drawContext.globalCompositeOperation = 'lighter';

    // Arms: two major, two slightly fainter arms evenly between them
    var arms = [];
    for (var major = 0; major < MAJOR_ARMS; major++) {
      arms.push({ offset: major * Math.PI * 2 / MAJOR_ARMS, strength: 1 });
      arms.push({ offset: major * Math.PI * 2 / MAJOR_ARMS + Math.PI / MAJOR_ARMS, strength: 0.75 });
    }

    arms.forEach(function (arm) {
      // Nebula haze along the arm, fading toward the tip
      for (var haze = 0; haze < 220 * arm.strength; haze++) {
        var hazeDistance = INNER_RADIUS + Math.pow(Math.random(), 0.9) * (1 - INNER_RADIUS);
        var hazeAngle = armAngle(arm.offset, hazeDistance) + spread() * 0.4;
        var fadeOut = 1 - hazeDistance;
        glow(drawContext,
             center + Math.cos(hazeAngle) * hazeDistance * radius,
             center + Math.sin(hazeAngle) * hazeDistance * radius,
             radius * random(0.05, 0.11) * (0.6 + hazeDistance),
             'rgba(40,150,255,' + (0.075 * fadeOut * arm.strength) + ')');
      }

      // Stars along the arm, scattering wider and fading toward the tip
      var starCount = Math.round(4600 * arm.strength);
      for (var index = 0; index < starCount; index++) {
        var distance = INNER_RADIUS + Math.pow(Math.random(), 0.75) * (1 - INNER_RADIUS);
        var angle = armAngle(arm.offset, distance) + spread() * (0.3 + distance * 0.5);
        var scatter = spread() * 0.045;
        var x = center + Math.cos(angle) * (distance + scatter) * radius;
        var y = center + Math.sin(angle) * (distance + scatter) * radius;
        var fade = Math.pow(1 - distance, 0.7) * arm.strength;
        var hue = 195 + distance * 10 + random(-6, 6);
        var lightness = 70 + (1 - distance) * 25;
        dot(drawContext, x, y, Math.random() < 0.05 ? 2.4 : random(0.8, 1.7),
            'hsla(' + hue + ',95%,' + lightness + '%,' + (random(0.35, 0.95) * fade) + ')');
      }

      // Bright star clusters dotted along the arm
      for (var cluster = 0; cluster < 14 * arm.strength; cluster++) {
        var clusterDistance = random(0.15, 0.85);
        var clusterAngle = armAngle(arm.offset, clusterDistance) + spread() * 0.1;
        var clusterX = center + Math.cos(clusterAngle) * clusterDistance * radius;
        var clusterY = center + Math.sin(clusterAngle) * clusterDistance * radius;
        glow(drawContext, clusterX, clusterY, radius * 0.03, 'rgba(120,200,255,' + (0.14 * (1 - clusterDistance)) + ')');
        for (var member = 0; member < 8; member++) {
          dot(drawContext, clusterX + spread() * radius * 0.02, clusterY + spread() * radius * 0.02,
              random(0.8, 1.5), 'rgba(210,240,255,0.45)');
        }
      }
    });

    // Dust lanes: dark streaks just inside the major arms
    drawContext.globalCompositeOperation = 'source-over';
    for (var majorArm = 0; majorArm < MAJOR_ARMS; majorArm++) {
      var laneOffset = majorArm * Math.PI * 2 / MAJOR_ARMS - 0.22;
      for (var dust = 0; dust < 900; dust++) {
        var dustDistance = 0.12 + Math.random() * 0.7;
        var dustAngle = armAngle(laneOffset, dustDistance) + spread() * 0.05;
        dot(drawContext,
            center + Math.cos(dustAngle) * dustDistance * radius,
            center + Math.sin(dustAngle) * dustDistance * radius,
            random(2, 5), 'rgba(2,6,16,' + random(0.15, 0.35) + ')');
      }
    }
    drawContext.globalCompositeOperation = 'lighter';

    // Faint disc of loose stars between the arms
    for (var loose = 0; loose < 4500; loose++) {
      var looseDistance = Math.pow(Math.random(), 0.55);
      var looseAngle = random(0, Math.PI * 2);
      dot(drawContext,
          center + Math.cos(looseAngle) * looseDistance * radius,
          center + Math.sin(looseAngle) * looseDistance * radius,
          random(0.6, 1.3), 'rgba(170,215,255,' + (random(0.15, 0.5) * (1 - looseDistance * 0.8)) + ')');
    }

    // Central bulge: dense warm stars and glow
    glow(drawContext, center, center, radius * 0.32, 'rgba(255,220,170,0.28)');
    glow(drawContext, center, center, radius * 0.16, 'rgba(255,240,215,0.5)');
    for (var bulge = 0; bulge < 2600; bulge++) {
      var bulgeDistance = Math.abs(spread()) * 0.16;
      var bulgeAngle = random(0, Math.PI * 2);
      dot(drawContext,
          center + Math.cos(bulgeAngle) * bulgeDistance * radius,
          center + Math.sin(bulgeAngle) * bulgeDistance * radius,
          random(0.8, 1.8), 'rgba(255,' + Math.round(random(225, 250)) + ',' + Math.round(random(190, 235)) + ',' + random(0.3, 0.8) + ')');
    }
  }

  function buildStars() {
    stars = [];
    for (var star = 0; star < BACKGROUND_STARS; star++) {
      stars.push({ x: Math.random(), y: Math.random(), size: random(0.5, 1.6),
                   twinkleSpeed: random(0.0006, 0.002), phase: random(0, Math.PI * 2) });
    }
  }

  function resize() {
    var scale = Math.min(window.devicePixelRatio || 1, 1.5);
    width  = window.innerWidth;
    height = window.innerHeight;
    canvas.width  = width * scale;
    canvas.height = height * scale;
    context.setTransform(scale, 0, 0, scale, 0, 0);
    galaxyRadius = Math.min(width * 0.5, height * 0.85);
  }

  function drawFrame(time) {
    var fade     = reducedMotion ? 1 : Math.min(time / FADE_IN_MS, 1);
    var growth   = 0.86 + 0.14 * (1 - Math.pow(1 - fade, 3));
    var centerX  = width / 2, centerY = height / 2;
    var radius   = galaxyRadius * growth;
    // Turns against the arm winding so the arm tips trail behind, like a real galaxy
    var rotation = reducedMotion ? 0 : -time * ROTATION_SPEED;

    context.globalCompositeOperation = 'source-over';
    context.globalAlpha = 1;
    context.fillStyle = '#03050c';
    context.fillRect(0, 0, width, height);

    for (var starIndex = 0; starIndex < stars.length; starIndex++) {
      var star = stars[starIndex];
      var twinkle = reducedMotion ? 0.7 : 0.4 + 0.6 * Math.abs(Math.sin(time * star.twinkleSpeed + star.phase));
      context.fillStyle = 'rgba(220,235,255,' + (twinkle * 0.85) + ')';
      context.fillRect(star.x * width, star.y * height, star.size, star.size);
    }

    context.globalCompositeOperation = 'lighter';
    context.globalAlpha = fade;

    // Orbit rings, partial arcs like a hologram readout
    context.lineWidth = 1;
    for (var ring = 1; ring <= 6; ring++) {
      var ringRadius = radius * ring / 6.2;
      var arcStart = ring * 1.7 + rotation * (ring % 2 ? 1 : -1) * 0.8;
      context.strokeStyle = 'rgba(56,214,255,' + (0.05 + 0.04 * (ring % 2)) + ')';
      context.beginPath();
      context.ellipse(centerX, centerY, ringRadius, ringRadius * TILT, DISC_ANGLE, arcStart, arcStart + Math.PI * 1.35);
      context.stroke();
    }

    // The galaxy itself, tilted and turning
    context.save();
    context.translate(centerX, centerY);
    context.rotate(DISC_ANGLE);
    context.scale(1, TILT);
    context.rotate(rotation);
    context.drawImage(galaxyImage, -radius, -radius, radius * 2, radius * 2);
    context.restore();

    // Core flare
    var pulse = reducedMotion ? 1 : 0.92 + 0.08 * Math.sin(time * 0.0012);
    var coreRadius = radius * 0.07 * pulse;
    var core = context.createRadialGradient(centerX, centerY, 0, centerX, centerY, coreRadius);
    core.addColorStop(0, 'rgba(255,255,255,0.95)');
    core.addColorStop(0.3, 'rgba(255,240,215,0.45)');
    core.addColorStop(1, 'rgba(255,220,170,0)');
    context.fillStyle = core;
    context.beginPath();
    context.arc(centerX, centerY, coreRadius, 0, Math.PI * 2);
    context.fill();
  }

  var startTime = null;
  function animate(timestamp) {
    if (startTime === null) startTime = timestamp;
    drawFrame(timestamp - startTime);
    if (!reducedMotion) requestAnimationFrame(animate);
  }

  resize();
  buildStars();
  renderGalaxy();
  window.addEventListener('resize', function () { resize(); if (reducedMotion) drawFrame(0); });
  requestAnimationFrame(animate);
})();
</script>
</body>
</html>
)=====";
