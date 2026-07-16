// User Enter — personalised welcome based on user state
onUserEnter = function () {
  var messages = [];

  if ($user.isFollower === false) {
    messages.push(
      'Welcome ' + $user.username + '! If you like me please click follow.'
    );
  } else {
    messages.push('Welcome back, ' + $user.username + '!');
  }

  if ($user.isMod) {
    $room.sendNotice(
      ':modonduty_cb Moderator ' + $user.username + ' is on duty!',
      { toUsername: $user.username, toColorGroup: 'red' }
    );
  }

  if ($user.inFanclub) {
    messages.push('Thank you for your fanclub membership!');
  }

  // Notify about hidden cam if active and user is excluded
  if ($kv.get(HIDDEN_CAM_KEY, false) && $room.status !== 'offline') {
    if (!$limitcam.hasAccess($user.username) && $user.username !== $room.owner) {
      var price = $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE);
      messages.push(
        'Hidden cam is active! Tip ' + price + '+ tokens for access.'
      );
    }
  }

  // If anon converter blackout is active, auto-grant access to this registered user.
  // Anons never trigger onUserEnter (they have no username), so they remain excluded.
  if ($kv.get(ANON_CONVERTER_BLACKOUT_KEY, false) && !$limitcam.hasAccess($user.username)) {
    $limitcam.add([$user.username]);
  }

  if (messages.length > 0) {
    $room.sendNotice(messages.join(' '), { toUsername: $user.username });
  }

  // Add rolling notification for user enter
  addNotification($user.username + ' has entered the room.', 'enter', 'event');
};
