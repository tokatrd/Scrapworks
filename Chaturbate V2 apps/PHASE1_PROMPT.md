# Phase 1 — Chaturbate App V2 Development

> This prompt incorporates ALL verified wiki knowledge from the extracted IDE bundle.
> Use `AGENTS.md` and `SKILL.md` in this directory as your definitive API reference.

---

## Goal

Build a single, real Chaturbate App V2 application called **"Stream Manager"** that demonstrates competent use of the platform. The app must be a practical tool for broadcasters, not a toy. Every handler, API object, and global must be justified by a real feature — no dead code, no "examples".

Then, as a **bonus deliverable**, create a **local simulator/workbench** (`simulator.js`) that lets the developer test the app's logic from a Node.js or browser console without deploying to the testbed — this is the "console features" deliverable.

---

## App Specification: "Stream Manager"

A broadcaster utility that monitors room activity, tracks goals, manages viewer engagement, and optionally controls an OBS overlay. It must use **all** of these globals meaningfully: `$app`, `$kv`, `$room`, `$user`, `$tip`, `$message`, `$settings`, `$callback`, `$fanclub`, `$shortcut`, `$overlay`, `$media`, `$limitcam`.

---

## Required Features (map every handler to a real broadcaster need)

### 1. Shared Code

Set up constants, runtime environment detection, and a standardized notice-sending utility.

```javascript
// DO: place constants, config, utility functions here
// DON'T: store mutable state here (use $kv)

const APP_NAME = "Stream Manager";
const SUPPORTED_SHOW_TYPES = ["public", "private", "hidden"];

// Utility: send a notice to the broadcaster
function notifyBroadcaster(message, options = {}) {
  $room.sendNotice(message, { toUsername: $room.owner, ...options });
}
```

### 2. App Start / App Stop

**Start**: Initialize KV defaults for all counters (tip goal, follower count, session start time). Log app version.

```javascript
onAppStart = () => {
  if ($kv.get("tipGoal") === undefined) $kv.set("tipGoal", 1000);
  if ($kv.get("sessionTips") === undefined) $kv.set("sessionTips", 0);
  $kv.set("sessionStart", Date.now());
  notifyBroadcaster(`${$app.name} v${$app.version} started.`);
};
```

**Stop**: Log session summary. Optionally fire an overlay event to clear the screen.

### 3. User Enter

Welcome message with room-specific context. Check if user is a follower, mod, or fanclub member and customize the notice.

- If `$user.isFollower === false`: prompt to follow
- If `$user.isMod`: mod on duty badge
- If `$user.inFanclub`: thank them for membership

### 4. User Leave

If a moderator leaves, alert the broadcaster privately.

### 5. User Follow / Unfollow

**Follow**: Count new followers with `$kv.incr("newFollowers", 1)`. Send overlay event.

**Unfollow**: Alert broadcaster privately if the unfollower was a known regular (implement a simple `$kv`-based tracker).

### 6. Chat Message

Implement a `!commands` handler and a `!tipmenu` command that sends the tip menu via `$room.sendNotice()`.

Check `$message.body` for the commands.

### 7. Chat Message Transform

Filter a configurable list of banned words (read from `$settings.bannedWords`). If matched, call `$message.setSpam(true)`.

```javascript
// REQUIRES: Transform messages permission
onChatMessageTransform = () => {
  const banned = $settings.bannedWords || "freak,darn";
  const words = banned.split(",").map(w => w.trim());
  if (words.some(w => $message.body.toLowerCase().includes(w))) {
    $message.setSpam(true);
  }
  return $message;
};
```

### 8. Tip Received

- Update session tips: `$kv.incr("sessionTips", $tip.tokens)`
- If `!$tip.isAnon`, announce the tip (include the optional message)
- Track top tipper in KV
- If session tips meet/exceed `$kv.get("tipGoal")`, announce goal reached and reset
- Emit overlay event with tip data

### 9. Tip Dialog Open

Offer 3 tip-option vote choices (read from `$settings.voteOptions`, a table-type setting). Let chat vote with their tips.

```javascript
onTipDialogOpen = () => {
  const options = ["Sing", "Dance", "Joke"].map(option => ({ label: option }));
  $room.setTipOptions({ label: "Vote:", options });
};
```

### 10. Broadcast Panel Update

Show a live 3-row panel:
- Row 1: Session tips / Goal
- Row 2: New followers this broadcast
- Row 3: Top tipper name

Use `3_rows_of_labels` template. Call this with `$room.reloadPanel()` from relevant handlers.

