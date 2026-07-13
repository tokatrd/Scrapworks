# SKILL.md — Chaturbate App V2 Development Skill

> Skill definition for AI agents building Chaturbate App V2 applications.
> Platform: https://devportal.cb.dev — Portal v0.102.0 — Updated 2026-07-11

---

## Skill Identity

**Name**: `chaturbate-app-v2`
**Description**: Develop, test, and debug Chaturbate App V2 JavaScript applications.
**Triggers**: "build a Chaturbate app", "create CB app", "Chaturbate V2", "CB app development", "app v2 development"

---

## Prerequisites

1. **Account**: Active Chaturbate account
2. **Access**: Login at https://devportal.cb.dev
3. **Knowledge**: JavaScript proficiency required
4. **References**: See `AGENTS.md` in this directory for the **definitive** API reference (verified from TypeScript types)
5. **External Dev Kit**: Optional — `recursivedesire/Chaturbate-AppV2-DevKit` for TypeScript/offline development (targets v0.73.0 — may lag behind portal v0.102.0)

### All 15 Global Objects Available in IDE
`$app`, `$api`, `$callback`, `$fanclub`, `$kv`, `$limitcam`, `$media`, `$message`, `$overlay`, `$room`, `$settings`, `$shortcut`, `$tip`, `$user`, `console`

> ⚠️ `$tipper` does not exist in V2. Use `$tip` instead.

---

## Workflow

### Phase 1: IDE Setup
1. Navigate to https://devportal.cb.dev and log in
2. Create a new app from the dashboard
3. Set app name, summary, description
4. Configure required permissions
5. Define settings fields (basic + advanced if needed)
6. Optionally add Broadcast Overlays (HTML/CSS/JS for OBS) or Shortcuts

### Phase 2: Development

**Shared Code** — Executes before every handler:
```javascript
// Utility functions, constants — NOT for data storage
// Use $kv for persistent data
const APP_VERSION = "1.0.0";
```

**Handler-specific code** — One tab per event in the IDE.

### Phase 3: Testing
Use the IDE's built-in testbed at https://testbed.cb.dev to simulate events.

### Phase 4: Publishing
Apps can be private or listed in the App Directory.

---

## Code Patterns

### Pattern 1: Tip Goal Tracker with KV
```javascript
// App Start
onAppStart = () => {
  if ($kv.get("currentTips") === undefined) $kv.set("currentTips", 0);
};

// Tip Received
onTipReceived = () => {
  let tips = $kv.get("currentTips") || 0;
  tips += $tip.tokens;
  $kv.set("currentTips", tips);
  $room.reloadPanel();
};

// Broadcast Panel Update
onBroadcastPanelUpdate = () => {
  const tips = $kv.get("currentTips") || 0;
  $room.setPanelTemplate({
    template: "image_template",
    layers: [
      { type: "text", text: `Goal: ${tips}/500`, top: 5, left: 10,
        "font-size": 14, color: "white" },
    ],
  });
};
```

### Pattern 2: Welcome Notice
```javascript
onUserEnter = () => {
  $room.sendNotice(`Welcome to the room, ${$user.username}!`, { toUsername: $user.username });
  if ($room.psEnabled) {
    $room.sendNotice([
      `Private shows: ${$room.psPrice} tks/min`,
      `Min: ${$room.psMinTime} min`,
      `Min balance: ${$room.psMinBalance} tks`,
    ].join("\n"), { toUsername: $user.username });
  }
};
```

### Pattern 3: Chat Command with $message
```javascript
onChatMessage = () => {
  if ($message.body === "!commands") {
    $room.sendNotice("Commands: !tipmenu, !social, !goal", { toUsername: $user.username });
  }
};
```

### Pattern 4: Anonymous Tip Alert
```javascript
onTipReceived = () => {
  if (!$tip.isAnon) {
    $room.sendNotice(`${$user.username} just tipped ${$tip.tokens} tokens!`);
  }
};
```

### Pattern 5: Fanclub Join Detection
```javascript
onFanclubJoin = () => {
  if ($fanclub.isNew) {
    $room.sendNotice(`${$user.username} joined the fanclub!`);
  } else {
    $room.sendNotice(`${$user.username} extended their fanclub membership`,
      { toUsername: $room.owner });
  }
};
```

### Pattern 6: Overlay Event
```javascript
// Tip Received handler — emit event to overlay
onTipReceived = () => {
  $overlay.emit("tip", {
    username: $user.username,
    amount: $tip.tokens,
    message: $tip.message,
    isAnon: $tip.isAnon,
  });
};

// In Broadcast Overlay JS:
$overlay.on("tip", (data) => {
  document.getElementById("alert").innerHTML =
    `${data.username} tipped ${data.amount} tokens!`;
});
```

---

## Common Pitfalls

| Issue | Cause | Fix |
|---|---|---|
| KV data not updating in panel | Caching in Panel Update handler | Use `$room.reloadPanel()` elsewhere |
| Panel rendering slow in large rooms | Accessing `$user` in panel handler | Minimize `$user` reads in `onBroadcastPanelUpdate` |
| Settings not visible | Schema not defined correctly | Check IDE settings tab for proper field definitions |
| Transform not working | Missing permission | Add Transform messages permission to app |
| $callback not firing | Broadcast offline | Callbacks pause when offline, resume when online |
| Duplicate console logs | Throttle active | Max 1 per 10 seconds across all handlers |
| $kv.get() raises error | Key doesn't exist | Provide default: `$kv.get("key", 0)` |
| Shortcut event not working | Only 1 shortcut supported | Delete existing shortcut before creating new one |

---

## Debugging

1. **Logging**: `$room.log(msg)` or `console.log(...args)` — visible with `/debug` chat command
2. **Testbed**: Simulate events at https://testbed.cb.dev
3. **Browser console**: Check for errors during development

---

## Support
- Email: apps@chaturbate.com
- Legacy docs: https://chaturbate.com/apps/docs/
