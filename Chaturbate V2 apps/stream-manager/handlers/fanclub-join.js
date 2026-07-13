// Fanclub Join — welcome new or renewing members
onFanclubJoin = function () {
  if ($fanclub.isNew) {
    $room.sendNotice(
      $user.username + ' has joined the fanclub! Welcome!'
    );
  } else {
    notifyBroadcaster(
      $user.username + ' has extended their fanclub membership.',
      { fontWeight: 'bolder' }
    );
  }
};
