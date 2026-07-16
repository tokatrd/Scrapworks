// Media Purchase — alert broadcaster with purchase details
onMediaPurchase = function () {
  notifyBroadcaster(
    'Media Purchase from ' + $user.username + ': ' +
    $media.name + ' (' + $media.type + ') — ' +
    $media.tokens + ' tokens spent.',
    { color: '#FFE0EA' }
  );
  addNotification(
    $user.username + ' purchased ' + $media.name + '!',
    'media',
    'event'
  );
};
