// Broadcast Start — announce the room is live
onBroadcastStart = function () {
  // Clear goal-reached guard for new broadcast session
  $kv.set(GOAL_REACHED_KEY, false);

  var psMsg = $room.psEnabled
    ? 'Private shows available at ' + $room.psPrice + ' tks/min.'
    : 'Private shows are not enabled.';

  $room.sendNotice(
    'Broadcast is starting! ' + psMsg
  );
};
