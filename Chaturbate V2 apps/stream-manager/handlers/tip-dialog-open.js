// Tip Dialog Open — show tip menu with per-user discounts
// REQUIRES: Tip options permission
onTipDialogOpen = function () {
  var username = $user.username;

  // If tip menu is enabled, build discounted options
  if ($settings.tipMenuEnabled !== false) {
    var menuOptions = buildTipMenuOptions(username);
    if (menuOptions) {
      $room.setTipOptions(menuOptions);
      return;
    }
  }

  // Fallback to vote options
  var voteOpts = $settings.voteOptions;
  if (voteOpts && Array.isArray(voteOpts) && voteOpts.length > 0) {
    var options = voteOpts.map(function (pair) {
      return { label: pair[0] + (pair[1] ? ' — ' + pair[1] : '') };
    });
    $room.setTipOptions({ label: 'Vote with your tip:', options: options });
  } else {
    $room.setTipOptions({
      label: 'Vote with your tip:',
      options: [
        { label: 'Sing a song' },
        { label: 'Do a dance' },
        { label: 'Tell a joke' },
      ],
    });
  }
};
