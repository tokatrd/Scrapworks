// App Settings Change — sync tip goal, hidden cam price, and progress bar settings
onAppSettingsChange = function () {
  var newGoal = $settings.tipGoal;
  if (typeof newGoal === 'number' && newGoal > 0) {
    $kv.set(TIP_GOAL_KEY, newGoal);
    notifyBroadcaster('Tip goal updated to ' + newGoal + ' tokens.');
  }

  var camPrice = $settings.hiddenCamPrice;
  if (typeof camPrice === 'number' && camPrice > 0) {
    $kv.set(HIDDEN_CAM_PRICE_KEY, camPrice);
  }

  // Sync progress bar settings
  var newTheme = $settings.progressTheme;
  if (newTheme && newTheme !== $kv.get(PROGRESS_THEME_KEY)) {
    $kv.set(PROGRESS_THEME_KEY, newTheme);
    resetScrollOffset();
    resetAnimateTick();
  }

  var newStyle = $settings.progressBarStyle;
  if (newStyle && newStyle !== $kv.get(PROGRESS_STYLE_KEY)) {
    $kv.set(PROGRESS_STYLE_KEY, newStyle);
    resetScrollOffset();
    resetAnimateTick();
  }

  var newTextMode = $settings.progressTextMode;
  if (newTextMode && newTextMode !== $kv.get(PROGRESS_TEXT_MODE_KEY)) {
    $kv.set(PROGRESS_TEXT_MODE_KEY, newTextMode);
    resetScrollOffset();
    resetAnimateTick();
  }

  // Handle anon converter enabled/disabled
  var anonEnabled = $settings.anonConverterEnabled !== false;
  var wasEnabled = $kv.get(ANON_CONVERTER_KEY, false);
  if (anonEnabled && !wasEnabled) {
    startAnonConverter();
    startPanelCycle();
  } else if (!anonEnabled && wasEnabled) {
    stopAnonConverter();
    stopPanelCycle();
  }

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('settingsChange', {
      tipGoal: newGoal,
      overlayEnabled: $settings.overlayEnabled,
    });
  }
};
