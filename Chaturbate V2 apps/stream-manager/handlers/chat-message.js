// Chat Message — !commands, !tipmenu, !goal, !hiddencam, !discounts handlers
onChatMessage = function () {
  var body = $message.body || '';
  var parts = body.split(' ');
  var cmd = parts[0];

if (cmd === '!commands') {
    $room.sendNotice(
      'Commands: !menu, !goal, !hiddencam, !discounts, !anon, !top, !notif',
      { toUsername: $user.username }
    );
    return;
  }

  if (cmd === '!menu' || cmd === '!tipmenu') {
    var menuText = formatTipMenuText($user.username);
    $room.sendNotice(menuText, { toUsername: $user.username });
    return;
  }

  if (cmd === '!discounts') {
    var discText = formatUserDiscountsText($user.username);
    $room.sendNotice(discText, { toUsername: $user.username });
    return;
  }

  if (cmd === '!goal') {
    var tips = $kv.get(SESSION_TIPS_KEY, 0);
    var goal = $kv.get(TIP_GOAL_KEY, 1000);
    var pct = goal > 0 ? Math.round((tips / goal) * 100) : 0;
    $room.sendNotice(
      'Tip goal: ' + tips + ' / ' + goal + ' (' + pct + '%)',
      { toUsername: $user.username }
    );
    return;
  }

  if (cmd === '!hiddencam') {
    var sub = parts[1];
    var arg = parts.slice(2).join(' ');

    if (!sub) {
      var active = $kv.get(HIDDEN_CAM_KEY, false);
      var excluded = active ? ($room.users.length - $limitcam.users.length) : 0;
      var price = $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE);
      $room.sendNotice(
        'Hidden cam: ' + (active ? 'ACTIVE' : 'OFF') +
        (active ? ' (' + excluded + ' users excluded, auto-access at ' + price + '+ tks)' : ''),
        { toUsername: $user.username }
      );
      return;
    }

    if (sub === 'on' || sub === 'off') {
      if ($user.username !== $room.owner) {
        $room.sendNotice('Only the broadcaster can toggle hidden cam.',
          { toUsername: $user.username });
        return;
      }
      var result = toggleHiddenCam();
      var status = result === 'started' ? 'Hidden cam started.' : 'Hidden cam stopped.';
      $room.sendNotice(status, { toUsername: $user.username });
      return;
    }

    if (sub === 'add' && arg) {
      if ($user.username !== $room.owner) {
        $room.sendNotice('Only the broadcaster can grant access.',
          { toUsername: $user.username });
        return;
      }
      $limitcam.add([arg]);
      $room.sendNotice(arg + ' has been granted hidden cam access.',
        { toUsername: $user.username });
      return;
    }

    if (sub === 'remove' && arg) {
      if ($user.username !== $room.owner) {
        $room.sendNotice('Only the broadcaster can revoke access.',
          { toUsername: $user.username });
        return;
      }
      $limitcam.remove([arg]);
      $room.sendNotice(arg + ' hidden cam access removed.',
        { toUsername: $user.username });
      return;
    }
  }

  // ============================================================
  // !anon command — anon converter control & blacklist
  // ============================================================
  if (cmd === '!anon') {
    var sub = parts[1];
    var arg = parts.slice(2).join(' ');

    if (!sub) {
      // Status
      var active = $kv.get(ANON_CONVERTER_KEY, false);
      var blackout = $kv.get(ANON_CONVERTER_BLACKOUT_KEY, false);
      var interval = $kv.get(ANON_CONVERTER_INTERVAL_KEY, DEFAULT_ANON_INTERVAL);
      var duration = $settings.anonConverterBlackoutDuration || DEFAULT_ANON_DURATION;
      var bl = getAnonBlacklist();
      var anonCount = $room.anonCount || 0;
      $room.sendNotice(
        'Anon Converter: ' + (active ? 'ON' : 'OFF') +
        (blackout ? ' (BLACKOUT ACTIVE)' : '') +
        ' | Interval: ' + interval + 's | Duration: ' + duration + 's | Anons: ' + anonCount +
        ' | Blacklisted: ' + bl.length,
        { toUsername: $user.username }
      );
      return;
    }

    // Broadcaster-only subcommands
    var isOwner = $user.username === $room.owner;

    if (sub === 'on') {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can start anon converter.', { toUsername: $user.username }); return; }
      startAnonConverter();
      $room.sendNotice('Anonymous User Converter started.', { toUsername: $user.username });
      return;
    }

    if (sub === 'off') {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can stop anon converter.', { toUsername: $user.username }); return; }
      stopAnonConverter();
      $room.sendNotice('Anonymous User Converter stopped.', { toUsername: $user.username });
      return;
    }

    if (sub === 'interval' && arg) {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can change interval.', { toUsername: $user.username }); return; }
      var sec = Number(arg);
      if (!sec || sec < 10) { $room.sendNotice('Interval must be a number >= 10 seconds.', { toUsername: $user.username }); return; }
      $settings.anonConverterInterval = sec;
      $kv.set(ANON_CONVERTER_INTERVAL_KEY, sec);
      $callback.cancel(ANON_CONVERTER_CB_LABEL);
      $callback.create(ANON_CONVERTER_CB_LABEL, sec, true);
      $room.sendNotice('Anon converter interval set to ' + sec + 's.', { toUsername: $user.username });
      return;
    }

    if (sub === 'duration' && arg) {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can change duration.', { toUsername: $user.username }); return; }
      var dur = Number(arg);
      if (!dur || dur < 1) { $room.sendNotice('Duration must be a number >= 1 second.', { toUsername: $user.username }); return; }
      $settings.anonConverterBlackoutDuration = dur;
      $room.sendNotice('Anon converter blackout duration set to ' + dur + 's.', { toUsername: $user.username });
      return;
    }

    if (sub === 'add' && arg) {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can grant access.', { toUsername: $user.username }); return; }
      $limitcam.add([arg]);
      $room.sendNotice(arg + ' granted cam access for current blackout.', { toUsername: $user.username });
      return;
    }

    if (sub === 'remove' && arg) {
      if (!isOwner) { $room.sendNotice('Only the broadcaster can revoke access.', { toUsername: $user.username }); return; }
      $limitcam.remove([arg]);
      $room.sendNotice(arg + ' cam access removed.', { toUsername: $user.username });
      return;
    }

    if (sub === 'blacklist') {
      var blSub = parts[2];
      var blUser = parts[3];
      if (!isOwner) { $room.sendNotice('Only the broadcaster can manage blacklist.', { toUsername: $user.username }); return; }
      if (blSub === 'add' && blUser) {
        if (addToAnonBlacklist(blUser)) {
          $room.sendNotice(blUser + ' added to anon blacklist.', { toUsername: $user.username });
        } else {
          $room.sendNotice(blUser + ' is already blacklisted.', { toUsername: $user.username });
        }
        return;
      }
      if (blSub === 'remove' && blUser) {
        if (removeFromAnonBlacklist(blUser)) {
          $room.sendNotice(blUser + ' removed from anon blacklist.', { toUsername: $user.username });
        } else {
          $room.sendNotice(blUser + ' was not blacklisted.', { toUsername: $user.username });
        }
        return;
      }
      // List blacklist
      var bl = getAnonBlacklist();
      if (bl.length === 0) {
        $room.sendNotice('Anon blacklist is empty.', { toUsername: $user.username });
      } else {
        $room.sendNotice('Blacklisted: ' + bl.join(', '), { toUsername: $user.username });
      }
      return;
    }
  }

  // ============================================================
  // !top command — tip leaderboard
  // ============================================================
  if (cmd === '!top') {
    var limit = parts[1] ? Number(parts[1]) : 5;
    if (!limit || limit < 1) limit = 5;
    var top = getTopTippers(limit);
    if (top.length === 0) {
      $room.sendNotice('No tips recorded yet.', { toUsername: $user.username });
      return;
    }
    var lines = ['Top tippers:'];
    for (var i = 0; i < top.length; i++) {
      lines.push((i + 1) + '. ' + top[i].username + ' — ' + top[i].tokens + ' tokens');
    }
    $room.sendNotice(lines.join(' | '), { toUsername: $user.username });
    return;
  }

  // ============================================================
  // !notif command — notification control
  // ============================================================
  if (cmd === '!notif') {
    var sub = parts[1];

    if (!sub) {
      var autoEnabled = $settings.notificationAutoEnabled !== false;
      var autoMin = Math.max(Number($settings.notificationAutoIntervalMin) || 60, 60);
      var autoMax = Math.max(Number($settings.notificationAutoIntervalMax) || 300, 60);
      var replyEnabled = $settings.notificationReplyEnabled !== false;
      var overlayOn = $settings.overlayNotificationsEnabled !== false;
      var queue = getNotificationQueue();
      $room.sendNotice(
        'Notif: Auto ' + (autoEnabled ? 'ON' : 'OFF') + ' (' + autoMin + '-' + autoMax + 's) | ' +
        'Reply ' + (replyEnabled ? 'ON' : 'OFF') + ' | ' +
        'Overlay ' + (overlayOn ? 'ON' : 'OFF') + ' | ' +
        'Queue: ' + queue.length,
        { toUsername: $user.username }
      );
      return;
    }

    if (sub === 'reset') {
      if ($user.username !== $room.owner) {
        $room.sendNotice('Only the broadcaster can reset notification settings.', { toUsername: $user.username });
        return;
      }
      resetNotificationSettings();
      $room.sendNotice('Notification settings reset to defaults.', { toUsername: $user.username });
      return;
    }
  }

  // ============================================================
  // Reply-mode: context-aware notification matching
  // ============================================================
  if ($settings.notificationReplyEnabled !== false && $user.username !== $room.owner) {
    var msgType = detectMessageType(body);
    if (msgType) {
      // Rate limit: max 1 reply notification per user per 30 seconds
      var rateKey = 'notif_reply_' + $user.username;
      var lastReply = $kv.get(rateKey, 0);
      var now = Date.now();
      if (now - lastReply > 30000) {
        $kv.set(rateKey, now);
        var replyNotif = pickContextualNotification(msgType, { username: $user.username });
        if (replyNotif) {
          addNotification(replyNotif.text, replyNotif.type, 'reply', $user.username);
        }
      }
    }
  }
};
