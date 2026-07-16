// Broadcast Panel Update — 3-row live stats panel with permanent tip goal progress bar
// Row 2 (middle line) always shows a themed progress bar.
// Rows 1 & 3 cycle through 3 views (same cycle as before).
// Uses image_template + table for color/styling support.
// REQUIRES: Broadcast panel permission
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

  // Row 1 is always "Tips: <amount> / <goal>"
  var row1_label = 'Tips';
  var row1_value = tips + ' / ' + goal;

  // Row 3 varies by view
  var row3_label, row3_value;

  if (view === 0) {
    // View 0: Tips / [Progress Bar] / Top Tipper
    if (hiddenCam) {
      var excluded = $room.users.length - $limitcam.users.length;
      row3_label = 'Hidden Cam';
      row3_value = 'ON (' + excluded + ' excl)';
    } else {
      row3_label = 'Top Tipper';
      row3_value = topTipper;
    }
  } else if (view === 1) {
    // View 1: Tips / [Progress Bar] / Anon Conv + counts
    row3_label = 'Anon Conv.';
    row3_value = (anonActive ? 'ON' : 'OFF') + (anonBlackout ? ' [BLACKOUT]' : '') + ' ' + anonCount + 'a/' + blCount + 'b';
  } else {
    // View 2: Tips / [Progress Bar] / Cam
    row3_label = followers > 0 ? 'Followers' : 'Cam';
    row3_value = followers > 0 ? '' + followers : (hiddenCam ? 'Hidden' : 'Public');
  }

  // ── Build the progress bar ──
  var theme = getProgressTheme();
  var style = getProgressStyle();
  var textMode = getProgressTextMode();
  var scrollOff = getNextScrollOffset();
  var animateTick = getNextAnimateTick();
  var bar = buildProgressBarText(tips, goal, style, textMode, scrollOff, animateTick);
  var progressText = bar.label + ': ' + bar.text;

  // ── Render panel ──
  var panel = buildPanelTable(row1_label, row1_value, progressText, row3_label, row3_value, theme);
  $room.setPanelTemplate(panel);
};
