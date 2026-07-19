// ============================================================
// Stream Manager — Console Simulator
// Runs the app's core logic in Node.js without Chaturbate IDE.
// Usage: node simulator.js
// ============================================================

'use strict';

// ── Deterministic mocks for reproducible simulation runs ──
// Override Date.now and Math.random so every run produces identical output.
// This enables regression testing by diffing against a reference run.

var _mockTime = 1700000000000; // fixed base timestamp (Nov 14, 2023)
var _seed = 42;

Date.now = function () {
  _mockTime += 1000; // increment by 1 second per call
  return _mockTime;
};

Math.random = function () {
  // Park-Miller LCG for reproducible pseudo-random numbers
  _seed = (_seed * 16807) % 2147483647;
  return (_seed - 1) / 2147483646;
};

// ── Helpers ──

var noticedLog = [];

function pad(s, n) { s = String(s); while (s.length < n) s = ' ' + s; return s; }

// ── Mock $kv (Map-backed, full API) ──

var kvStore = new Map();

var $kv = {
  get: function (key, def) {
    if (kvStore.has(key)) return kvStore.get(key);
    if (def !== undefined) return def;
    return undefined;
  },
  set: function (key, val, expire) {
    kvStore.set(key, val);
    if (expire && typeof expire === 'number') {
      setTimeout(function () { kvStore.delete(key); }, expire * 1000);
    }
    return true;
  },
  incr: function (key, amt) {
    var v = kvStore.has(key) ? Number(kvStore.get(key)) || 0 : 0;
    v += (typeof amt === 'number' ? amt : 1);
    kvStore.set(key, v);
  },
  decr: function (key, amt) {
    var v = kvStore.has(key) ? Number(kvStore.get(key)) || 0 : 0;
    v -= (typeof amt === 'number' ? amt : 1);
    kvStore.set(key, v);
  },
  remove: function (key) { return kvStore.delete(key); },
  clear: function () { kvStore.clear(); return true; },
  iter: function (prefix) {
    var entries = Array.from(kvStore.entries());
    var idx = -1;
    var filtered = prefix
      ? entries.filter(function (e) { return e[0].indexOf(prefix) === 0; })
      : entries;
    return {
      next: function () { idx++; return idx < filtered.length; },
      key: function () { return idx >= 0 && idx < filtered.length ? filtered[idx][0] : undefined; },
      value: function () { return idx >= 0 && idx < filtered.length ? filtered[idx][1] : undefined; },
      seek: function (k) {
        for (var i = 0; i < filtered.length; i++) {
          if (filtered[i][0] === k) { idx = i; return; }
        }
        idx = filtered.length;
      },
      delete: function () {
        if (idx >= 0 && idx < filtered.length) {
          kvStore.delete(filtered[idx][0]);
          filtered.splice(idx, 1);
          idx--;
          return true;
        }
        return false;
      },
    };
  },
};

// ── Mock globals ──

var $app = { name: 'Stream Manager', version: '1.0.0', summary: 'Broadcaster utility', description: 'Monitors room activity and tracks goals', permission: { chatMessageTransform: true, broadcastPanelUpdate: true, tipDialogOpen: true } };

var $settings = {
  tipGoal: 1000,
  bannedWords: 'freak,darn,shoot',
  voteOptions: [['Sing', 'Sing a song'], ['Dance', 'Do a dance'], ['Joke', 'Tell a joke']],
  overlayEnabled: true,
  hiddenCamEnabled: true,
  hiddenCamMessage: 'Hidden cam is active! Tip {{price}}+ tokens for access.',
  hiddenCamPrice: 100,
  hiddenCamAutoGrant: true,
  tipMenuEnabled: true,
  tipMenuLabel: 'Choose a menu item:',
  tipMenuItems: [['Dance', 'A short dance', 10], ['Sing', 'Sing a song', 25], ['Joi', 'JOI session', 50]],
  discountsEnabled: true,
  discountEntries: [['fanclub', '*', 20], ['follower', 'Dance', 10]],
  anonConverterEnabled: true,
  anonConverterInterval: 180,
  anonConverterBlackoutDuration: 15,
  anonConverterMessage: 'Uninterrupted cam access for registered users! Sign up free at {{link}}',
  anonConverterAffiliateLink: 'https://chaturbate.com/ref/signup/',
  progressTheme: 'coral-ember',
  progressBarStyle: 'bar',
  progressTextMode: 'auto',
  notificationCount: 5,
  notificationAutoEnabled: true,
  notificationAutoIntervalMin: 60,
  notificationAutoIntervalMax: 300,
  notificationReplyEnabled: true,
  overlayNotificationsEnabled: false,
};

var $room = {
  anonCount: 3,
  chatAllowedBy: 'everyone',
  fanclubPrice: 9.99,
  followerCount: 1500,
  genders: ['female'],
  owner: 'broadcaster',
  ownerGender: 'female',
  psEnabled: true,
  psMinBalance: 100,
  psMinTime: 3,
  psPrice: 30,
  psRecEnabled: false,
  psSpyPrice: 15,
  status: 'public',
  subject: 'Welcome!',
  users: ['alice', 'bob', 'charlie'],
  reloadPanel: function () { /* handled by panel call tracking */ },
  sendNotice: function (msg, opts) {
    opts = opts || {};
    var target = opts.toUsername || 'room';
    noticedLog.push({ target: target, msg: String(msg), opts: opts });
    console.log('[NOTICE \u2192 ' + target + '] ' + msg);
  },
  setPanelTemplate: function (opts) {
    if (opts.template === 'image_template' && opts.table) {
      var t = opts.table;
      var lines = [];
      var rowKeys = Object.keys(t).sort();
      for (var ri = 0; ri < rowKeys.length; ri++) {
        var rk = rowKeys[ri];
        var row = t[rk];
        var cells = [];
        if (row.col_1) cells.push(row.col_1.value);
        if (row.col_2) cells.push(row.col_2.value);
        lines.push(rk + ': ' + cells.join(' | '));
      }
      console.log('[PANEL] ' + lines.join('  '));
    } else {
      console.log('[PANEL] Set: ' + Object.keys(opts).filter(function (k) { return k !== 'template'; }).map(function (k) { return k + '="' + opts[k] + '"'; }).join(', '));
    }
    return true;
  },
  setSubject: function (s) { $room.subject = s; },
  setTipOptions: function (opts) {
    console.log('[TIP_OPTIONS] Set: "' + opts.label + '" with ' + opts.options.length + ' options');
    opts.options.forEach(function (o, i) { console.log('  [' + (i + 1) + '] ' + o.label); });
  },
};

var $callback = {
  _timers: [],
  create: function (label, delay, repeating) {
    console.log('[CALLBACK] Created "' + label + '" (delay=' + delay + 's, repeating=' + !!repeating + ')');
    $callback._timers.push({ label: label, delay: delay, repeating: repeating, fired: 0 });
  },
  cancel: function (label) {
    var before = $callback._timers.length;
    $callback._timers = $callback._timers.filter(function (t) { return t.label !== label; });
    if ($callback._timers.length < before) {
      console.log('[CALLBACK] Cancelled "' + label + '"');
    }
  },
  label: '',
};

var $tip = { tokens: 0, message: '', isAnon: false };
var $user = { username: '', gender: 'male', subgender: null, colorGroup: null, language: 'en', inFanclub: false, fcAutoRenew: false, hasTokens: true, hasDarkmode: false, isMod: false, isOwner: false, isFollower: true, isBroadcasting: false, isSilenced: false, inPrivateShow: false, tippedRecently: false, tippedAlotRecently: false, tippedTonsRecently: false, recentTips: 'none' };
var $message = { body: '', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '', setBody: function (b) { $message.body = b; }, setBgColor: function (c) { $message.bgColor = c; }, setColor: function (c) { $message.color = c; }, setFont: function (f) { $message.font = f; }, setSpam: function (s) { $message.isSpam = s; } };
var $fanclub = { isNew: true };
var $media = { id: 'media_001', name: 'Photo Set #1', tokens: 150, type: 'photos' };
var $shortcut = { name: 'tipGoal', title: 'Tip Goal', description: 'Show tip goal progress' };
var $overlay = {
  _events: {},
  emit: function (ev, data) {
    console.log('[OVERLAY] Emit "' + ev + '": ' + JSON.stringify(data));
    if ($overlay._events[ev]) {
      $overlay._events[ev].forEach(function (cb) { cb(data); });
    }
  },
  on: function (ev, cb) {
    if (!$overlay._events[ev]) $overlay._events[ev] = [];
    $overlay._events[ev].push(cb);
  },
};
var $limitcam = { active: false, users: [], add: function (u) { if (Array.isArray(u)) u.forEach(function (x) { if ($limitcam.users.indexOf(x) === -1) $limitcam.users.push(x); }); }, hasAccess: function (u) { return $limitcam.users.indexOf(u) !== -1; }, remove: function (u) { if (Array.isArray(u)) u.forEach(function (x) { var i = $limitcam.users.indexOf(x); if (i !== -1) $limitcam.users.splice(i, 1); }); }, removeAll: function () { $limitcam.users = []; }, start: function (msg, u) { $limitcam.active = true; console.log('[LIMITCAM] Started: "' + (msg || '') + '"'); if (u) $limitcam.add(u); }, stop: function () { $limitcam.active = false; console.log('[LIMITCAM] Stopped'); } };

// ── Handler function definitions (mirroring stream-manager handlers) ──

var APP_NAME = 'Stream Manager';
var APP_VERSION = '1.2.0';
var SUPPORTED_SHOW_TYPES = ['public', 'private', 'hidden'];
var TOP_TIPPER_KEY = 'topTipper';
var TOP_TIP_TOKENS_KEY = 'topTipTokens';
var SESSION_TIPS_KEY = 'sessionTips';
var TIP_GOAL_KEY = 'tipGoal';
var NEW_FOLLOWERS_KEY = 'newFollowers';
var SESSION_START_KEY = 'sessionStart';
var REGULAR_USERS_KEY = 'regularUsers';
var HIDDEN_CAM_KEY = 'hiddenCamActive';
var HIDDEN_CAM_PRICE_KEY = 'hiddenCamPrice';
var HIDDEN_CAM_MSG_KEY = 'hiddenCamMessage';
var DEFAULT_HIDDEN_CAM_PRICE = 100;
var ANON_CONVERTER_KEY = 'anonConverterActive';
var ANON_CONVERTER_BLACKOUT_KEY = 'anonConverterBlackedOut';
var ANON_CONVERTER_INTERVAL_KEY = 'anonConverterInterval';
var ANON_CONVERTER_DURATION_KEY = 'anonConverterDuration';
var ANON_CONVERTER_CB_LABEL = 'anonConverterTick';
var ANON_CONVERTER_RESTORE_LABEL = 'anonConverterRestore';
var DEFAULT_ANON_INTERVAL = 180;
var DEFAULT_ANON_DURATION = 15;

