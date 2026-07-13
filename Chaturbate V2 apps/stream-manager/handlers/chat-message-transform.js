// Chat Message Transform — filter banned words
// REQUIRES: Transform messages permission
onChatMessageTransform = function () {
  var banned = $settings.bannedWords || 'freak,darn,shoot';
  var words = banned.split(',').map(function (w) { return w.trim().toLowerCase(); });
  var body = $message.body || '';

  var matched = words.some(function (w) {
    return w.length > 0 && body.toLowerCase().indexOf(w) !== -1;
  });

  if (matched) {
    $message.setSpam(true);
  }

  return $message;
};
