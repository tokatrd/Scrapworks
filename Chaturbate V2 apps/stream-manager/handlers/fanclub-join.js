// Fanclub Join — welcome new or renewing members
onFanclubJoin = function () {
  if ($fanclub.isNew) {
    // Public notice handled by addNotification (single source)
    addNotification($user.username + ' joined the fanclub! Welcome!', 'fanclub', 'event');
  } else {
    notifyBroadcaster(
      $user.username + ' has extended their fanclub membership.',
      { fontWeight: 'bolder' }
    );
    addNotification($user.username + ' renewed their fanclub membership!', 'fanclub', 'event');
  }
};