var PANEL_CYCLE_LABEL = 'panelCycleTick';
var PANEL_VIEW_KEY = 'panelCurrentView';
var ANON_BLACKLIST_KEY = 'anonBlacklist';
var NOTIFICATION_QUEUE_KEY = 'notificationQueue';
var NOTIFICATION_AUTO_LABEL = 'notificationAutoTick';
var PANEL_RELOAD_DEBOUNCE_LABEL = 'panelReloadDebounce';

// Progress bar constants (mirrored from shared.js)
var PROGRESS_STYLE_KEY = 'progressStyle';
var PROGRESS_THEME_KEY = 'progressTheme';
var PROGRESS_TEXT_MODE_KEY = 'progressTextMode';
var PROGRESS_SCROLL_KEY = 'progressScrollOffset';
var PROGRESS_ANIMATE_KEY = 'progressAnimateTick';
var DEFAULT_PROGRESS_STYLE = 'bar';
var DEFAULT_PROGRESS_THEME = 'coral-ember';
var DEFAULT_TEXT_MODE = 'auto';

var PROGRESS_THEMES = [
  { id: 'coral-ember',     name: 'Coral Ember',     type: 'white', fill: '#FF6B6B', track: '#FFF0F0', text: '#333', label: '#888', accent: '#FF6B6B' },
  { id: 'golden-glow',     name: 'Golden Glow',     type: 'white', fill: '#FFB300', track: '#FFF8E0', text: '#333', label: '#888', accent: '#FFB300' },
  { id: 'lime-zest',       name: 'Lime Zest',       type: 'white', fill: '#43A047', track: '#F0FFF0', text: '#333', label: '#888', accent: '#43A047' },
  { id: 'ocean-breeze',    name: 'Ocean Breeze',    type: 'white', fill: '#1E88E5', track: '#E0F4FF', text: '#333', label: '#888', accent: '#1E88E5' },
  { id: 'lavender-mist',   name: 'Lavender Mist',   type: 'white', fill: '#8E24AA', track: '#F3E8FF', text: '#333', label: '#888', accent: '#8E24AA' },
  { id: 'peach-sorbet',    name: 'Peach Sorbet',    type: 'white', fill: '#FF7043', track: '#FFF0E0', text: '#333', label: '#888', accent: '#FF7043' },
  { id: 'mint-dew',        name: 'Mint Dew',        type: 'white', fill: '#00897B', track: '#E0FFF0', text: '#333', label: '#888', accent: '#00897B' },
  { id: 'rose-quartz',     name: 'Rose Quartz',     type: 'white', fill: '#D81B60', track: '#FFE8F0', text: '#333', label: '#888', accent: '#D81B60' },
  { id: 'sky-blue',        name: 'Sky Blue',        type: 'white', fill: '#0D47A1', track: '#E0F0FF', text: '#333', label: '#888', accent: '#0D47A1' },
  { id: 'silver-gray',     name: 'Silver Gray',     type: 'white', fill: '#546E7A', track: '#F0F0F0', text: '#333', label: '#888', accent: '#546E7A' },
  { id: 'ruby-night',      name: 'Ruby Night',      type: 'black', fill: '#FF1744', track: '#1A0000', text: '#FFF', label: '#888', accent: '#FF1744' },
  { id: 'amber-dark',      name: 'Amber Dark',      type: 'black', fill: '#FF9100', track: '#1A0A00', text: '#FFF', label: '#888', accent: '#FF9100' },
  { id: 'neon-green',      name: 'Neon Green',      type: 'black', fill: '#00E676', track: '#001A05', text: '#FFF', label: '#888', accent: '#00E676' },
  { id: 'cyber-teal',      name: 'Cyber Teal',      type: 'black', fill: '#1DE9B6', track: '#001A18', text: '#FFF', label: '#888', accent: '#1DE9B6' },
  { id: 'deep-cyan',       name: 'Deep Cyan',       type: 'black', fill: '#00BCD4', track: '#001A24', text: '#FFF', label: '#888', accent: '#00BCD4' },
  { id: 'royal-indigo',    name: 'Royal Indigo',    type: 'black', fill: '#536DFE', track: '#08001A', text: '#FFF', label: '#888', accent: '#536DFE' },
  { id: 'ultra-violet',    name: 'Ultra Violet',    type: 'black', fill: '#E040FB', track: '#14001A', text: '#FFF', label: '#888', accent: '#E040FB' },
  { id: 'hot-pink',        name: 'Hot Pink',        type: 'black', fill: '#FF4081', track: '#1A000A', text: '#FFF', label: '#888', accent: '#FF4081' },
  { id: 'electric-yellow', name: 'Electric Yellow', type: 'black', fill: '#FFEA00', track: '#1A1A00', text: '#222', label: '#AAA', accent: '#FFEA00' },
  { id: 'acid-lime',       name: 'Acid Lime',       type: 'black', fill: '#76FF03', track: '#0A1A00', text: '#222', label: '#AAA', accent: '#76FF03' },
];

var BAR_STYLES = [
  { id: 'bar',    name: 'Blocks',   fillChar: '\u2588', emptyChar: '\u2591' },
  { id: 'arrow',  name: 'Arrows',   fillChar: '\u25B6', emptyChar: '\u25B7' },
  { id: 'circle', name: 'Circles',  fillChar: '\u25CF', emptyChar: '\u25CB' },
  { id: 'square', name: 'Squares',  fillChar: '\u25A0', emptyChar: '\u25A1' },
  { id: 'number', name: 'Percent',  fillChar: '',       emptyChar: '' },
];

var BAR_CHAR_COUNT = 10;

// ============================================================
// Utility: determine which discount groups a user belongs to
// ============================================================
function getUserGroups(username) {
  var groups = ['all'];
  if ($user.isFollower) groups.push('follower');
  if ($user.inFanclub) groups.push('fanclub');
  if ($user.isMod) groups.push('mod');
  var reg = $kv.get(REGULAR_USERS_KEY, []);
  if (reg.indexOf(username) !== -1) groups.push('regular');
  groups.push('user:' + username);
  return groups;
}

// ============================================================
// Calculate discounted price for an item given a user's groups.
// ============================================================
function getDiscountedPrice(itemName, basePrice, username, discountEntries) {
  var result = { price: basePrice, discountPct: 0, isDiscounted: false };
  if (!discountEntries || !Array.isArray(discountEntries) || discountEntries.length === 0) return result;
  if (!$settings.discountsEnabled) return result;

  var groups = getUserGroups(username);
  var bestDiscount = 0;

  for (var i = 0; i < discountEntries.length; i++) {
    var entry = discountEntries[i];
    var entryGroup = String(entry[0] || '').trim();
    var entryItem = String(entry[1] || '').trim();
    var entryPct = Number(entry[2]) || 0;

    var groupMatch = false;
    for (var g = 0; g < groups.length; g++) {
      if (groups[g] === entryGroup) { groupMatch = true; break; }
    }
    if (!groupMatch) continue;

    var itemMatch = (entryItem === '*' || entryItem.toLowerCase() === itemName.toLowerCase());
    if (!itemMatch) continue;

    if (entryPct > bestDiscount) bestDiscount = entryPct;
  }

  if (bestDiscount > 0) {
    var discounted = Math.round(basePrice * (1 - bestDiscount / 100));
    result.price = discounted > 0 ? discounted : 1;
    result.discountPct = bestDiscount;
    result.isDiscounted = true;
  }
  return result;
}

// ============================================================
// Build the full tip options array for the tip dialog, with discounts applied.
// ============================================================
function buildTipMenuOptions(username) {
  var rawItems = $settings.tipMenuItems;
  var rawDiscounts = $settings.discountEntries;
  if (!rawItems || !Array.isArray(rawItems) || rawItems.length === 0) return null;

  var label = $settings.tipMenuLabel || 'Choose a menu item:';
  var options = [];

  for (var i = 0; i < rawItems.length; i++) {
    var item = rawItems[i];
    var name = String(item[0] || '').trim();
    var desc = String(item[1] || '').trim();
    var basePrice = Number(item[2]) || 0;
    if (!name || basePrice <= 0) continue;

    var priceInfo = getDiscountedPrice(name, basePrice, username, rawDiscounts);
    var optionLabel = name + ' (' + priceInfo.price + ' tks)';
    if (desc) optionLabel += ' \u2014 ' + desc;
    if (priceInfo.isDiscounted) optionLabel += ' [' + priceInfo.discountPct + '% off!]';
    options.push({ label: optionLabel });
  }
  if (options.length === 0) return null;
  return { label: label, options: options };
}

// ============================================================
// Format tip menu as a chat message (for !tipmenu command)
// ============================================================
function formatTipMenuText(username) {
  var rawItems = $settings.tipMenuItems;
  var rawDiscounts = $settings.discountEntries;
  if (!rawItems || !Array.isArray(rawItems) || rawItems.length === 0) return 'No tip menu items configured.';

  var lines = [];
  var maxNameLen = 0;
  for (var i = 0; i < rawItems.length; i++) {
    var nlen = String(rawItems[i][0] || '').trim().length;
    if (nlen > maxNameLen) maxNameLen = nlen;
  }

  for (var i = 0; i < rawItems.length; i++) {
    var item = rawItems[i];
    var name = String(item[0] || '').trim();
    var desc = String(item[1] || '').trim();
    var basePrice = Number(item[2]) || 0;
    if (!name || basePrice <= 0) continue;

    var priceInfo = getDiscountedPrice(name, basePrice, username, rawDiscounts);
    var parts = [name + ':'];
    for (var p = name.length; p < maxNameLen + 1; p++) parts.push(' ');
    parts[0] += ' ' + priceInfo.price + ' tks';
    if (desc) parts[0] += ' \u2014 ' + desc;
    if (priceInfo.isDiscounted) parts[0] += ' (was ' + basePrice + ', -' + priceInfo.discountPct + '%)';
    lines.push(parts[0]);
  }
  return lines.join(' | ');
}

