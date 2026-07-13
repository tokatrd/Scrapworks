// User Follow — count new followers, emit overlay event
onUserFollow = function () {
  var count = incrCounter(NEW_FOLLOWERS_KEY, 1);

  $room.sendNotice(
    'Thank you for the follow, ' + $user.username + '!'
  );

  notifyBroadcaster(
    count + ' new follower' + (count === 1 ? '' : 's') + ' this broadcast.'
  );

  if ($settings.overlayEnabled !== false) {
    $overlay.emit('follow', {
      username: $user.username,
      total: count,
    });
  }
};