### 11. Fanclub Join

Send a room-wide welcome if `$fanclub.isNew`, else a private thank-you to the broadcaster for the renewal.

### 12. Media Purchase

Alert the broadcaster with the media name, type, and tokens spent.

### 13. Callback

Set a repeating 5-minute callback on App Start that reminds the broadcaster to check their tip goal. Cancel it on App Stop.

### 14. Shortcut

Create one shortcut button labeled "Tip Goal". When clicked, announce the current progress to the clicker.

### 15. Room Status Change

If room goes `"offline"`, save session stats to KV. If room goes `"public"`, reset session counters.

### 16. Broadcast Start / Stop

**Start**: Announce the room is live with basic stats.
**Stop**: Announce stream duration and session stats.

### 17. App Settings Change

If the tip goal setting changed, update KV. Notify broadcaster of the change.

### 18. Overlay Integration (Bonus)

In the Broadcast Overlays section of the IDE, create an overlay HTML file that:
- Shows a vertical tip goal progress bar (HTML/CSS)
- Listens for `$overlay.on("tip", ...)` events to animate the bar
- Listens for `$overlay.on("follow", ...)` events to show a follower alert

---

## Console Features Bonus: `simulator.js`

Create a file `simulator.js` that runs in Node.js and simulates the app's core logic without needing the Chaturbate testbed.

### Requirements

1. **Mock all globals** (`$app`, `$kv`, `$room`, `$user`, `$settings`, `$tip`, `$message`, `$fanclub`, `$media`, `$shortcut`, `$overlay`, `$callback`) as in-memory JavaScript objects
2. **Implement `$kv`** as a real in-memory Map with full API: `get`, `set`, `incr`, `decr`, `remove`, `clear`, `iter`
3. **Parse the app's four code sections** (shared, app-start, tip-received, broadcast-panel-update, etc.) and execute them against the mock globals
4. **Simulate a sequence of events**: app start → user enter → 3 tips → user follow → panel update → user leave
5. **Log all `$room.sendNotice()` calls** to the console with their parameters
6. **Print the final KV state** after the event sequence
7. **TypeScript/JS module format** — importable, no runtime dependencies

### Example output

```
=== Stream Manager Simulator ===

[APP START] Stream Manager v1.0.0 started.
[NOTICE → broadcaster] Stream Manager v1.0.0 started.

[USER ENTER] alice
[NOTICE → alice] Welcome alice! If you like me please click follow.

[TIP] alice tipped 50 tokens
[NOTICE → room] alice just tipped 50 tokens!
[PANEL] Set: row1_label="Tips", row1_value="50/1000"

[TIP] bob tipped 200 tokens (anon)
[PANEL] Set: row1_label="Tips", row1_value="250/1000"

[USER FOLLOW] charlie
[NOTICE → room] Thank you for the follow, charlie!

=== Final KV State ===
tipGoal: 1000
sessionTips: 250
newFollowers: 1
topTipper: bob (anonymous)
sessionStart: <timestamp>
```

---

## Implementation Rules

1. **No `as any`**, no `@ts-ignore`, no type suppressions anywhere
2. **Every handler must have a real use case** — no placeholder handlers
3. **Use `$kv` correctly** — `get` with defaults, `incr` for counters, `set` for config
4. **Handle edge cases**: anon tips, offline rooms, missing settings values
5. **Match the exact API signatures** documented in AGENTS.md (verified from TypeScript types)
6. **The simulator must actually run** — verify by running `node simulator.js` and checking output

---

## Deliverables

1. **`stream-manager/` directory** containing:
   - `shared.js` — Shared code section
   - `handlers/` — One file per event handler (18 files)
   - `settings-schema.json` — The IDE settings schema
   - `overlay.html` — Broadcast Overlay HTML/CSS/JS
   - `simulator.js` — Console simulator (the bonus deliverable)
   - `README.md` — Setup and usage instructions

2. **All 18 event handlers implemented** as documented above

3. **The simulator must produce output** matching the example format (actual KV values will differ based on simulation sequence)

---

## Success Criteria

- [ ] All 19 event handlers produce real broadcaster value
- [ ] KV storage used correctly (no data loss between handlers)
- [ ] Anon tip handling respects `$tip.isAnon`
- [ ] Panel displays live session stats
- [ ] Overlay receives and displays events
- [ ] Shortcut button works and pulls from KV
- [ ] Callback fires on schedule
- [ ] `simulator.js` runs to completion without errors
- [ ] Simulator output shows all events in sequence
- [ ] No type errors, no `any`, no suppressions
