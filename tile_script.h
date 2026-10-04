#pragma once

// =============================================================================
// TI4 Hex Riser - Tile and Faction Images (/tiles.js)
// =============================================================================
// Loads the TI4 tile and faction indexes from the image CDN so pages can show
// tile art and faction icons. The board only stores tile numbers and faction
// ids. With no internet (AP mode) loading fails quietly and pages fall back
// to plain hexes.
//
//   loadTableData() -> Promise of { tiles, factions } or null when offline
//   tileInfo(data, tileNumber), tileImageUrl(data, tileNumber)
//   factionIconUrl(data, factionId)
//   factionsBySet(data) -> [{ label, factions: [{ id, name, homeSystem }] }]
//   parseTtsMapString(text) -> { center, tiles: [{ tileNumber, rotation }] }
// =============================================================================

const char TILE_SCRIPT[] = R"=====(
(function () {
  var CDN_URL = 'https://evident-dev.github.io/TI4-Images/';
  var LOAD_TIMEOUT_MS = 6000;

  // Display order and names for the faction sets in the CDN index
  var FACTION_SETS = [
    { id: 'base',          label: 'Base Game' },
    { id: 'pok',           label: 'Prophecy of Kings' },
    { id: 'keleres',       label: 'Codex' },
    { id: 'te',            label: "Thunder's Edge" },
    { id: 'discordant',    label: 'Discordant Stars' },
    { id: 'discordantexp', label: 'Discordant Stars Expansion' }
  ];

  var dataPromise = null;

  function fetchJson(path) {
    var controller = window.AbortController ? new AbortController() : null;
    var timer = setTimeout(function () { if (controller) controller.abort(); }, LOAD_TIMEOUT_MS);
    return fetch(CDN_URL + path, controller ? { signal: controller.signal } : {})
      .then(function (response) {
        clearTimeout(timer);
        if (!response.ok) throw new Error('HTTP ' + response.status);
        return response.json();
      });
  }

  window.loadTableData = function () {
    if (!dataPromise) {
      dataPromise = Promise.all([fetchJson('index/tiles.json'), fetchJson('index/factions.json')])
        .then(function (results) { return { tiles: results[0], factions: results[1] }; })
        .catch(function () { return null; });
    }
    return dataPromise;
  };

  // Looks up a tile, forgiving letter case ("83a" finds "83A")
  window.tileInfo = function (data, tileNumber) {
    if (!data || !tileNumber) return null;
    return data.tiles[tileNumber] || data.tiles[tileNumber.toUpperCase()] || data.tiles[tileNumber.toLowerCase()] || null;
  };

  window.tileImageUrl = function (data, tileNumber) {
    var tile = window.tileInfo(data, tileNumber);
    return tile ? CDN_URL + tile.image : null;
  };

  window.factionIconUrl = function (data, factionId) {
    var faction = data && factionId ? data.factions[factionId] : null;
    return faction ? CDN_URL + faction.icon : null;
  };

  window.factionsBySet = function (data) {
    if (!data) return [];
    var groups = FACTION_SETS.map(function (set) { return { id: set.id, label: set.label, factions: [] }; });
    Object.keys(data.factions).forEach(function (factionId) {
      var faction = data.factions[factionId];
      var group = groups.filter(function (candidate) { return candidate.id === faction.set; })[0];
      if (!group) {
        group = { id: faction.set, label: faction.set, factions: [] };
        groups.push(group);
      }
      group.factions.push({ id: factionId, name: faction.name, homeSystem: faction.homeSystem });
    });
    groups.forEach(function (group) {
      group.factions.sort(function (first, second) {
        return first.name.replace(/^The /, '').localeCompare(second.name.replace(/^The /, ''));
      });
    });
    return groups.filter(function (group) { return group.factions.length > 0; });
  };

  // TTS map string: tiles listed ring by ring from the top of ring 1,
  // clockwise. "{18}" first means the center tile is included. 0 or -1 is an
  // empty spot (home systems in a Milty Draft string). A hyperlane's trailing
  // digit is its rotation, as in "83A2".
  window.parseTtsMapString = function (text) {
    var tokens = (text || '').trim().split(/[\s,]+/).filter(function (token) { return token.length > 0; });
    var center = null;
    if (tokens.length > 0 && /^\{.*\}$/.test(tokens[0])) center = parseMapToken(tokens.shift());
    return { center: center, tiles: tokens.map(parseMapToken) };
  };

  function parseMapToken(token) {
    token = token.replace(/[{}]/g, '');
    if (token === '' || token === '0' || token === '-1') return { tileNumber: '', rotation: 0 };
    var hyperlane = /^(\d+[AaBb])(\d)$/.exec(token);
    if (hyperlane) return { tileNumber: hyperlane[1].toUpperCase(), rotation: +hyperlane[2] % 6 };
    return { tileNumber: token, rotation: 0 };
  }
})();
)=====";
