// App Stop — cancel callback, stop hidden cam, log session summary
onAppStop = function () {
  $callback.cancel('tipGoalReminder');

  // Stop hidden cam if active
  if ($kv.get(HIDDEN_CAM_KEY, false)) {
    stopHiddenCam();
  }

  // Stop anon converter if active
  if ($kv.get(ANON_CONVERTER_KEY, false)) {
    stopAnonConverter();
  }

  // Stop panel cycle
  stopPanelCycle();

  var tips = $kv.get(SESSION_TIPS_KEY, 0);
  var top = $kv.get(TOP_TIPPER_KEY, 'nobody');

  notifyBroadcaster(
    APP_NAME + ' stopped. Session tips: ' + tips + ' | Top tipper: ' + top
  );

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('sessionEnd', { tips: tips, topTipper: top });
  }
};
