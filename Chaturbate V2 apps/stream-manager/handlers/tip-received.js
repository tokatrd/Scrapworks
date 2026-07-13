// Tip Received — track tips, announce, emit overlay event
onTipReceived = function () {
  var tips = incrCounter(SESSION_TIPS_KEY, $tip.tokens);
  var goal = $kv.get(TIP_GOAL_KEY, 1000);

  // Announce non-anonymous tips
  if (!$tip.isAnon) {
    var msg = $user.username + ' just tipped ' + $tip.tokens + ' tokens!';
    if ($tip.message) {
      msg += ' "' + $tip.message + '"';
    }
    $room.sendNotice(msg);
  }

  // Track top tipper (only if not anonymous)
  if (!$tip.isAnon) {
    var topTokens = $kv.get(TOP_TIP_TOKENS_KEY, 0);
    var topUser = $kv.get(TOP_TIPPER_KEY, '');
    var currentUserTips = $kv.get('userTips_' + $user.username, 0);
    currentUserTips += $tip.tokens;
    $kv.set('userTips_' + $user.username, currentUserTips);

    if (currentUserTips > topTokens) {
      $kv.set(TOP_TIP_TOKENS_KEY, currentUserTips);
      $kv.set(TOP_TIPPER_KEY, $user.username);
    }
  } else {
    // Track anonymous tips separately
    var anonTips = $kv.get('anonTipTokens', 0);
    anonTips += $tip.tokens;
    $kv.set('anonTipTokens', anonTips);
  }

  // Check goal reached
  if (tips >= goal) {
    $room.sendNotice(
      'Tip goal reached! Total: ' + tips + ' tokens! Thank you everyone!'
    );
    $kv.set(SESSION_TIPS_KEY, 0); // reset counter
    if ($settings.overlayEnabled !== false) {
      $overlay.emit('goalReached', { total: tips });
    }
  }

  // Auto-grant hidden cam access if active and tip meets threshold
  if ($kv.get(HIDDEN_CAM_KEY, false) && !$tip.isAnon) {
    var camPrice = $kv.get(HIDDEN_CAM_PRICE_KEY, DEFAULT_HIDDEN_CAM_PRICE);
    if ($tip.tokens >= camPrice && !$limitcam.hasAccess($user.username)) {
      $limitcam.add([$user.username]);
      $room.sendNotice(
        $user.username + ' has been granted hidden cam access!',
        { toUsername: $user.username }
      );
    }
  }

  // Update panel
  $room.reloadPanel();

  // Emit overlay event
  if ($settings.overlayEnabled !== false) {
    $overlay.emit('tip', {
      username: $tip.isAnon ? 'Anonymous' : $user.username,
      amount: $tip.tokens,
      message: $tip.message || '',
      isAnon: $tip.isAnon,
      total: tips,
      goal: goal,
    });
  }
};