// ============================================================
// Format discount info for a user (for !discounts command)
// ============================================================
function formatUserDiscountsText(username) {
  var rawDiscounts = $settings.discountEntries;
  if (!rawDiscounts || !Array.isArray(rawDiscounts) || rawDiscounts.length === 0) return 'No discounts configured.';
  if (!$settings.discountsEnabled) return 'Discounts are currently disabled.';

  var groups = getUserGroups(username);
  var lines = ['Your groups: ' + groups.join(', '), 'Active discounts:'];
  var found = false;
  for (var i = 0; i < rawDiscounts.length; i++) {
    var entry = rawDiscounts[i];
    var entryGroup = String(entry[0] || '').trim();
    var entryItem = String(entry[1] || '').trim();
    var entryPct = Number(entry[2]) || 0;
    var groupMatch = false;
    for (var g = 0; g < groups.length; g++) {
      if (groups[g] === entryGroup) { groupMatch = true; break; }
    }
    if (!groupMatch) continue;
    found = true;
    lines.push('  - ' + entryPct + '% off ' + (entryItem === '*' ? 'all items' : '"' + entryItem + '"') + ' (' + entryGroup + ')');
  }
  if (!found) lines.push('  (none apply to you)');
  return lines.join(' | ');
}

// ============================================================
// Anon Blacklist Utilities
// ============================================================

function getAnonBlacklist() {
  return $kv.get(ANON_BLACKLIST_KEY, []);
}

function addToAnonBlacklist(username) {
  var bl = getAnonBlacklist();
  if (bl.indexOf(username) !== -1) return false;
  bl.push(username);
  $kv.set(ANON_BLACKLIST_KEY, bl);
  return true;
}

function removeFromAnonBlacklist(username) {
  var bl = getAnonBlacklist();
  var idx = bl.indexOf(username);
  if (idx === -1) return false;
  bl.splice(idx, 1);
  $kv.set(ANON_BLACKLIST_KEY, bl);
  return true;
}

function isBlacklisted(username) {
  return getAnonBlacklist().indexOf(username) !== -1;
}

// ============================================================
// Leaderboard Utility
// ============================================================

function getTopTippers(count) {
  var limit = typeof count === 'number' ? count : 5;
  var results = [];
  var iter = $kv.iter('userTips_');
  while (iter.next()) {
    var k = iter.key();
    var username = k.replace('userTips_', '');
    var tokens = iter.value();
    results.push({ username: username, tokens: tokens });
  }
  results.sort(function(a, b) { return b.tokens - a.tokens; });
  return results.slice(0, limit);
}

// ============================================================
// Anon Converter Utilities
// ============================================================
function startAnonConverter() {
  if ($kv.get(ANON_CONVERTER_KEY, false)) return;
  $kv.set(ANON_CONVERTER_KEY, true);
  $kv.set(ANON_CONVERTER_BLACKOUT_KEY, false);
  var interval = $settings.anonConverterInterval || DEFAULT_ANON_INTERVAL;
  $kv.set(ANON_CONVERTER_INTERVAL_KEY, interval);
  $callback.create(ANON_CONVERTER_CB_LABEL, interval, true);
  notifyBroadcaster('Anonymous User Converter started (every ' + interval + 's).');
}

function stopAnonConverter() {
  if (!$kv.get(ANON_CONVERTER_KEY, false)) return;
  $callback.cancel(ANON_CONVERTER_CB_LABEL);
  $callback.cancel(ANON_CONVERTER_RESTORE_LABEL);
  if ($kv.get(ANON_CONVERTER_BLACKOUT_KEY, false)) {
    $limitcam.stop();
    $kv.set(ANON_CONVERTER_BLACKOUT_KEY, false);
  }
  $kv.set(ANON_CONVERTER_KEY, false);
  notifyBroadcaster('Anonymous User Converter stopped.');
}

function doAnonConverterBlackout() {
  if (!$settings.anonConverterEnabled) return;
  if ($room.status === 'offline') return;
  if ($kv.get(HIDDEN_CAM_KEY, false)) return;

  var msg = $settings.anonConverterMessage || '';
  var link = $settings.anonConverterAffiliateLink || '';
  msg = msg.replace('{{link}}', link);
  var duration = $settings.anonConverterBlackoutDuration || DEFAULT_ANON_DURATION;

  var users = ($room.users || []).filter(function(u) { return !isBlacklisted(u); });
  $limitcam.start(msg, users);
  $kv.set(ANON_CONVERTER_BLACKOUT_KEY, true);
  notifyBroadcaster('Anon Converter: cam blacked out for ' + duration + 's (' + $room.anonCount + ' anons affected).');

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('anonBlackout', { duration: duration, anonCount: $room.anonCount, message: msg, link: link });
  }
  $callback.create(ANON_CONVERTER_RESTORE_LABEL, duration, false);
}

function doAnonConverterRestore() {
  if (!$kv.get(ANON_CONVERTER_BLACKOUT_KEY, false)) return;
  // Don't stop limitcam if hidden cam is active — it was started independently
  if (!$kv.get(HIDDEN_CAM_KEY, false)) {
    $limitcam.stop();
  }
  $kv.set(ANON_CONVERTER_BLACKOUT_KEY, false);
  notifyBroadcaster('Anon Converter: cam feed restored.');
  if ($settings.overlayEnabled !== false) $overlay.emit('anonRestore', {});
}

// ============================================================
// Panel Cycle Utilities
// ============================================================

function startPanelCycle() {
  if ($kv.get(PANEL_VIEW_KEY, 0) === 0) $kv.set(PANEL_VIEW_KEY, 0);
  $callback.create(PANEL_CYCLE_LABEL, 6, true);
}

function stopPanelCycle() {
  $callback.cancel(PANEL_CYCLE_LABEL);
}

function doPanelCycleTick() {
  var view = ($kv.get(PANEL_VIEW_KEY, 0) + 1) % 3;
  $kv.set(PANEL_VIEW_KEY, view);
  $room.reloadPanel();
}

// ============================================================
// Hidden Cam Utilities
// ============================================================
function startHiddenCam(message) {
  if ($room.status === 'offline') { notifyBroadcaster('Cannot start hidden cam while offline.'); return false; }
  // Don't start hidden cam during anon converter blackout
  if ($kv.get(ANON_CONVERTER_BLACKOUT_KEY, false)) {
    notifyBroadcaster('Cannot start hidden cam while anon converter blackout is active.');
    return false;
  }
  var msg = message || $settings.hiddenCamMessage || '';
  $limitcam.start(msg);
  $kv.set(HIDDEN_CAM_KEY, true);
  notifyBroadcaster('Hidden cam is now active.');
  return true;
}

function stopHiddenCam() {
  $limitcam.stop();
  $kv.set(HIDDEN_CAM_KEY, false);
  notifyBroadcaster('Hidden cam has been stopped. Feed is now visible to everyone.');
}

function toggleHiddenCam(message) {
  if ($kv.get(HIDDEN_CAM_KEY, false)) { stopHiddenCam(); return 'stopped'; }
  return startHiddenCam(message) ? 'started' : 'failed';
}

function noticeExcludedUsers(msg) {
  var allUsers = $room.users || [];
  for (var i = 0; i < allUsers.length; i++) {
    var u = allUsers[i];
    if (u === $room.owner) continue;
    if (!$limitcam.hasAccess(u)) $room.sendNotice(msg, { toUsername: u });
  }
}

// ============================================================
// Generic Utilities
// ============================================================
function notifyBroadcaster(message, options) {
  options = options || {};
  var target = {};
  if ($room && $room.owner) target.toUsername = $room.owner;
  $room.sendNotice(message, Object.assign(target, options));
}

function incrCounter(key, amount) {
  // Use atomic increment (matching shared.js pattern)
  $kv.incr(key, typeof amount === 'number' ? amount : 1);
  return $kv.get(key, 0);
}

// ============================================================
// Debounced Panel Reload (mirrored from shared.js)
// ============================================================

function debouncedReloadPanel() {
  $callback.cancel(PANEL_RELOAD_DEBOUNCE_LABEL);
  $callback.create(PANEL_RELOAD_DEBOUNCE_LABEL, 1, false);
}

// ============================================================
// Progress Bar Utilities (mirrored from shared.js)
// ============================================================

function getProgressTheme() {
  var id = $kv.get(PROGRESS_THEME_KEY, DEFAULT_PROGRESS_THEME);
  for (var i = 0; i < PROGRESS_THEMES.length; i++) {
    if (PROGRESS_THEMES[i].id === id) return PROGRESS_THEMES[i];
  }
  return PROGRESS_THEMES[0];
}

function getProgressStyle() {
  var id = $kv.get(PROGRESS_STYLE_KEY, DEFAULT_PROGRESS_STYLE);
  for (var i = 0; i < BAR_STYLES.length; i++) {
    if (BAR_STYLES[i].id === id) return BAR_STYLES[i];
  }
  return BAR_STYLES[0];
}

function getProgressTextMode() {
  return $kv.get(PROGRESS_TEXT_MODE_KEY, DEFAULT_TEXT_MODE);
}

function buildProgressBarText(tips, goal, style, textMode, offset, animateTick) {
  var pct = goal > 0 ? Math.min(Math.round((tips / goal) * 100), 100) : 0;
  var shortPct = pct + '%';
  var rawText;

  if (style.id === 'number') {
    rawText = shortPct + ' (' + tips + '/' + goal + ')';
  } else {
    var fillCount = Math.round(BAR_CHAR_COUNT * pct / 100);
    var emptyCount = BAR_CHAR_COUNT - fillCount;
    var barStr = '';
    for (var i = 0; i < fillCount; i++) barStr += style.fillChar;
    for (var i = 0; i < emptyCount; i++) barStr += style.emptyChar;
    rawText = barStr + ' ' + shortPct;
  }

  if (textMode === 'auto') textMode = rawText.length > 18 ? 'scroll' : 'static';

  if (textMode === 'scroll') {
    var scrollOff = typeof offset === 'number' ? offset : 0;
    var padded = rawText + '   ' + rawText;
    var maxLen = 20;
    if (padded.length > maxLen) {
      var start = scrollOff % (rawText.length + 3);
      rawText = padded.substr(start, maxLen);
    }
  } else if (textMode === 'animate') {
    var tick = typeof animateTick === 'number' ? animateTick : 0;
    if (tick % 2 === 1) {
      if (style.id === 'number') {
        rawText = '--> ' + shortPct + ' <--';
      } else {
        var invFillCount = Math.round(BAR_CHAR_COUNT * (100 - pct) / 100);
        var invEmptyCount = BAR_CHAR_COUNT - invFillCount;
        var invBar = '';
        for (var i = 0; i < invFillCount; i++) invBar += style.emptyChar;
        for (var i = 0; i < invEmptyCount; i++) invBar += style.fillChar;
        rawText = invBar + ' ' + shortPct;
      }
    }
  }

  return { text: rawText, label: 'Goal' };
}

