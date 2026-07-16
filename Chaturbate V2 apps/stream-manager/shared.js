// ============================================================
// Stream Manager — Shared Code
// Runs before every event handler
// ============================================================

const APP_NAME = 'Stream Manager';
const APP_VERSION = '1.0.0';
const SUPPORTED_SHOW_TYPES = ['public', 'private', 'hidden'];

// ── Goal & Session Keys ──
const TOP_TIPPER_KEY = 'topTipper';
const TOP_TIP_TOKENS_KEY = 'topTipTokens';
const SESSION_TIPS_KEY = 'sessionTips';
const TIP_GOAL_KEY = 'tipGoal';
const NEW_FOLLOWERS_KEY = 'newFollowers';
const SESSION_START_KEY = 'sessionStart';
const REGULAR_USERS_KEY = 'regularUsers';

// ── Hidden Cam Keys ──
const HIDDEN_CAM_KEY = 'hiddenCamActive';
const HIDDEN_CAM_PRICE_KEY = 'hiddenCamPrice';
const HIDDEN_CAM_MSG_KEY = 'hiddenCamMessage';
var DEFAULT_HIDDEN_CAM_PRICE = 100;

// ── Anon Converter Keys ──
const ANON_CONVERTER_KEY = 'anonConverterActive';
const ANON_CONVERTER_BLACKOUT_KEY = 'anonConverterBlackedOut';
const ANON_CONVERTER_INTERVAL_KEY = 'anonConverterInterval';
const ANON_CONVERTER_DURATION_KEY = 'anonConverterDuration';
const ANON_CONVERTER_CB_LABEL = 'anonConverterTick';
const ANON_CONVERTER_RESTORE_LABEL = 'anonConverterRestore';
var DEFAULT_ANON_INTERVAL = 180;
var DEFAULT_ANON_DURATION = 15;

// ── Anon Blacklist Keys ──
const ANON_BLACKLIST_KEY = 'anonBlacklist';

// ── Panel Cycle Keys ──
const PANEL_CYCLE_LABEL = 'panelCycleTick';
const PANEL_VIEW_KEY = 'panelCurrentView';

// ── Tip Menu Keys ──
const TIP_MENU_ITEMS_KEY = 'tipMenuItems';

// ============================================================
// Utility: determine which discount groups a user belongs to
// Returns an array of group strings.
// ============================================================
function getUserGroups(username) {
  var groups = ['all'];
  if ($user.isFollower) groups.push('follower');
  if ($user.inFanclub) groups.push('fanclub');
  if ($user.isMod) groups.push('mod');
  // "regular" = has tipped before this session
  var reg = $kv.get(REGULAR_USERS_KEY, []);
  if (reg.indexOf(username) !== -1) groups.push('regular');
  // Specific user override
  groups.push('user:' + username);
  return groups;
}

// ============================================================
// Calculate discounted price for an item given a user's groups.
// discountEntries: array of [group, itemName, discountPct]
// Item "*" means all items. Highest discount wins.
// Returns { price: number, discountPct: number, isDiscounted: bool }
// ============================================================
function getDiscountedPrice(itemName, basePrice, username, discountEntries) {
  var result = { price: basePrice, discountPct: 0, isDiscounted: false };
  if (!discountEntries || !Array.isArray(discountEntries) || discountEntries.length === 0) {
    return result;
  }
  if (!$settings.discountsEnabled) return result;

  var groups = getUserGroups(username);
  var bestDiscount = 0;

  for (var i = 0; i < discountEntries.length; i++) {
    var entry = discountEntries[i];
    var entryGroup = String(entry[0] || '').trim();
    var entryItem = String(entry[1] || '').trim();
    var entryPct = Number(entry[2]) || 0;

    // Check if user belongs to this discount's target group
    var groupMatch = false;
    for (var g = 0; g < groups.length; g++) {
      if (groups[g] === entryGroup) {
        groupMatch = true;
        break;
      }
    }

    if (!groupMatch) continue;

    // Check if this discount applies to the item
    var itemMatch = (entryItem === '*' || entryItem.toLowerCase() === itemName.toLowerCase());
    if (!itemMatch) continue;

    // Take the highest applicable discount
    if (entryPct > bestDiscount) {
      bestDiscount = entryPct;
    }
  }

  if (bestDiscount > 0) {
    var discounted = Math.round(basePrice * (1 - bestDiscount / 100));
    result.price = discounted > 0 ? discounted : 1; // minimum 1 token
    result.discountPct = bestDiscount;
    result.isDiscounted = true;
  }

  return result;
}

