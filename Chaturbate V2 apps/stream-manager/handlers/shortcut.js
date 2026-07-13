// Shortcut — tip goal progress + hidden cam status to the clicking user
onShortcut = function () {
  if ($shortcut.name !== 'tipGoal') {
    return;
  }

  var tips = $kv.get(SESSION_TIPS_KEY, 0);
  var goal = $kv.get(TIP_GOAL_KEY, 1000);
  var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
  var topTipper = $kv.get(TOP_TIPPER_KEY, '—');
  var hiddenCam = $kv.get(HIDDEN_CAM_KEY, false);
  var parts = [
    'Tip Goal: ' + tips + ' / ' + goal + ' (' + pct + '%)',
    'Top tipper: ' + topTipper,
  ];
  if (hiddenCam) {
    var excluded = $room.users.length - $limitcam.users.length;
    parts.push('Hidden cam: ON (' + excluded + ' excluded)');
  }
  $room.sendNotice(parts.join(' | '), { toUsername: $user.username });
};