function buildPanelTable(row1_label, row1_value, progressText, row3_label, row3_value, theme) {
  if (!theme) theme = getProgressTheme();
  return {
    template: 'image_template',
    table: {
      row_1: {
        'background-color': theme.track,
        col_1: { value: row1_label, color: theme.label, 'font-weight': 'bold' },
        col_2: { value: row1_value, color: theme.accent, 'text-align': 'right' },
      },
      row_2: {
        'background-color': theme.track,
        col_1: { value: progressText, color: theme.fill, 'text-align': 'center', 'font-weight': 'bold' },
      },
      row_3: {
        'background-color': theme.track,
        col_1: { value: row3_label, color: theme.label, 'font-weight': 'bold' },
        col_2: { value: row3_value, color: theme.accent, 'text-align': 'right' },
      },
    },
  };
}

function getNextScrollOffset() {
  var off = $kv.get(PROGRESS_SCROLL_KEY, 0);
  // Wrap at 1000 to prevent unbounded KV growth (mirrors shared.js)
  $kv.set(PROGRESS_SCROLL_KEY, (off + 1) % 1000);
  return off;
}

function getNextAnimateTick() {
  var tick = $kv.get(PROGRESS_ANIMATE_KEY, 0);
  // Wrap at 100 to prevent unbounded KV growth (mirrors shared.js)
  $kv.set(PROGRESS_ANIMATE_KEY, (tick + 1) % 100);
  return tick;
}

function resetScrollOffset() {
  $kv.set(PROGRESS_SCROLL_KEY, 0);
}

function resetAnimateTick() {
  $kv.set(PROGRESS_ANIMATE_KEY, 0);
}

// ============================================================
// Rolling Chat Notification Utilities (mirrored from shared.js)
// ============================================================

var NOTIFICATION_DEFAULTS = [
  { type: 'tip',        template: '{{username}} just tipped {{amount}} tokens! \u2726',                    weight: 10 },
  { type: 'tip',        template: 'Thank you {{username}} for the {{amount}} token tip! \u2661',           weight: 8 },
  { type: 'follow',     template: 'New follower: {{username}}! \u2726 Welcome to the show!',               weight: 10 },
  { type: 'follow',     template: '{{username}} just followed! Give them a warm welcome!',                 weight: 8 },
  { type: 'enter',      template: '{{username}} has entered the room! \u2192',                             weight: 8 },
  { type: 'enter',      template: 'Welcome back, {{username}}! Good to see you!',                          weight: 6 },
  { type: 'fanclub',    template: '{{username}} joined the fanclub! Thank you so much! \u2726',            weight: 8 },
  { type: 'fanclub',    template: '{{username}} renewed their fanclub membership! \u2661',                 weight: 6 },
  { type: 'media',      template: '{{username}} purchased {{item}}! Thank you for your support!',          weight: 8 },
  { type: 'goal',       template: '\u2726 Goal reached! {{amount}} tokens total! Thank you everyone!',     weight: 5 },
  { type: 'prompt',     template: 'Don\'t forget to check !menu',                                          weight: 8 },
  { type: 'prompt',     template: 'Having fun? !menu has all the options',                                 weight: 7 },
  { type: 'prompt',     template: 'Feeling generous? Every tip is appreciated! \u2726',                   weight: 6 },
  { type: 'prompt',     template: 'Looking for something? Check !menu',                                    weight: 7 },
  { type: 'prompt',     template: 'New here? !commands for everything available',                          weight: 7 },
  { type: 'prompt',     template: 'Your support makes the show better! \u2661',                            weight: 5 },
  { type: 'prompt',     template: 'Loving the vibes tonight! \u2726',                                      weight: 6 },
  { type: 'prompt',     template: 'Don\'t be shy - say hi! !commands for options',                         weight: 6 },
  { type: 'prompt',     template: 'Curious about discounts? !discounts to check',                          weight: 5 },
  { type: 'prompt',     template: 'Stay tuned - more fun coming up! \u2726',                               weight: 5 },
];

var REPLY_TEMPLATES = [
  { type: 'request',    template: 'Don\'t forget to check !menu', weight: 10 },
  { type: 'request',    template: '!menu has everything listed with prices', weight: 9 },
  { type: 'request',    template: 'There\'s an option for that in !menu', weight: 9 },
  { type: 'request',    template: 'Check !menu for all available options', weight: 8 },
  { type: 'request',    template: 'All options are listed in !menu', weight: 7 },
  { type: 'greeting',   template: 'Hi {{username}} -- check !menu and !commands', weight: 10 },
  { type: 'greeting',   template: 'Welcome {{username}} -- !menu for options', weight: 9 },
  { type: 'compliment', template: 'Thank you, {{username}} -- check !menu', weight: 10 },
  { type: 'compliment', template: 'Glad you\'re enjoying the show, {{username}} -- !menu has options', weight: 8 },
  { type: 'question',   template: 'Most features are listed in !commands', weight: 10 },
  { type: 'question',   template: 'Check !menu for available options with prices', weight: 9 },
  { type: 'excitement', template: 'So glad you\'re enjoying it! Check !menu for more', weight: 10 },
  { type: 'excitement', template: 'More options in !menu', weight: 9 },
  { type: 'farewell',   template: 'See you, {{username}} -- !menu when you return', weight: 10 },
  { type: 'farewell',   template: 'Bye {{username}} -- !menu', weight: 9 },
];

function resolveTemplate(tpl, vars) {
  var result = tpl;
  if (vars.username) result = result.replace(/\{\{username\}\}/g, vars.username);
  if (vars.amount) result = result.replace(/\{\{amount\}\}/g, vars.amount);
  if (vars.item) result = result.replace(/\{\{item\}\}/g, vars.item);
  if (vars.count) result = result.replace(/\{\{count\}\}/g, vars.count);
  return result;
}

function getNotificationPool() {
  var raw = $settings.notificationTemplates;
  if (raw && Array.isArray(raw) && raw.length > 0) {
    return raw.map(function(e) {
      return { type: String(e[0] || '').trim(), template: String(e[1] || '').trim(), weight: Number(e[2]) || 1 };
    }).filter(function(e) { return e.type && e.template; });
  }
  return NOTIFICATION_DEFAULTS;
}

function getReplyPool() {
  var raw = $settings.notificationReplyTemplates;
  if (raw && Array.isArray(raw) && raw.length > 0) {
    return raw.map(function(e) {
      return { type: String(e[0] || '').trim(), template: String(e[1] || '').trim(), weight: Number(e[2]) || 1 };
    }).filter(function(e) { return e.type && e.template; });
  }
  return REPLY_TEMPLATES;
}

function weightedPick(pool) {
  if (!pool || pool.length === 0) return null;
  var totalWeight = 0, i;
  for (i = 0; i < pool.length; i++) totalWeight += pool[i].weight;
  var r = Math.random() * totalWeight;
  var accum = 0;
  for (i = 0; i < pool.length; i++) {
    accum += pool[i].weight;
    if (r < accum) return pool[i];
  }
  return pool[pool.length - 1];
}

function pickRandomNotification(vars) {
  var pool = getNotificationPool();
  var picked = weightedPick(pool);
  if (!picked) return null;
  return { text: resolveTemplate(picked.template, vars || {}), type: picked.type };
}

function pickContextualNotification(messageType, vars) {
  var pool = getReplyPool();
  var filtered = pool.filter(function(e) { return e.type === messageType; });
  if (filtered.length === 0) filtered = pool;
  var picked = weightedPick(filtered);
  if (!picked) return null;
  return { text: resolveTemplate(picked.template, vars || {}), type: picked.type };
}

function addNotification(text, type, mode, toUsername) {
  var maxEntries = $settings.notificationCount || 5;
  var queue = $kv.get(NOTIFICATION_QUEUE_KEY, []);
  var entry = { text: text, type: type || 'general', timestamp: Date.now(), mode: mode || 'auto' };
  if (toUsername) entry.toUsername = toUsername;
  queue.push(entry);
  while (queue.length > maxEntries) queue.shift();
  $kv.set(NOTIFICATION_QUEUE_KEY, queue);
  var logTarget = toUsername ? ('->' + toUsername) : '';
  console.log('[NOTIF ' + (mode || 'auto') + '] [' + (type || 'general') + '] ' + text + ' ' + logTarget);
  if (toUsername) {
    $room.sendNotice(text, { toUsername: toUsername });
  } else {
    $room.sendNotice(text);
  }
  if ($settings.overlayNotificationsEnabled !== false) {
    $overlay.emit('notification', { text: text, type: type || 'general', mode: mode || 'auto', queue: queue });
  }
}

function getNotificationQueue() {
  return $kv.get(NOTIFICATION_QUEUE_KEY, []);
}

function clearNotificationQueue() {
  $kv.set(NOTIFICATION_QUEUE_KEY, []);
}

function startNotificationAutoTimer() {
  var min = Math.max(Number($settings.notificationAutoIntervalMin) || 60, 60);
  var max = Math.max(Number($settings.notificationAutoIntervalMax) || 300, 60);
  if (max < min) max = min;
  var interval = Math.floor(Math.random() * (max - min + 1)) + min;
  stopNotificationAutoTimer();
  $callback.create(NOTIFICATION_AUTO_LABEL, interval, false);
  console.log('[NOTIF_AUTO] Timer started with ' + interval + 's interval');
}

function stopNotificationAutoTimer() {
  $callback.cancel(NOTIFICATION_AUTO_LABEL);
}

