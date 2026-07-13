// User Leave — alert broadcaster if a moderator leaves
onUserLeave = function () {
  if ($user.isMod) {
    notifyBroadcaster(
      'Moderator ' + $user.username + ' has left the room.',
      { fontWeight: 'bolder' }
    );
  }
};
