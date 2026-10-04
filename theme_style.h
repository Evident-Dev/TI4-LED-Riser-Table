#pragma once

// =============================================================================
// TI4 Hex Riser - Shared Theme (/theme.css)
// =============================================================================
// Holo button look used on every page: cut corners and a glowing edge.
// The edge is the element's own background showing around an inset fill, so
// it follows the cut corners. Tune a button with:
//   --edge, --edge-dim   edge color at the corners and along the sides
//   --edge-width         edge thickness
//   --fill               inside background
//   --cut                corner cut size
// Button variants: .on / .primary (selected), .gold, .danger, .small.
// Applies to every <button> plus anything with the holo class.
//
// Also here: .panel (cut-corner card), .tag (small label, .gold / .green),
// .panel-label (section heading), .holo-bar (top bar), page background,
// text inputs and selects.
// =============================================================================

const char THEME_STYLE[] = R"=====(
body {
  background-color: #04070f;
  background-image:
    linear-gradient(#38d6ff08 1px, transparent 1px),
    linear-gradient(90deg, #38d6ff08 1px, transparent 1px);
  background-size: 48px 48px;
  background-attachment: fixed;
}

button, .holo, .panel, .tag {
  --cut: 10px;
  --edge: #38d6ff;
  --edge-dim: #38d6ff40;
  --edge-width: 1px;
  --fill: linear-gradient(180deg, #0d2740, #07131f);
  position: relative; isolation: isolate;
  border: none; border-radius: 0;
  background: linear-gradient(135deg, var(--edge), var(--edge-dim) 42%, var(--edge-dim) 58%, var(--edge));
  color: #e2f4ff; font-family: inherit; font-weight: 600;
  letter-spacing: 0.06em; text-transform: uppercase;
  clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut));
  transition: filter 0.15s, transform 0.1s;
}
button { cursor: pointer; }
button::before, .holo::before, .panel::before, .tag::before {
  content: ""; position: absolute; inset: var(--edge-width); z-index: -1; pointer-events: none;
  background: var(--fill);
  clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut));
}
button:hover:not(:disabled), .holo:hover { filter: brightness(1.25); }
button:active:not(:disabled) { transform: translateY(1px); }
button:disabled { opacity: 0.38; cursor: default; }

button.on, button.primary, .holo.on {
  --edge: #7fe6ff; --edge-dim: #38d6ff99;
  --fill: linear-gradient(180deg, #0e5a85, #093552);
  color: #ffffff;
}
button.gold {
  --edge: #fbbf24; --edge-dim: #fbbf2455;
  --fill: linear-gradient(180deg, #7a3a0e, #4a2208);
  color: #fef3c7;
}
button.danger {
  --edge: #f87171; --edge-dim: #ef444455;
  --fill: linear-gradient(180deg, #4a0d0d, #2a0707);
  color: #fecaca;
}
button.small { --cut: 6px; }

/* Cards and sections: same edge, quieter, no hover */
.panel {
  --cut: 16px; --edge: #38d6ff70; --edge-dim: #38d6ff14;
  --fill: linear-gradient(180deg, #0a1828f5, #060e19f5);
  color: inherit; font-weight: inherit; letter-spacing: normal; text-transform: none;
}

/* Small labels: phase pill, badges */
.tag {
  --cut: 5px; --edge: #38d6ff55; --edge-dim: #38d6ff22; --fill: #071624;
  display: inline-block; padding: 4px 10px; color: #a5c8e4;
  font-size: 0.64rem; letter-spacing: 0.12em; white-space: nowrap;
}
.tag.gold  { --edge: #fbbf24; --edge-dim: #fbbf2444; --fill: #241604; color: #fbbf24; }
.tag.green { --edge: #4ade80; --edge-dim: #4ade8033; --fill: #05210f; color: #86efac; }

/* Section heading with a fading rule */
.panel-label {
  display: flex; align-items: center; gap: 10px; margin-bottom: 12px;
  font-size: 0.66rem; font-weight: 600; letter-spacing: 0.24em; text-transform: uppercase; color: #7fe6ff;
}
.panel-label::after { content: ""; flex: 1; height: 1px; background: linear-gradient(90deg, #38d6ff55, transparent); }

/* Top bar */
.holo-bar {
  background: linear-gradient(180deg, #0a1828, #060e19);
  border-bottom: 1px solid #38d6ff40; box-shadow: 0 1px 18px #38d6ff14;
}

/* Inputs */
input[type=text], input[type=password], input[type=number], select {
  background: #06111d; color: #e2f4ff; border: 1px solid #38d6ff40; border-radius: 0;
  font-family: inherit;
}
input[type=text]:focus, input[type=password]:focus, input[type=number]:focus, select:focus {
  outline: none; border-color: #7fe6ff; box-shadow: 0 0 0 1px #38d6ff55, 0 0 14px #38d6ff33;
}
input[type=range], input[type=checkbox] { accent-color: #38d6ff; }

[hidden] { display: none !important; }
)=====";
