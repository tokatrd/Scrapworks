// Callback — tip goal reminder + anon converter blackout/restore
onCallback = function () {
  var label = $callback.label;

  // Tip goal reminder (5-minute repeating)
  if (label === 'tipGoalReminder') {
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var goal = $kv.get(TIP_GOAL_KEY, 1000);
    var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
    notifyBroadcaster(
      'Tip goal reminder: ' + tips + ' / ' + goal + ' tokens (' + pct + '%)'
    );
    return;
  }

  // Anon converter: execute blackout
  if (label === ANON_CONVERTER_CB_LABEL) {
    doAnonConverterBlackout();
    return;
  }

  // Anon converter: restore after blackout
  if (label === ANON_CONVERTER_RESTORE_LABEL) {
    doAnonConverterRestore();
    return;
  }

  // Panel cycle tick
  if (label === PANEL_CYCLE_LABEL) {
    doPanelCycleTick();
    return;
  }

  // Notification auto tick
  if (label === NOTIFICATION_AUTO_LABEL) {
    doNotificationAutoTick();
    return;
  }
};
