// App Start — initialise KV defaults, set up repeating callbacks, start anon converter
onAppStart = function () {
  if ($kv.get(TIP_GOAL_KEY) === undefined) $kv.set(TIP_GOAL_KEY, 1000);
  if ($kv.get(SESSION_TIPS_KEY) === undefined) $kv.set(SESSION_TIPS_KEY, 0);
  if ($kv.get(NEW_FOLLOWERS_KEY) === undefined) $kv.set(NEW_FOLLOWERS_KEY, 0);
  $kv.set(SESSION_START_KEY, Date.now());

  // Init progress bar defaults
  if ($kv.get(PROGRESS_THEME_KEY) === undefined) $kv.set(PROGRESS_THEME_KEY, $settings.progressTheme || DEFAULT_PROGRESS_THEME);
  if ($kv.get(PROGRESS_STYLE_KEY) === undefined) $kv.set(PROGRESS_STYLE_KEY, $settings.progressBarStyle || DEFAULT_PROGRESS_STYLE);
  if ($kv.get(PROGRESS_TEXT_MODE_KEY) === undefined) $kv.set(PROGRESS_TEXT_MODE_KEY, $settings.progressTextMode || DEFAULT_TEXT_MODE);
  resetScrollOffset();
  resetAnimateTick();

  // 5-minute repeating callback to nudge the broadcaster
  $callback.create('tipGoalReminder', 300, true);

  // Start anon converter if enabled
  if ($settings.anonConverterEnabled !== false) {
    startAnonConverter();
    startPanelCycle();
  }

  notifyBroadcaster(APP_NAME + ' v' + APP_VERSION + ' started.');
};
