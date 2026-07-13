// Broadcast Start — announce the room is live
onBroadcastStart = function () {
  var psMsg = $room.psEnabled
    ? 'Private shows available at ' + $room.psPrice + ' tks/min.'
    : 'Private shows are not enabled.';

  $room.sendNotice(
    'Broadcast is starting! ' + psMsg
  );
};