// ============================================================
// Build the full tip options array for the tip dialog,
// with discounts applied for the given user.
// Returns the options object for $room.setTipOptions().
// ============================================================
function buildTipMenuOptions(username) {
  var rawItems = $settings.tipMenuItems;
  var rawDiscounts = $settings.discountEntries;

  if (!rawItems || !Array.isArray(rawItems) || rawItems.length === 0) {
    return null;
  }

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
    if (desc) optionLabel += ' — ' + desc;
    if (priceInfo.isDiscounted) {
      optionLabel += ' [' + priceInfo.discountPct + '% off!]';
    }

    options.push({ label: optionLabel });
  }

  if (options.length === 0) return null;

  return { label: label, options: options };
}

// ============================================================
// Format tip menu as a chat message (for !menu / !tipmenu command)
// ============================================================
function formatTipMenuText(username) {
  var rawItems = $settings.tipMenuItems;
  var rawDiscounts = $settings.discountEntries;

  if (!rawItems || !Array.isArray(rawItems) || rawItems.length === 0) {
    return 'No tip menu items configured.';
  }

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
    if (desc) parts[0] += ' — ' + desc;
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
  if (!rawDiscounts || !Array.isArray(rawDiscounts) || rawDiscounts.length === 0) {
    return 'No discounts configured.';
  }
  if (!$settings.discountsEnabled) return 'Discounts are currently disabled.';

  var groups = getUserGroups(username);
  var lines = [];
  lines.push('Your groups: ' + groups.join(', '));
  lines.push('Active discounts:');

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
  if ($room.status === 'offline') {
    notifyBroadcaster('Cannot start hidden cam while offline.');
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
  if ($kv.get(HIDDEN_CAM_KEY, false)) {
    stopHiddenCam();
    return 'stopped';
  }
  return startHiddenCam(message) ? 'started' : 'failed';
}

function noticeExcludedUsers(msg) {
  var allUsers = $room.users || [];
  for (var i = 0; i < allUsers.length; i++) {
    var u = allUsers[i];
    if (u === $room.owner) continue;
    if (!$limitcam.hasAccess(u)) {
      $room.sendNotice(msg, { toUsername: u });
    }
  }
}

// ============================================================
// Anonymous User Converter Utilities
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
  // Restore cam if currently blacked out
  if ($kv.get(ANON_CONVERTER_BLACKOUT_KEY, false)) {
    $limitcam.stop();
    $kv.set(ANON_CONVERTER_BLACKOUT_KEY, false);
  }
  $kv.set(ANON_CONVERTER_KEY, false);
  notifyBroadcaster('Anonymous User Converter stopped.');
}

/**
 * Execute a blackout cycle for anon converter.
 * Called by the repeating callback.
 * Blacks out cam for anons by starting limitcam + adding registered users,
 * then schedules a restore callback.
 */
function doAnonConverterBlackout() {
  if (!$settings.anonConverterEnabled) return;
  if ($room.status === 'offline') return;
  if ($kv.get(HIDDEN_CAM_KEY, false)) return; // hidden cam conflicts

  var msg = $settings.anonConverterMessage || '';
  var link = $settings.anonConverterAffiliateLink || '';
  msg = msg.replace('{{link}}', link);

  var duration = $settings.anonConverterBlackoutDuration || DEFAULT_ANON_DURATION;

  // Start limitcam with registered users pre-loaded in the allow-list.
  // $limitcam.start(message, users) is ATOMIC — registered users NEVER
  // lose feed access, not even for a single frame. Only anons are excluded.
  // Blacklisted registered users are also excluded from the allow-list.
  var users = ($room.users || []).filter(function(u) { return !isBlacklisted(u); });
  $limitcam.start(msg, users);

  $kv.set(ANON_CONVERTER_BLACKOUT_KEY, true);
  notifyBroadcaster('Anon Converter: cam blacked out for ' + duration + 's (' + $room.anonCount + ' anons affected).');

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('anonBlackout', {
      duration: duration,
      anonCount: $room.anonCount,
      message: msg,
      link: link,
    });
  }

  // Schedule the restore
  $callback.create(ANON_CONVERTER_RESTORE_LABEL, duration, false);
}

