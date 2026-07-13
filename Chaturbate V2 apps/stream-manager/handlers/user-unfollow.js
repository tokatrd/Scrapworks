// User Unfollow — track regulars and alert broadcaster
onUserUnfollow = function () {
  var regulars = $kv.get(REGULAR_USERS_KEY, []);
  var isRegular = regulars.indexOf($user.username) !== -1;

  if (isRegular) {
    notifyBroadcaster(
      'Regular viewer ' + $user.username + ' has unfollowed the room.'
    );
  }

  // Remove from regulars list if present
  var idx = regulars.indexOf($user.username);
  if (idx !== -1) {
    regulars.splice(idx, 1);
    $kv.set(REGULAR_USERS_KEY, regulars);
  }
};