function doNotificationAutoTick() {
  var notif = pickRandomNotification({ username: $room.owner || 'there' });
  if (notif) addNotification(notif.text, notif.type, 'auto');
  startNotificationAutoTimer();
}

function detectMessageType(body) {
  var lower = (body || '').toLowerCase().trim();
  if (!lower) return null;
  if (/^(hi+|hell+o+|hey+|yoo?|sup|howdy|good (morning|evening|afternoon|day))\b/i.test(lower)) return 'greeting';
  if (/^(bye+|goodbye+|gotta go|see you|talk to you|later|cya|leave)/i.test(lower)) return 'farewell';
  if (/\b(cute|pretty|hot|sexy|beautiful|gorgeous|stunning|lovely|adorable|handsome|beauty|hottie)\b/i.test(lower)) return 'compliment';
  if (/\?/.test(lower) || /^(what|how|why|when|where|who|can |could |would |will |do you|are you|is it|is there)/i.test(lower)) return 'question';
  if (/\b(please |can you|could you|will you|would you|show |dance |sing |play |do a |do the|maybe |wish |want )/i.test(lower)) return 'request';
  if (/\b(dance|sing|song|show|play|perform|tip |flash|smile|talk|role|game)\b/i.test(lower) && /\?/.test(lower)) return 'request';
  if (/\b(wow|nice|amazing|incredible|awesome|fantastic|great|love it|perfect|goddess|queen|best|fire|lit)\b/i.test(lower)) return 'excitement';
  return null;
}

function resetNotificationSettings() {
  $settings.notificationCount = 5;
  $settings.notificationAutoEnabled = true;
  $settings.notificationAutoIntervalMin = 60;
  $settings.notificationAutoIntervalMax = 300;
  $settings.notificationReplyEnabled = true;
  $settings.overlayNotificationsEnabled = false;
  // Reset templates to built-in defaults (mirrors shared.js)
  var defaults = NOTIFICATION_DEFAULTS.map(function(e) {
    return [e.type, e.template, String(e.weight)];
  });
  $settings.notificationTemplates = defaults;
  // Reset reply templates
  var replyDefaults = REPLY_TEMPLATES.map(function(e) {
    return [e.type, e.template, String(e.weight)];
  });
  $settings.notificationReplyTemplates = replyDefaults;
  clearNotificationQueue();
  stopNotificationAutoTimer();
  if ($settings.notificationAutoEnabled) startNotificationAutoTimer();
  notifyBroadcaster('Notification settings reset to defaults.');
}

// ============================================================
// Handlers
// ============================================================

var onAppStart, onAppStop, onUserEnter, onUserLeave, onUserFollow, onUserUnfollow;
var onChatMessage, onChatMessageTransform, onTipReceived, onTipDialogOpen;
var onBroadcastPanelUpdate, onFanclubJoin, onMediaPurchase, onCallback, onShortcut;
var onRoomStatusChange, onBroadcastStart, onBroadcastStop, onAppSettingsChange;