/**
 * Restore cam feed after a blackout.
 */
function doAnonConverterRestore() {
  if (!$kv.get(ANON_CONVERTER_BLACKOUT_KEY, false)) return;

  $limitcam.stop();
  $kv.set(ANON_CONVERTER_BLACKOUT_KEY, false);

  notifyBroadcaster('Anon Converter: cam feed restored.');

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('anonRestore', {});
  }
}

// ============================================================
// Generic Utilities
// ============================================================

function notifyBroadcaster(message, options) {
  var target = {};
  if ($room && $room.owner) {
    target.toUsername = $room.owner;
  }
  $room.sendNotice(message, Object.assign(target, options || {}));
}

function incrCounter(key, amount) {
  // Use atomic increment to avoid race conditions
  $kv.incr(key, typeof amount === 'number' ? amount : 1);
  return $kv.get(key, 0);
}

// ============================================================
// Progress Bar — Tip Goal Progress Display
// ============================================================

// ── Progress Bar Keys ──
var PROGRESS_STYLE_KEY = 'progressStyle';
var PROGRESS_THEME_KEY = 'progressTheme';
var PROGRESS_TEXT_MODE_KEY = 'progressTextMode';
var PROGRESS_SCROLL_KEY = 'progressScrollOffset';
var PROGRESS_ANIMATE_KEY = 'progressAnimateTick';

var DEFAULT_PROGRESS_STYLE = 'bar';
var DEFAULT_PROGRESS_THEME = 'coral-ember';
var DEFAULT_TEXT_MODE = 'auto';

// ── 20 Pre-Packed Themes (10 White, 10 Black) ──
var PROGRESS_THEMES = [
  // WHITE THEMES (light background, dark text) ──
  { id: 'coral-ember',   name: 'Coral Ember',   type: 'white', fill: '#FF6B6B', track: '#FFF0F0', text: '#333', label: '#888', accent: '#FF6B6B' },
  { id: 'golden-glow',   name: 'Golden Glow',   type: 'white', fill: '#FFB300', track: '#FFF8E0', text: '#333', label: '#888', accent: '#FFB300' },
  { id: 'lime-zest',     name: 'Lime Zest',     type: 'white', fill: '#43A047', track: '#F0FFF0', text: '#333', label: '#888', accent: '#43A047' },
  { id: 'ocean-breeze',  name: 'Ocean Breeze',  type: 'white', fill: '#1E88E5', track: '#E0F4FF', text: '#333', label: '#888', accent: '#1E88E5' },
  { id: 'lavender-mist', name: 'Lavender Mist', type: 'white', fill: '#8E24AA', track: '#F3E8FF', text: '#333', label: '#888', accent: '#8E24AA' },
  { id: 'peach-sorbet',  name: 'Peach Sorbet',  type: 'white', fill: '#FF7043', track: '#FFF0E0', text: '#333', label: '#888', accent: '#FF7043' },
  { id: 'mint-dew',      name: 'Mint Dew',      type: 'white', fill: '#00897B', track: '#E0FFF0', text: '#333', label: '#888', accent: '#00897B' },
  { id: 'rose-quartz',   name: 'Rose Quartz',   type: 'white', fill: '#D81B60', track: '#FFE8F0', text: '#333', label: '#888', accent: '#D81B60' },
  { id: 'sky-blue',      name: 'Sky Blue',      type: 'white', fill: '#0D47A1', track: '#E0F0FF', text: '#333', label: '#888', accent: '#0D47A1' },
  { id: 'silver-gray',   name: 'Silver Gray',   type: 'white', fill: '#546E7A', track: '#F0F0F0', text: '#333', label: '#888', accent: '#546E7A' },

  // BLACK THEMES (dark background, light text) ──
  { id: 'ruby-night',    name: 'Ruby Night',    type: 'black', fill: '#FF1744', track: '#1A0000', text: '#FFF', label: '#888', accent: '#FF1744' },
  { id: 'amber-dark',    name: 'Amber Dark',    type: 'black', fill: '#FF9100', track: '#1A0A00', text: '#FFF', label: '#888', accent: '#FF9100' },
  { id: 'neon-green',    name: 'Neon Green',    type: 'black', fill: '#00E676', track: '#001A05', text: '#FFF', label: '#888', accent: '#00E676' },
  { id: 'cyber-teal',    name: 'Cyber Teal',    type: 'black', fill: '#1DE9B6', track: '#001A18', text: '#FFF', label: '#888', accent: '#1DE9B6' },
  { id: 'deep-cyan',     name: 'Deep Cyan',     type: 'black', fill: '#00BCD4', track: '#001A24', text: '#FFF', label: '#888', accent: '#00BCD4' },
  { id: 'royal-indigo',  name: 'Royal Indigo',  type: 'black', fill: '#536DFE', track: '#08001A', text: '#FFF', label: '#888', accent: '#536DFE' },
  { id: 'ultra-violet',  name: 'Ultra Violet',  type: 'black', fill: '#E040FB', track: '#14001A', text: '#FFF', label: '#888', accent: '#E040FB' },
  { id: 'hot-pink',      name: 'Hot Pink',      type: 'black', fill: '#FF4081', track: '#1A000A', text: '#FFF', label: '#888', accent: '#FF4081' },
  { id: 'electric-yellow', name: 'Electric Yellow', type: 'black', fill: '#FFEA00', track: '#1A1A00', text: '#222', label: '#AAA', accent: '#FFEA00' },
  { id: 'acid-lime',     name: 'Acid Lime',     type: 'black', fill: '#76FF03', track: '#0A1A00', text: '#222', label: '#AAA', accent: '#76FF03' },
];

