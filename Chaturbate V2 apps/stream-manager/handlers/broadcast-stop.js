// Broadcast Stop — announce end with session stats, stop hidden cam
onBroadcastStop = function () {
  var tips = $kv.get(SESSION_TIPS_KEY, 0);
  var followers = $kv.get(NEW_FOLLOWERS_KEY, 0);
  var top = $kv.get(TOP_TIPPER_KEY, '—');
  var start = $kv.get(SESSION_START_KEY, Date.now());
  var duration = Math.round((Date.now() - start) / 60000);

  if ($kv.get(HIDDEN_CAM_KEY, false)) {
    stopHiddenCam();
  }

  $room.sendNotice(
    'Broadcast ended after ' + duration + ' min | ' +
    tips + ' tokens | ' + followers + ' new followers | Top: ' + top
  );
};