function loadHandlers() {
  onAppStart = function () {
    if ($kv.get(TIP_GOAL_KEY) === undefined) $kv.set(TIP_GOAL_KEY, 1000);
    if ($kv.get(SESSION_TIPS_KEY) === undefined) $kv.set(SESSION_TIPS_KEY, 0);
    if ($kv.get(NEW_FOLLOWERS_KEY) === undefined) $kv.set(NEW_FOLLOWERS_KEY, 0);
    $kv.set(SESSION_START_KEY, Date.now());

    if ($kv.get(PROGRESS_THEME_KEY) === undefined) $kv.set(PROGRESS_THEME_KEY, $settings.progressTheme || DEFAULT_PROGRESS_THEME);
    if ($kv.get(PROGRESS_STYLE_KEY) === undefined) $kv.set(PROGRESS_STYLE_KEY, $settings.progressBarStyle || DEFAULT_PROGRESS_STYLE);
    if ($kv.get(PROGRESS_TEXT_MODE_KEY) === undefined) $kv.set(PROGRESS_TEXT_MODE_KEY, $settings.progressTextMode || DEFAULT_TEXT_MODE);
    resetScrollOffset();
    resetAnimateTick();

    $callback.create('tipGoalReminder', 300, true);
    if ($settings.anonConverterEnabled !== false) {
      startAnonConverter();
      startPanelCycle();
    }
    if ($settings.notificationAutoEnabled !== false) {
      startNotificationAutoTimer();
    }
    notifyBroadcaster(APP_NAME + ' v' + APP_VERSION + ' started.');
  };

  onAppStop = function () {
    $callback.cancel('tipGoalReminder');
    if ($kv.get(HIDDEN_CAM_KEY, false)) stopHiddenCam();
    if ($kv.get(ANON_CONVERTER_KEY, false)) stopAnonConverter();
    stopPanelCycle();
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var top = $kv.get(TOP_TIPPER_KEY, 'nobody');
    notifyBroadcaster(APP_NAME + ' stopped. Session tips: ' + tips + ' | Top tipper: ' + top);
    if ($settings.overlayEnabled !== false) $overlay.emit('sessionEnd', { tips: tips, topTipper: top });
  };

  onUserEnter = function () {
    var msgs = [];
    if ($user.isFollower === false) {
      msgs.push('Welcome ' + $user.username + '! If you like me please click follow.');
    } else {
      msgs.push('Welcome back, ' + $user.username + '!');
    }
    if ($user.isMod) {
      $room.sendNotice(':modonduty_cb Moderator ' + $user.username + ' is on duty!',
        { toUsername: $user.username, toColorGroup: 'red' });
    }
    if ($user.inFanclub) msgs.push('Thank you for your fanclub membership!');
    if ($kv.get(HIDDEN_CAM_KEY, false) && $room.status !== 'offline') {
      if (!$limitcam.hasAccess($user.username) && $user.username !== $room.owner) {
        msgs.push('Hidden cam is active! Tip ' + $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE) + '+ tokens for access.');
      }
    }
    if ($kv.get(ANON_CONVERTER_BLACKOUT_KEY, false) && !$limitcam.hasAccess($user.username)) {
      $limitcam.add([$user.username]);
    }
    if (msgs.length > 0) $room.sendNotice(msgs.join(' '), { toUsername: $user.username });
    addNotification($user.username + ' has entered the room.', 'enter', 'event');
  };

  onUserLeave = function () {
    if ($user.isMod) {
      notifyBroadcaster('Moderator ' + $user.username + ' has left the room.', { fontWeight: 'bolder' });
    }
  };

  onUserFollow = function () {
    var count = incrCounter(NEW_FOLLOWERS_KEY, 1);
    $room.sendNotice('Thank you for the follow, ' + $user.username + '!');
    notifyBroadcaster(count + ' new follower' + (count === 1 ? '' : 's') + ' this broadcast.');
    addNotification($user.username + ' just followed!', 'follow', 'event');
    if ($settings.overlayEnabled !== false) $overlay.emit('follow', { username: $user.username, total: count });
  };

  onUserUnfollow = function () {
    var regulars = $kv.get(REGULAR_USERS_KEY, []);
    var isRegular = regulars.indexOf($user.username) !== -1;
    if (isRegular) notifyBroadcaster('Regular viewer ' + $user.username + ' has unfollowed the room.');
    var idx = regulars.indexOf($user.username);
    if (idx !== -1) { regulars.splice(idx, 1); $kv.set(REGULAR_USERS_KEY, regulars); }
  };

  onChatMessage = function () {
    var body = $message.body || '';
    var parts = body.split(' ');
    var cmd = parts[0];

    if (cmd === '!commands') {
      $room.sendNotice('Commands: !menu, !goal, !hiddencam, !discounts, !anon, !top, !notif', { toUsername: $user.username });
      return;
    }
    if (cmd === '!menu' || cmd === '!tipmenu') {
      $room.sendNotice(formatTipMenuText($user.username), { toUsername: $user.username });
      return;
    }
    if (cmd === '!discounts') {
      $room.sendNotice(formatUserDiscountsText($user.username), { toUsername: $user.username });
      return;
    }
    if (cmd === '!goal') {
      var tips = $kv.get(SESSION_TIPS_KEY, 0);
      var goal = $kv.get(TIP_GOAL_KEY, 1000);
      var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
      var anonTips = $kv.get('anonTipTokens', 0);
      var msg = 'Tip goal: ' + tips + ' / ' + goal + ' (' + pct + '%)';
      if (anonTips > 0) msg += ' | Anon: ' + anonTips;
      $room.sendNotice(msg, { toUsername: $user.username });
      return;
    }
    if (cmd === '!hiddencam') {
      var sub = parts[1];
      var arg = parts.slice(2).join(' ');
      if (!sub) {
        var active = $kv.get(HIDDEN_CAM_KEY, false);
        var excluded = active ? ($room.users.length - (($limitcam.users || []).length)) : 0;
        $room.sendNotice('Hidden cam: ' + (active ? 'ACTIVE' : 'OFF') + (active ? ' (' + excluded + ' users excluded, auto-access at ' + $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE) + '+ tks)' : ''), { toUsername: $user.username });
        return;
      }
      if (sub === 'on' || sub === 'off') {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can toggle hidden cam.', { toUsername: $user.username }); return; }
        var result = toggleHiddenCam();
        $room.sendNotice(result === 'started' ? 'Hidden cam started.' : 'Hidden cam stopped.', { toUsername: $user.username });
        return;
      }
      if (sub === 'add' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can grant access.', { toUsername: $user.username }); return; }
        $limitcam.add([arg]);
        $room.sendNotice(arg + ' has been granted hidden cam access.', { toUsername: $user.username });
        return;
      }
      if (sub === 'remove' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can revoke access.', { toUsername: $user.username }); return; }
        $limitcam.remove([arg]);
        $room.sendNotice(arg + ' hidden cam access removed.', { toUsername: $user.username });
        return;
      }
    }

    if (cmd === '!anon') {
      var sub = parts[1];
      var arg = parts.slice(2).join(' ');

      if (!sub) {
        var active = $kv.get(ANON_CONVERTER_KEY, false);
        var blackout = $kv.get(ANON_CONVERTER_BLACKOUT_KEY, false);
        var interval = $kv.get(ANON_CONVERTER_INTERVAL_KEY, DEFAULT_ANON_INTERVAL);
        var duration = $kv.get(ANON_CONVERTER_DURATION_KEY, DEFAULT_ANON_DURATION);
        var blacklist = getAnonBlacklist();
        var anonTips = $kv.get('anonTipTokens', 0);
        $room.sendNotice(
          'Anon Converter: ' + (active ? 'ON' : 'OFF') +
          (blackout ? ' (BLACKOUT ACTIVE)' : '') +
          ' | Interval: ' + interval + 's | Duration: ' + duration + 's' +
          ' | Anons: ' + $room.anonCount +
          ' | Anon tips: ' + anonTips +
          ' | Blacklisted: ' + blacklist.length,
          { toUsername: $user.username }
        );
        return;
      }

      if (sub === 'on' || sub === 'off') {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can toggle anon converter.', { toUsername: $user.username }); return; }
        if (sub === 'on') { startAnonConverter(); $room.sendNotice('Anonymous User Converter started.', { toUsername: $user.username }); }
        else { stopAnonConverter(); $room.sendNotice('Anonymous User Converter stopped.', { toUsername: $user.username }); }
        return;
      }

      if (sub === 'interval' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can change interval.', { toUsername: $user.username }); return; }
        var sec = parseInt(arg, 10);
        if (sec > 0) { $settings.anonConverterInterval = sec; $kv.set(ANON_CONVERTER_INTERVAL_KEY, sec); $callback.cancel(ANON_CONVERTER_CB_LABEL); $callback.create(ANON_CONVERTER_CB_LABEL, sec, true); $room.sendNotice('Anon converter interval set to ' + sec + 's.', { toUsername: $user.username }); }
        return;
      }

      if (sub === 'duration' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can change duration.', { toUsername: $user.username }); return; }
        var sec = parseInt(arg, 10);
        if (sec > 0) { $settings.anonConverterBlackoutDuration = sec; $kv.set(ANON_CONVERTER_DURATION_KEY, sec); $room.sendNotice('Anon converter blackout duration set to ' + sec + 's.', { toUsername: $user.username }); }
        return;
      }

      if (sub === 'add' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can grant access.', { toUsername: $user.username }); return; }
        $limitcam.add([arg]);
        $room.sendNotice(arg + ' added to anon converter allow-list.', { toUsername: $user.username });
        return;
      }

      if (sub === 'remove' && arg) {
        if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can revoke access.', { toUsername: $user.username }); return; }
        $limitcam.remove([arg]);
        $room.sendNotice(arg + ' removed from anon converter allow-list.', { toUsername: $user.username });
        return;
      }

      if (sub === 'blacklist') {
        var blSub = parts[2];
        var blArg = parts.slice(3).join(' ');
        if (!blSub) {
          var bl = getAnonBlacklist();
          $room.sendNotice('Blacklisted users: ' + (bl.length ? bl.join(', ') : '(none)'), { toUsername: $user.username });
          return;
        }
        if (blSub === 'add' && blArg) {
          if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can manage blacklist.', { toUsername: $user.username }); return; }
          if (addToAnonBlacklist(blArg)) { $room.sendNotice(blArg + ' added to anon blacklist.', { toUsername: $user.username }); }
          else { $room.sendNotice(blArg + ' is already blacklisted.', { toUsername: $user.username }); }
          return;
        }
        if (blSub === 'remove' && blArg) {
          if ($user.username !== $room.owner) { $room.sendNotice('Only the broadcaster can manage blacklist.', { toUsername: $user.username }); return; }
          if (removeFromAnonBlacklist(blArg)) { $room.sendNotice(blArg + ' removed from anon blacklist.', { toUsername: $user.username }); }
          else { $room.sendNotice(blArg + ' was not blacklisted.', { toUsername: $user.username }); }
          return;
        }
      }
    }

    if (cmd === '!top') {
      var count = parts[1] ? parseInt(parts[1], 10) : 5;
      if (isNaN(count) || count < 1) count = 5;
      var top = getTopTippers(count);
      if (top.length === 0) { $room.sendNotice('No tippers yet.', { toUsername: $user.username }); return; }
      var lines = top.map(function(t, i) { return (i + 1) + '. ' + t.username + ' \u2014 ' + t.tokens + ' tokens'; });
      $room.sendNotice('Top ' + top.length + ' tippers: ' + lines.join(' | '), { toUsername: $user.username });
      return;
    }

    if (cmd === '!notif') {
      var sub = parts[1];
      if (!sub) {
        var autoEnabled = $settings.notificationAutoEnabled !== false;
        var autoMin = Math.max(Number($settings.notificationAutoIntervalMin) || 60, 60);
        var autoMax = Math.max(Number($settings.notificationAutoIntervalMax) || 300, 60);
        var replyEnabled = $settings.notificationReplyEnabled !== false;
        var overlayOn = $settings.overlayNotificationsEnabled !== false;
        var queue = getNotificationQueue();
        $room.sendNotice(
          'Notif: Auto ' + (autoEnabled ? 'ON' : 'OFF') + ' (' + autoMin + '-' + autoMax + 's) | ' +
          'Reply ' + (replyEnabled ? 'ON' : 'OFF') + ' | Overlay ' + (overlayOn ? 'ON' : 'OFF') + ' | Queue: ' + queue.length,
          { toUsername: $user.username }
        );
        return;
      }
      if (sub === 'reset') {
        if ($user.username !== $room.owner) {
          $room.sendNotice('Only the broadcaster can reset notification settings.', { toUsername: $user.username });
          return;
        }
        resetNotificationSettings();
        $room.sendNotice('Notification settings reset to defaults.', { toUsername: $user.username });
        return;
      }
    }

    if ($settings.notificationReplyEnabled !== false && $user.username !== $room.owner) {
      var msgType = detectMessageType(body);
      if (msgType) {
        var rateKey = 'notif_reply_' + $user.username;
        var lastReply = $kv.get(rateKey, 0);
        var now = Date.now();
        if (now - lastReply > 30000) {
          $kv.set(rateKey, now);
          var replyNotif = pickContextualNotification(msgType, { username: $user.username });
          if (replyNotif) addNotification(replyNotif.text, replyNotif.type, 'reply', $user.username);
        }
      }
    }
  };

  onChatMessageTransform = function () {
    var banned = $settings.bannedWords || 'freak,darn,shoot';
    var words = banned.split(',').map(function (w) { return w.trim().toLowerCase(); });
    var body = $message.body || '';
    var matched = words.some(function (w) { return w.length > 0 && body.toLowerCase().indexOf(w) !== -1; });
    if (matched) $message.setSpam(true);
    return $message;
  };

  onTipReceived = function () {
    var tips = incrCounter(SESSION_TIPS_KEY, $tip.tokens);
    var goal = $kv.get(TIP_GOAL_KEY, 1000);
    // Non-anonymous announcements handled by addNotification below (avoids duplicate room notices)
    if (!$tip.isAnon) {
      var topTokens = $kv.get(TOP_TIP_TOKENS_KEY, 0);
      var currentUserTips = $kv.get('userTips_' + $user.username, 0);
      currentUserTips += $tip.tokens;
      $kv.set('userTips_' + $user.username, currentUserTips);
      if (currentUserTips > topTokens) { $kv.set(TOP_TIP_TOKENS_KEY, currentUserTips); $kv.set(TOP_TIPPER_KEY, $user.username); }
    } else {
      var anonTips = $kv.get('anonTipTokens', 0);
      anonTips += $tip.tokens;
      $kv.set('anonTipTokens', anonTips);
    }
    if ($kv.get(HIDDEN_CAM_KEY, false) && !$tip.isAnon) {
      var camPrice = $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE);
      if ($tip.tokens >= camPrice && !$limitcam.hasAccess($user.username)) {
        $limitcam.add([$user.username]);
        $room.sendNotice($user.username + ' has been granted hidden cam access!', { toUsername: $user.username });
      }
    }
    // Add rolling notification (single source of public tip announcements)
    var tipText = ($tip.isAnon ? 'Someone' : $user.username)
      + ' just tipped ' + $tip.tokens + ' tokens!';
    if ($tip.message) tipText += ' "' + $tip.message + '"';
    addNotification(tipText, 'tip', 'event');
    if (tips >= goal) {
      $room.sendNotice('Tip goal reached! Total: ' + tips + ' tokens! Thank you everyone!');
      $kv.set(SESSION_TIPS_KEY, 0);
      if ($settings.overlayEnabled !== false) $overlay.emit('goalReached', { total: tips });
    }
    debouncedReloadPanel();
    if ($settings.overlayEnabled !== false) {
      $overlay.emit('tip', {
        username: $tip.isAnon ? 'Anonymous' : $user.username,
        amount: $tip.tokens, message: $tip.message || '', isAnon: $tip.isAnon,
        total: tips, goal: goal,
      });
    }
  };

  onTipDialogOpen = function () {
    var username = $user.username;
    if ($settings.tipMenuEnabled !== false) {
      var menuOptions = buildTipMenuOptions(username);
      if (menuOptions) { $room.setTipOptions(menuOptions); return; }
    }
    var voteOpts = $settings.voteOptions;
    if (voteOpts && Array.isArray(voteOpts) && voteOpts.length > 0) {
      var options = voteOpts.map(function (pair) { return { label: pair[0] + (pair[1] ? ' \u2014 ' + pair[1] : '') }; });
      $room.setTipOptions({ label: 'Vote with your tip:', options: options });
    } else {
      $room.setTipOptions({ label: 'Vote with your tip:', options: [{ label: 'Sing a song' }, { label: 'Do a dance' }, { label: 'Tell a joke' }] });
    }
  };

  onBroadcastPanelUpdate = function () {
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var goal = $kv.get(TIP_GOAL_KEY, 1000);
    var followers = $kv.get(NEW_FOLLOWERS_KEY, 0);
    var topTipper = $kv.get(TOP_TIPPER_KEY, '\u2014');
    var hiddenCam = $kv.get(HIDDEN_CAM_KEY, false);
    var anonActive = $kv.get(ANON_CONVERTER_KEY, false);
    var anonBlackout = $kv.get(ANON_CONVERTER_BLACKOUT_KEY, false);
    var anonInterval = $kv.get(ANON_CONVERTER_INTERVAL_KEY, DEFAULT_ANON_INTERVAL);
    var anonDuration = $settings.anonConverterBlackoutDuration || DEFAULT_ANON_DURATION;
    var blCount = getAnonBlacklist().length;
    var anonCount = $room.anonCount || 0;

    var view = $kv.get(PANEL_VIEW_KEY, 0) % 3;

    var row1_label = 'Tips';
    var row1_value = tips + ' / ' + goal;

    var row3_label, row3_value;

    if (view === 0) {
      if (hiddenCam) {
        var excluded = $room.users.length - (($limitcam.users || []).length);
        row3_label = 'Hidden Cam';
        row3_value = 'ON (' + excluded + ' excl)';
      } else {
        row3_label = 'Top Tipper';
        row3_value = topTipper;
      }
    } else if (view === 1) {
      row3_label = 'Anon Conv.';
      row3_value = (anonActive ? 'ON' : 'OFF') + (anonBlackout ? ' [BLACKOUT]' : '') + ' ' + anonCount + 'a/' + blCount + 'b';
    } else {
      row3_label = followers > 0 ? 'Followers' : 'Cam';
      row3_value = followers > 0 ? '' + followers : (hiddenCam ? 'Hidden' : 'Public');
    }

    var theme = getProgressTheme();
    var style = getProgressStyle();
    var textMode = getProgressTextMode();
    var scrollOff = getNextScrollOffset();
    var animateTick = getNextAnimateTick();
    var bar = buildProgressBarText(tips, goal, style, textMode, scrollOff, animateTick);
    var progressText = bar.label + ': ' + bar.text;

    var panel = buildPanelTable(row1_label, row1_value, progressText, row3_label, row3_value, theme);
    $room.setPanelTemplate(panel);
  };

  onFanclubJoin = function () {
    if ($fanclub.isNew) {
      // Public notice handled by addNotification (single source)
      addNotification($user.username + ' joined the fanclub! Welcome!', 'fanclub', 'event');
    } else {
      notifyBroadcaster($user.username + ' has extended their fanclub membership.', { fontWeight: 'bolder' });
      addNotification($user.username + ' renewed their fanclub membership!', 'fanclub', 'event');
    }
  };

  onMediaPurchase = function () {
    notifyBroadcaster('Media Purchase from ' + $user.username + ': ' +
      $media.name + ' (' + $media.type + ') \u2014 ' + $media.tokens + ' tokens spent.',
      { color: '#FFE0EA' });
    addNotification($user.username + ' purchased ' + $media.name + '!', 'media', 'event');
  };

  onCallback = function () {
    var label = $callback.label;
    if (label === 'tipGoalReminder') {
      var tips = $kv.get(SESSION_TIPS_KEY, 0);
      var goal = $kv.get(TIP_GOAL_KEY, 1000);
      var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
      notifyBroadcaster('Tip goal reminder: ' + tips + ' / ' + goal + ' tokens (' + pct + '%)');
      return;
    }
    if (label === ANON_CONVERTER_CB_LABEL) { doAnonConverterBlackout(); return; }
    if (label === ANON_CONVERTER_RESTORE_LABEL) { doAnonConverterRestore(); return; }
    if (label === PANEL_CYCLE_LABEL) { doPanelCycleTick(); return; }
    if (label === NOTIFICATION_AUTO_LABEL) { doNotificationAutoTick(); return; }
    if (label === PANEL_RELOAD_DEBOUNCE_LABEL) { $room.reloadPanel(); return; }
  };

  onShortcut = function () {
    if ($shortcut.name !== 'tipGoal') return;
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var goal = $kv.get(TIP_GOAL_KEY, 1000);
    var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
    var topTipper = $kv.get(TOP_TIPPER_KEY, '\u2014');
    var hiddenCam = $kv.get(HIDDEN_CAM_KEY, false);
    var parts = ['Tip Goal: ' + tips + ' / ' + goal + ' (' + pct + '%)', 'Top tipper: ' + topTipper];
    if (hiddenCam) {
      var excluded = $room.users.length - (($limitcam.users || []).length);
      parts.push('Hidden cam: ON (' + excluded + ' excluded)');
    }
    $room.sendNotice(parts.join(' | '), { toUsername: $user.username });
  };

  onRoomStatusChange = function () {
    if ($room.status === 'offline') {
      var tips = $kv.get(SESSION_TIPS_KEY, 0);
      var top = $kv.get(TOP_TIPPER_KEY, '\u2014');
      $kv.set('lastSessionTips', tips);
      $kv.set('lastSessionTopTipper', top);
      $kv.set('lastSessionEnd', Date.now());
      if ($kv.get(HIDDEN_CAM_KEY, false)) stopHiddenCam();
      notifyBroadcaster('Room went offline. Session saved: ' + tips + ' tokens, top tipper: ' + top);
      if ($settings.overlayEnabled !== false) $overlay.emit('sessionEnd', { tips: tips, topTipper: top });
    }
    if ($room.status === 'public') {
      $kv.set(SESSION_TIPS_KEY, 0);
      $kv.set(NEW_FOLLOWERS_KEY, 0);
      $kv.set(TOP_TIPPER_KEY, '');
      $kv.set(TOP_TIP_TOKENS_KEY, 0);
      $kv.set(SESSION_START_KEY, Date.now());
      notifyBroadcaster('Room is now public. Session counters have been reset.');
    }
  };

  onBroadcastStart = function () {
    var psMsg = $room.psEnabled
      ? 'Private shows available at ' + $room.psPrice + ' tks/min.'
      : 'Private shows are not enabled.';
    $room.sendNotice('Broadcast is starting! ' + psMsg);
  };

  onBroadcastStop = function () {
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var followers = $kv.get(NEW_FOLLOWERS_KEY, 0);
    var top = $kv.get(TOP_TIPPER_KEY, '\u2014');
    var start = $kv.get(SESSION_START_KEY, Date.now());
    var duration = Math.round((Date.now() - start) / 60000);
    if ($kv.get(HIDDEN_CAM_KEY, false)) stopHiddenCam();
    $room.sendNotice('Broadcast ended after ' + duration + ' min | ' +
      tips + ' tokens | ' + followers + ' new followers | Top: ' + top);
  };

  onAppSettingsChange = function () {
    var newGoal = $settings.tipGoal;
    if (typeof newGoal === 'number' && newGoal > 0) { $kv.set(TIP_GOAL_KEY, newGoal); notifyBroadcaster('Tip goal updated to ' + newGoal + ' tokens.'); }
    var camPrice = $settings.hiddenCamPrice;
    if (typeof camPrice === 'number' && camPrice > 0) $kv.set(HIDDEN_CAM_PRICE_KEY, camPrice);

    var newTheme = $settings.progressTheme;
    if (newTheme && newTheme !== $kv.get(PROGRESS_THEME_KEY)) { $kv.set(PROGRESS_THEME_KEY, newTheme); resetScrollOffset(); resetAnimateTick(); }
    var newStyle = $settings.progressBarStyle;
    if (newStyle && newStyle !== $kv.get(PROGRESS_STYLE_KEY)) { $kv.set(PROGRESS_STYLE_KEY, newStyle); resetScrollOffset(); resetAnimateTick(); }
    var newTextMode = $settings.progressTextMode;
    if (newTextMode && newTextMode !== $kv.get(PROGRESS_TEXT_MODE_KEY)) { $kv.set(PROGRESS_TEXT_MODE_KEY, newTextMode); resetScrollOffset(); resetAnimateTick(); }

    var anonEnabled = $settings.anonConverterEnabled !== false;
    if (anonEnabled && !$kv.get(ANON_CONVERTER_KEY, false)) {
      startAnonConverter();
      startPanelCycle();
    } else if (!anonEnabled && $kv.get(ANON_CONVERTER_KEY, false)) {
      stopAnonConverter();
      stopPanelCycle();
    }

    var notifAutoEnabled = $settings.notificationAutoEnabled !== false;
    if (notifAutoEnabled) {
      startNotificationAutoTimer();
    } else {
      stopNotificationAutoTimer();
    }

    if ($settings.overlayEnabled !== false) $overlay.emit('settingsChange', { tipGoal: newGoal, overlayEnabled: $settings.overlayEnabled });
  };
}