// ── 5 Progress Bar Visual Styles ──
var BAR_STYLES = [
  { id: 'bar',    name: 'Blocks',   fillChar: '\u2588', emptyChar: '\u2591' },   // █ ░
  { id: 'arrow',  name: 'Arrows',   fillChar: '\u25B6', emptyChar: '\u25B7' },   // ▶ ▷
  { id: 'circle', name: 'Circles',  fillChar: '\u25CF', emptyChar: '\u25CB' },   // ● ○
  { id: 'square', name: 'Squares',  fillChar: '\u25A0', emptyChar: '\u25A1' },   // ■ □
  { id: 'number', name: 'Percent',  fillChar: '',       emptyChar: '' },          // text only
];

var BAR_CHAR_COUNT = 10; // total characters in the bar

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

// ============================================================
// Build the progress bar text string.
// Returns { text, label } where text is the progress bar string,
// and label is a short prefix (e.g. "Goal").
// ============================================================
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

  // Apply text mode
  if (textMode === 'auto') {
    textMode = rawText.length > 18 ? 'scroll' : 'static';
  }

  if (textMode === 'scroll') {
    // Pad and scroll the text
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
      // Alternate frame: show partial inversion (flash the percentage)
      if (style.id === 'number') {
        rawText = '--> ' + shortPct + ' <--';
      } else {
        // Flash the bar by alternating characters
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

// ============================================================
// Build the full panel template object using image_template + table.
// ============================================================
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

// ============================================================
// Track scroll offset and animate tick for progress bar.
// Call these from broadcast-panel-update each time the panel renders.
// ============================================================
function getNextScrollOffset() {
  var off = $kv.get(PROGRESS_SCROLL_KEY, 0);
  // Wrap at 1000 to prevent unbounded KV growth
  $kv.set(PROGRESS_SCROLL_KEY, (off + 1) % 1000);
  return off;
}

function getNextAnimateTick() {
  var tick = $kv.get(PROGRESS_ANIMATE_KEY, 0);
  // Wrap at 100 to prevent unbounded KV growth
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
// Rolling Chat Notifications
// ============================================================

// ── Notification Keys ──
const NOTIFICATION_QUEUE_KEY = 'notificationQueue';
const NOTIFICATION_AUTO_LABEL = 'notificationAutoTick';

// ── 20 Rolling Notification Templates (fallback if settings table is empty) ──
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

// ── Context-Aware Reply Templates ──
// These fire when a user sends a chat message matching a pattern.
// Replies are instructive: they guide users toward proper interaction channels (tip menu, commands, etc.)
var REPLY_TEMPLATES = [
  // Requests → soft redirect
  { type: 'request', template: 'Don\'t forget to check !menu', weight: 10 },
  { type: 'request', template: '!menu has everything listed with prices', weight: 9 },
  { type: 'request', template: 'There\'s an option for that in !menu', weight: 9 },
  { type: 'request', template: 'Check !menu for all available options', weight: 8 },
  { type: 'request', template: 'All options are listed in !menu', weight: 7 },
  // Greetings → welcome + guidance
  { type: 'greeting', template: 'Hi {{username}} -- check !menu and !commands', weight: 10 },
  { type: 'greeting', template: 'Welcome {{username}} -- !menu for options', weight: 9 },
  // Compliments → brief + redirect
  { type: 'compliment', template: 'Thank you, {{username}} -- check !menu', weight: 10 },
  { type: 'compliment', template: 'Glad you\'re enjoying the show, {{username}} -- !menu has options', weight: 8 },
  // Questions → point to commands
  { type: 'question', template: 'Most features are listed in !commands', weight: 10 },
  { type: 'question', template: 'Check !menu for available options with prices', weight: 9 },
  // Excitement → redirect
  { type: 'excitement', template: 'So glad you\'re enjoying it! Check !menu for more', weight: 10 },
  { type: 'excitement', template: 'More options in !menu', weight: 9 },
  // Farewell
  { type: 'farewell', template: 'See you, {{username}} -- !menu when you return', weight: 10 },
  { type: 'farewell', template: 'Bye {{username}} -- !menu', weight: 9 },
];

// ── Helper: resolve template placeholders ──
function resolveTemplate(tpl, vars) {
  var result = tpl;
  if (vars.username) result = result.replace(/\{\{username\}\}/g, vars.username);
  if (vars.amount) result = result.replace(/\{\{amount\}\}/g, vars.amount);
  if (vars.item) result = result.replace(/\{\{item\}\}/g, vars.item);
  if (vars.count) result = result.replace(/\{\{count\}\}/g, vars.count);
  return result;
}

// ── Get merged pool: settings table + built-in defaults ──
function getNotificationPool() {
  var raw = $settings.notificationTemplates;
  if (raw && Array.isArray(raw) && raw.length > 0) {
    return raw.map(function(e) {
      return { type: String(e[0] || '').trim(), template: String(e[1] || '').trim(), weight: Number(e[2]) || 1 };
    }).filter(function(e) { return e.type && e.template; });
  }
  return NOTIFICATION_DEFAULTS;
}

// ── Get reply templates pool ──
function getReplyPool() {
  var raw = $settings.notificationReplyTemplates;
  if (raw && Array.isArray(raw) && raw.length > 0) {
    return raw.map(function(e) {
      return { type: String(e[0] || '').trim(), template: String(e[1] || '').trim(), weight: Number(e[2]) || 1 };
    }).filter(function(e) { return e.type && e.template; });
  }
  return REPLY_TEMPLATES;
}

// ── Weighted random pick from pool ──
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

// ── Pick a random notification from the rolling pool ──
function pickRandomNotification(vars) {
  var pool = getNotificationPool();
  var picked = weightedPick(pool);
  if (!picked) return null;
  return {
    text: resolveTemplate(picked.template, vars || {}),
    type: picked.type,
  };
}

// ── Pick a contextually-matched reply from the REPLY pool ──
function pickContextualNotification(messageType, vars) {
  var pool = getReplyPool();
  // Filter pool to matching type, fall back to full reply pool if none match
  var filtered = pool.filter(function(e) { return e.type === messageType; });
  if (filtered.length === 0) filtered = pool;
  var picked = weightedPick(filtered);
  if (!picked) return null;
  return {
    text: resolveTemplate(picked.template, vars || {}),
    type: picked.type,
  };
}

// ── Add notification to rolling queue ──
function addNotification(text, type, mode, toUsername) {
  var maxEntries = $settings.notificationCount || 5;
  var queue = $kv.get(NOTIFICATION_QUEUE_KEY, []);
  var entry = { text: text, type: type || 'general', timestamp: Date.now(), mode: mode || 'auto' };
  if (toUsername) entry.toUsername = toUsername;
  queue.push(entry);
  // Keep only the last maxEntries
  while (queue.length > maxEntries) queue.shift();
  $kv.set(NOTIFICATION_QUEUE_KEY, queue);

  // Emit to overlay if enabled
  if ($settings.overlayNotificationsEnabled !== false) {
    $overlay.emit('notification', {
      text: text,
      type: type || 'general',
      mode: mode || 'auto',
      queue: queue,
    });
  }

  // Send as a chat notice — privately for replies, publicly for events/auto
  if (toUsername) {
    $room.sendNotice(text, { toUsername: toUsername });
  } else {
    $room.sendNotice(text);
  }
}

// ── Get current notification queue ──
function getNotificationQueue() {
  return $kv.get(NOTIFICATION_QUEUE_KEY, []);
}

// ── Clear notification queue ──
function clearNotificationQueue() {
  $kv.set(NOTIFICATION_QUEUE_KEY, []);
}

// ── Start auto-mode timer with random interval between min/max ──
function startNotificationAutoTimer() {
  var min = Math.max(Number($settings.notificationAutoIntervalMin) || 60, 60);
  var max = Math.max(Number($settings.notificationAutoIntervalMax) || 300, 60);
  if (max < min) max = min;
  var interval = Math.floor(Math.random() * (max - min + 1)) + min;
  // Cancel any existing auto timer first
  stopNotificationAutoTimer();
  $callback.create(NOTIFICATION_AUTO_LABEL, interval, false);
}

// ── Stop auto-mode timer ──
function stopNotificationAutoTimer() {
  $callback.cancel(NOTIFICATION_AUTO_LABEL);
}

// ── Execute one auto-mode notification tick ──
function doNotificationAutoTick() {
  var notif = pickRandomNotification({ username: $room.owner || 'there' });
  if (notif) {
    addNotification(notif.text, notif.type, 'auto');
  }
  // Re-schedule next tick with new random interval
  startNotificationAutoTimer();
}

// ── Detect message type from chat text for reply-mode ──
function detectMessageType(body) {
  var lower = (body || '').toLowerCase().trim();
  if (!lower) return null;
  // Greetings
  if (/^(hi+|hell+o+|hey+|yoo?|sup|howdy|good (morning|evening|afternoon|day))\b/i.test(lower)) return 'greeting';
  // Farewells
  if (/^(bye+|goodbye+|gotta go|see you|talk to you|later|cya|leave)/i.test(lower)) return 'farewell';
  // Compliments
  if (/\b(cute|pretty|hot|sexy|beautiful|gorgeous|stunning|lovely|adorable|handsome|beauty|hottie)\b/i.test(lower)) return 'compliment';
  // Questions
  if (/\?/.test(lower) || /^(what|how|why|when|where|who|can |could |would |will |do you|are you|is it|is there)/i.test(lower)) return 'question';
  // Requests (no ? needed for explicit request language)
  if (/\b(please |can you|could you|will you|would you|show |dance |sing |play |do a |do the|maybe |wish |want )/i.test(lower)) return 'request';
  // Requests that need ? to disambiguate from general chat
  if (/\b(dance|sing|song|show|play|perform|tip |flash|smile|talk|role|game)\b/i.test(lower) && /\?/.test(lower)) return 'request';
  // Excitement
  if (/\b(wow|nice|amazing|incredible|awesome|fantastic|great|love it|perfect|goddess|queen|best|fire|lit)\b/i.test(lower)) return 'excitement';
  return null;
}

// ── Reset all notification settings to schema defaults ──
function resetNotificationSettings() {
  $settings.notificationCount = 5;
  $settings.notificationAutoEnabled = true;
  $settings.notificationAutoIntervalMin = 60;
  $settings.notificationAutoIntervalMax = 300;
  $settings.notificationReplyEnabled = true;
  $settings.overlayNotificationsEnabled = false;
  // Reset templates to built-in defaults
  var defaults = NOTIFICATION_DEFAULTS.map(function(e) {
    return [e.type, e.template, String(e.weight)];
  });
  $settings.notificationTemplates = defaults;
  // Reset reply templates
  var replyDefaults = REPLY_TEMPLATES.map(function(e) {
    return [e.type, e.template, String(e.weight)];
  });
  $settings.notificationReplyTemplates = replyDefaults;
  // Clear queue
  clearNotificationQueue();
  // Restart or stop auto timer
  stopNotificationAutoTimer();
  if ($settings.notificationAutoEnabled) {
    startNotificationAutoTimer();
  }
  notifyBroadcaster('Notification settings reset to defaults.');
}
