// Room Status Change — save/reset session state on status transitions
onRoomStatusChange = function () {
  if ($room.status === 'offline') {
    // Save final session stats before reset
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var top = $kv.get(TOP_TIPPER_KEY, '—');
    $kv.set('lastSessionTips', tips);
    $kv.set('lastSessionTopTipper', top);
    $kv.set('lastSessionEnd', Date.now());

    // Stop hidden cam when room goes offline
    if ($kv.get(HIDDEN_CAM_KEY, false)) {
      stopHiddenCam();
    }

    notifyBroadcaster(
      'Room went offline. Session saved: ' + tips + ' tokens, top tipper: ' + top
    );

    if ($settings.overlayEnabled !== false) {
      $overlay.emit('sessionEnd', { tips: tips, topTipper: top });
    }
  }

  if ($room.status === 'public') {
    // Reset session counters for a new public show
    $kv.set(SESSION_TIPS_KEY, 0);
    $kv.set(NEW_FOLLOWERS_KEY, 0);
    $kv.set(TOP_TIPPER_KEY, '');
    $kv.set(TOP_TIP_TOKENS_KEY, 0);
    $kv.set(SESSION_START_KEY, Date.now());

    notifyBroadcaster(
      'Room is now public. Session counters have been reset.'
    );
  }
};