// ============================================================
// Simulation Runner
// ============================================================

function simulateCallback(label) {
  $callback.label = label;
  onCallback();
}

function simulation() {
  console.log('=== Stream Manager Simulator ===\n');
  noticedLog = [];

  // ── Phase 1: Core features (goal, tips, followers) ──

  console.log('--- Event: App Start ---');
  onAppStart();

  console.log('\n--- Event: User Enter (alice, non-follower) ---');
  $user = Object.assign({}, $user, { username: 'alice', isFollower: false, inFanclub: false, isMod: false });
  onUserEnter();

  console.log('\n--- Event: Tip Dialog Open (alice, non-follower) ---');
  $user = Object.assign({}, $user, { username: 'alice', isFollower: false, inFanclub: false });
  onTipDialogOpen();

  console.log('\n--- Event: Tip Received (alice, 50 tokens) ---');
  $user = Object.assign({}, $user, { username: 'alice', isFollower: true });
  $tip = { tokens: 50, message: 'Great stream!', isAnon: false };
  onTipReceived();

  console.log('\n--- Event: Tip Received (bob, 200 tokens, anon) ---');
  $user = Object.assign({}, $user, { username: 'bob', isFollower: true });
  $tip = { tokens: 200, message: '', isAnon: true };
  onTipReceived();

  console.log('\n--- Event: Tip Received (charlie, 30 tokens) ---');
  $user = Object.assign({}, $user, { username: 'charlie', isFollower: true });
  $tip = { tokens: 30, message: 'Nice!', isAnon: false };
  onTipReceived();

  console.log('\n--- Event: User Follow (diana) ---');
  $user = Object.assign({}, $user, { username: 'diana', isFollower: false });
  onUserFollow();

  console.log('\n--- Event: Broadcast Panel Update ---');
  onBroadcastPanelUpdate();

  console.log('\n--- Event: User Leave (mod) ---');
  $user = Object.assign({}, $user, { username: 'eve', isMod: true, isFollower: true });
  onUserLeave();

  // ── Phase 2: Tip Menu & Discounts ──

  console.log('\n=== TIP MENU & DISCOUNTS SCENARIO ===');

  console.log('\n--- !tipmenu (alice, non-fanclub, non-follower) ---');
  $user = Object.assign({}, $user, { username: 'alice', isFollower: false, inFanclub: false, isMod: false });
  $message = { body: '!tipmenu', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!tipmenu' };
  onChatMessage();

  console.log('\n--- !discounts (alice) ---');
  $message = { body: '!discounts', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!discounts' };
  onChatMessage();

  console.log('\n--- !tipmenu (charlie, fanclub member) ---');
  $user = Object.assign({}, $user, { username: 'charlie', isFollower: true, inFanclub: true, isMod: false });
  $message = { body: '!tipmenu', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!tipmenu' };
  onChatMessage();

  console.log('\n--- !discounts (charlie, fanclub) ---');
  $message = { body: '!discounts', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!discounts' };
  onChatMessage();

  console.log('\n--- Tip Dialog Open (charlie, fanclub) ---');
  onTipDialogOpen();

  // ── Phase 2b: Anon Commands & Leaderboard ──

  console.log('\n=== ANON COMMANDS & LEADERBOARD ===');

  console.log('\n--- !anon (status) ---');
  $user = Object.assign({}, $user, { username: 'alice', isFollower: true });
  $message = { body: '!anon', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!anon' };
  onChatMessage();

  console.log('\n--- !anon blacklist add baduser (broadcaster) ---');
  $user = Object.assign({}, $user, { username: $room.owner });
  $message = { body: '!anon blacklist add baduser', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!anon blacklist add baduser' };
  onChatMessage();

  console.log('\n--- !anon blacklist (list) ---');
  $message = { body: '!anon blacklist', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!anon blacklist' };
  onChatMessage();

  console.log('\n--- !top (default 5) ---');
  $user = Object.assign({}, $user, { username: 'alice' });
  $message = { body: '!top', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!top' };
  onChatMessage();

  console.log('\n--- !top 3 ---');
  $message = { body: '!top 3', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!top 3' };
  onChatMessage();

  // ── Phase 3: Hidden Cam ──

  console.log('\n=== HIDDEN CAM SCENARIO ===');

  console.log('\n--- !hiddencam on ---');
  $user = Object.assign({}, $user, { username: $room.owner, isFollower: true, isMod: true });
  $message = { body: '!hiddencam on', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!hiddencam on' };
  onChatMessage();

  console.log('\n--- User Enter (dave, hidden cam active) ---');
  $user = Object.assign({}, $user, { username: 'dave', isFollower: false, inFanclub: false, isMod: false });
  onUserEnter();

  console.log('\n--- !hiddencam (status check) ---');
  $user = Object.assign({}, $user, { username: 'dave', isFollower: true });
  $message = { body: '!hiddencam', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!hiddencam' };
  onChatMessage();

  console.log('\n--- Panel (hidden cam active) ---');
  onBroadcastPanelUpdate();

  console.log('\n--- Tip Received (dave, 120 tokens, auto-grant) ---');
  $tip = { tokens: 120, message: 'For hidden cam!', isAnon: false };
  onTipReceived();

  console.log('\n--- Shortcut (hidden cam active) ---');
  onShortcut();

  console.log('\n--- !hiddencam add bob ---');
  $user = Object.assign({}, $user, { username: $room.owner });
  $message = { body: '!hiddencam add bob', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!hiddencam add bob' };
  onChatMessage();

  console.log('\n--- !hiddencam off ---');
  $message = { body: '!hiddencam off', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!hiddencam off' };
  onChatMessage();

  console.log('\n--- Panel (hidden cam off) ---');
  onBroadcastPanelUpdate();

  // ── Phase 4b: Panel Cycle Test ──

  console.log('\n=== PANEL CYCLE + PROGRESS BAR TEST ===');

  console.log('\n--- Panel (view 0 - Tips / [Progress Bar] / Top Tipper) ---');
  onBroadcastPanelUpdate();

  console.log('\n--- Trigger panel cycle tick (view 1 - Tips / [Progress Bar] / Anon Conv) ---');
  simulateCallback(PANEL_CYCLE_LABEL);
  onBroadcastPanelUpdate();

  console.log('\n--- Trigger panel cycle tick (view 2 - Tips / [Progress Bar] / Cam) ---');
  simulateCallback(PANEL_CYCLE_LABEL);
  onBroadcastPanelUpdate();

  console.log('\n--- Trigger panel cycle tick (view 0 again) ---');
  simulateCallback(PANEL_CYCLE_LABEL);
  onBroadcastPanelUpdate();

  // ── Progress Bar Styles Test ──

  console.log('\n=== PROGRESS BAR STYLES TEST ===');

  var savedTheme = $kv.get(PROGRESS_THEME_KEY);
  var savedStyle = $kv.get(PROGRESS_STYLE_KEY);
  var savedMode = $kv.get(PROGRESS_TEXT_MODE_KEY);

  console.log('\n--- Progress bar style: arrows ---');
  $kv.set(PROGRESS_STYLE_KEY, 'arrow');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar style: circles ---');
  $kv.set(PROGRESS_STYLE_KEY, 'circle');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar style: squares ---');
  $kv.set(PROGRESS_STYLE_KEY, 'square');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar style: percent text ---');
  $kv.set(PROGRESS_STYLE_KEY, 'number');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar theme: ruby-night (dark) ---');
  $kv.set(PROGRESS_STYLE_KEY, 'bar');
  $kv.set(PROGRESS_THEME_KEY, 'ruby-night');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar theme: neon-green ---');
  $kv.set(PROGRESS_THEME_KEY, 'neon-green');
  onBroadcastPanelUpdate();

  console.log('\n--- Progress bar text mode: scroll ---');
  $kv.set(PROGRESS_THEME_KEY, savedTheme);
  $kv.set(PROGRESS_STYLE_KEY, 'bar');
  $kv.set(PROGRESS_TEXT_MODE_KEY, 'scroll');
  // Trigger a few scroll offsets
  for (var si = 0; si < 4; si++) {
    onBroadcastPanelUpdate();
  }

  console.log('\n--- Progress bar text mode: animate ---');
  $kv.set(PROGRESS_TEXT_MODE_KEY, 'animate');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();
  onBroadcastPanelUpdate();

  console.log('\n--- Restore defaults ---');
  $kv.set(PROGRESS_STYLE_KEY, savedStyle || 'bar');
  $kv.set(PROGRESS_THEME_KEY, savedTheme || 'coral-ember');
  $kv.set(PROGRESS_TEXT_MODE_KEY, savedMode || 'auto');
  resetScrollOffset();
  resetAnimateTick();
  onBroadcastPanelUpdate();

  // ── Phase 5: Anon Converter ──

  console.log('\n=== ANONYMOUS USER CONVERTER SCENARIO ===');

  console.log('\n--- Trigger anon converter blackout ---');
  simulateCallback(ANON_CONVERTER_CB_LABEL);

  console.log('\n--- !commands (during blackout) ---');
  $user = Object.assign({}, $user, { username: 'alice' });
  $message = { body: '!commands', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!commands' };
  onChatMessage();

  console.log('\n--- Trigger anon converter restore ---');
  simulateCallback(ANON_CONVERTER_RESTORE_LABEL);

  // ── Phase 5: Notification Tests ──

  console.log('\n=== ROLLING NOTIFICATION TESTS ===');

  console.log('\n--- !notif (status) ---');
  $user = Object.assign({}, $user, { username: 'alice' });
  $message = { body: '!notif', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!notif' };
  onChatMessage();

  console.log('\n--- !notif reset (broadcaster) ---');
  $user = Object.assign({}, $user, { username: $room.owner });
  $message = { body: '!notif reset', bgColor: '', color: '', font: 'Default', isSpam: false, orig: '!notif reset' };
  onChatMessage();

  console.log('\n--- Reply mode: greeting message ---');
  $user = Object.assign({}, $user, { username: 'frank', isFollower: true, isMod: false });
  $message = { body: 'hey everyone!', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'hey everyone!' };
  onChatMessage();

  console.log('\n--- Reply mode: compliment message ---');
  $user = Object.assign({}, $user, { username: 'george', isFollower: true, isMod: false });
  $message = { body: 'you look so cute today!', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'you look so cute today!' };
  onChatMessage();

  console.log('\n--- Reply mode: question ---');
  $user = Object.assign({}, $user, { username: 'helen', isFollower: false, isMod: false });
  $message = { body: 'how are you doing?', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'how are you doing?' };
  onChatMessage();

  console.log('\n--- Reply mode: request (no question mark) ---');
  $user = Object.assign({}, $user, { username: 'ivan', isFollower: true, isMod: false });
  $message = { body: 'show me your dance', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'show me your dance' };
  onChatMessage();

  console.log('\n--- Reply mode: excitement ---');
  $user = Object.assign({}, $user, { username: 'julia', isFollower: false, isMod: false });
  $message = { body: 'wow amazing stream!', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'wow amazing stream!' };
  onChatMessage();

  console.log('\n--- Reply mode: farewell ---');
  $user = Object.assign({}, $user, { username: 'kevin', isFollower: true, isMod: false });
  $message = { body: 'bye gotta go', bgColor: '', color: '', font: 'Default', isSpam: false, orig: 'bye gotta go' };
  onChatMessage();

  console.log('\n--- Event: Fanclub Join (grace, new) ---');
  $user = Object.assign({}, $user, { username: 'grace', isFollower: true, inFanclub: false });
  $fanclub = { isNew: true };
  onFanclubJoin();

  console.log('\n--- Event: Media Purchase (grace) ---');
  $media = { id: 'media_002', name: 'Video Pack #1', tokens: 200, type: 'videos' };
  onMediaPurchase();

  // ── Phase 6: Go offline (cleanup) ──

  console.log('\n=== CLEANUP ===');

  console.log('\n--- Room Status: offline ---');
  var prevStatus = $room.status;
  $room.status = 'offline';
  onRoomStatusChange();
  $room.status = prevStatus;

  console.log('\n--- App Stop ---');
  onAppStop();

  // ── Final KV State ──
  console.log('\n=== Final KV State ===');
  var keys = Array.from(kvStore.keys()).sort();
  keys.forEach(function (k) {
    var v = kvStore.get(k);
    if (typeof v === 'number' && v > 1000000000000) {
      console.log(k + ': ' + new Date(v).toISOString());
    } else {
      console.log(k + ': ' + JSON.stringify(v));
    }
  });

  console.log('\n=== Simulation Complete ===');
  console.log('Total notices sent: ' + noticedLog.length);
}

// ── Boot ──
loadHandlers();
simulation();
