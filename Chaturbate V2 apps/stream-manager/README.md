# Stream Manager — Chaturbate App V2

A broadcaster utility that monitors room activity, tracks tip goals, manages viewer engagement, and controls an OBS overlay.

## Features

| Handler | What it does |
|---|---|
| App Start | Initialises KV counters, sets up callback reminder |
| App Stop | Cancels callback, saves session summary |
| App Settings Change | Syncs tip goal from settings to KV |
| Broadcast Start | Announces room is live with PS status |
| Broadcast Stop | Reports session stats and duration |
| Room Status Change | Saves stats on offline, resets on public |
| User Enter | Personalised welcome (follower/mod/fanclub) |
| User Leave | Alerts broadcaster if moderator leaves |
| User Follow | Tracks count, sends overlay event |
| User Unfollow | Detects regular unfollows, alerts broadcaster |
| Chat Message | `!commands`, `!tipmenu`, `!goal`, `!hiddencam`, `!anon`, `!top`, `!notif` commands |
| Chat Message Transform | Filters banned words via settings |
| Tip Received | Tracks tips/goal, top tipper, overlay events |
| Tip Dialog Open | Configurable vote options / tip menu with per-user discounts |
| Broadcast Panel Update | Live 3-row cycling stats panel with themed progress bar |
| Fanclub Join | Welcome new/renewing members |
| Media Purchase | Alerts broadcaster with purchase details |
| Callback | 5-min tip goal reminder, anon converter blackout/restore, panel cycling, notification auto-tick |
| Shortcut | `Tip Goal` button — shows progress and status |
| Broadcast Overlay | OBS overlay with tip goal bar, follower alerts, rolling notifications, anon banner |

## Installation

1. Create a new app at https://devportal.cb.dev
2. Copy the **Shared Code** from `shared.js`
3. Copy each handler into its respective IDE tab
4. Import `settings-schema.json` in the IDE Settings tab
5. (Optional) Upload `overlay.html` in the Broadcast Overlays section
6. Required permissions: Broadcast panel, Tip options, Transform messages

## Simulator

```bash
node simulator.js
```

Runs a full event sequence locally without deploying to the testbed. Covers:
- Tip goal tracking, goal reached reset
- Discount system (fanclub/follower per-group discounts)
- Hidden cam lifecycle (toggle, auto-grant on tip, user enter notification)
- Anon user converter (blackout/restore cycle, blacklist)
- Panel cycling (3 views: Top Tipper / Anon Conv / Cam status)
- Progress bar themes, styles, text modes
- Context-aware reply notifications (greeting, compliment, question, request, farewell, excitement)
- Tip leaderboard (`!top`)
- Rolling notification queue

## Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| tipGoal | number | 1000 | Session tip goal in tokens |
| bannedWords | text | "freak,darn,shoot" | Comma-separated words to auto-filter in chat |
| voteOptions | table | [Sing, Dance, Joke] | Custom tip-vote options |
| overlayEnabled | boolean | true | Enable overlay event emission |
| hiddenCamPrice | number | 100 | Min tokens for auto-granted hidden cam access |
| hiddenCamMessage | text | "Hidden cam is active! Tip {{price}}+ tokens for access." | Message shown when hidden cam is active |
| tipMenuEnabled | boolean | true | Enable the itemised tip menu with discounts |
| tipMenuLabel | text | "Choose a menu item:" | Label above tip options in dialog |
| tipMenuItems | table | [Dance(10), Sing(25), Joi(50)] | Menu items (name, description, price) |
| discountsEnabled | boolean | false | Enable per-group discounts on tip items |
| discountEntries | table | [fanclub→*→20%, follower→Dance→10%] | Discount rules: group, item, percent |
| anonConverterEnabled | boolean | true | Periodically blacks out cam for anonymous users |
| anonConverterInterval | number | 180 | Seconds between blackout events |
| anonConverterBlackoutDuration | number | 15 | Seconds the cam is blacked out |
| anonConverterMessage | text | "Uninterrupted cam access..." | Message to anons during blackout |
| anonConverterAffiliateLink | text | "https://chaturbate.com/ref/signup/" | Affiliate link in blackout message |
| progressTheme | select | coral-ember | Progress bar color theme (20 options) |
| progressBarStyle | select | bar | Visual style: blocks, arrows, circles, squares, percent |
| progressTextMode | select | auto | Text behavior: auto, static, scroll, animate |
| notificationCount | number | 5 | Max visible notifications in rolling queue |
| notificationAutoEnabled | boolean | true | Auto-mode: random notifications at intervals |
| notificationAutoIntervalMin | number | 60 | Min seconds between auto notifications |
| notificationAutoIntervalMax | number | 300 | Max seconds between auto notifications |
| notificationReplyEnabled | boolean | true | Reply-mode: context-aware responses to chat messages |
| overlayNotificationsEnabled | boolean | false | Show rolling notifications in OBS overlay |
| notificationTemplates | table | 20 defaults | Custom notification message templates (type, template, weight) |
| notificationReplyTemplates | table | 15 defaults | Context-aware reply templates (type, template, weight) |

## Overlay

The broadcast overlay (`overlay.html`) provides:
- **Tip Goal Bar**: Animated gradient progress bar with current/goal display
- **Follower Alert**: Slides in when someone follows, auto-fades
- **Goal Reached**: Gold burst animation when goal is hit
- **Anon Converter Banner**: Full-screen overlay during blackout with countdown timer
- **Rolling Notifications**: Slide-in widget with color-coded borders per type (tip=gold, follow=green, enter=blue, fanclub=purple, etc.)

## Architecture

```
shared.js  ──→  Utility functions, discount engine, anon converter lifecycle,
                hidden cam, panel cycling, leaderboard, progress bar, notifications
     │
     ├── handlers/app-start.js              Initialize KV, callbacks
     ├── handlers/app-stop.js               Cleanup, session summary
     ├── handlers/app-settings-change.js    Sync settings → KV
     ├── handlers/broadcast-start.js        Live announcement
     ├── handlers/broadcast-stop.js         Session stats
     ├── handlers/room-status-change.js     Save/reset on offline/public
     ├── handlers/user-enter.js             Personalised welcome
     ├── handlers/user-leave.js             Mod departure alert
     ├── handlers/user-follow.js            Track count, overlay event
     ├── handlers/user-unfollow.js          Regular unfollow detection
     ├── handlers/chat-message.js           Command routing + reply mode
     ├── handlers/chat-message-transform.js Banned word filter
     ├── handlers/tip-received.js           Goal tracking, top tipper, auto-grant
     ├── handlers/tip-dialog-open.js        Per-user discounted tip menu
     ├── handlers/broadcast-panel-update.js 3-row cycling panel
     ├── handlers/fanclub-join.js           New/renew detection
     ├── handlers/media-purchase.js         Purchase alert
     ├── handlers/callback.js               Routes all timers
     └── handlers/shortcut.js               Tip Goal button
```

## Chat Commands

| Command | Description | Access |
|---|---|---|
| `!commands` | List available commands | Everyone |
| `!menu` / `!tipmenu` | Show tip menu with per-user prices | Everyone |
| `!goal` | Show current tip goal progress | Everyone |
| `!discounts` | Show your active discount groups | Everyone |
| `!hiddencam` | Show hidden cam status | Everyone |
| `!hiddencam on/off` | Toggle hidden cam | Broadcaster only |
| `!hiddencam add/remove <user>` | Manage hidden cam access | Broadcaster only |
| `!anon` | Show anon converter status | Everyone |
| `!anon on/off` | Toggle anon converter | Broadcaster only |
| `!anon interval <sec>` | Set blackout interval | Broadcaster only |
| `!anon duration <sec>` | Set blackout duration | Broadcaster only |
| `!anon add/remove <user>` | Manage anon exception list | Broadcaster only |
| `!anon blacklist` | List blacklisted users | Broadcaster only |
| `!anon blacklist add/remove <user>` | Manage anon blacklist | Broadcaster only |
| `!top [N]` | Show top N tippers (default 5) | Everyone |
| `!notif` | Show notification system status | Everyone |
| `!notif reset` | Reset notification settings | Broadcaster only |

---

## AI Development Disclosure

This app was developed with assistance from the following AI models and agents:

| Component | Model / Agent | Role |
|---|---|---|
| Initial scaffolding & handler stubs | Claude (Anthropic) | Generated initial `PHASE1_PROMPT.md` specification and code structure |
| Feature planning (anon converter, notifications) | `gem-implementer` agent | Designed feature plans under `docs/plan/` and `.omo/plans/` |
| Code refinement & bug fixes | Claude (Anthropic) via pi coding agent | Debugged duplicate const declarations, atomicity issues, duplicate notices, unbounded KV growth, missing settings, personal content in templates |
| Handler implementation | Claude (Anthropic) | Wrote handler logic for all 18 event handlers + overlay.html |
| Simulator | Claude (Anthropic) | Built the Node.js simulation environment with mocked globals |
| Settings schema | Claude (Anthropic) | Defined 30+ settings in IDE-compatible format |
| Final audit & review | pi coding agent (2026-07-16) | Systematic bug hunt, optimization pass, documentation update |

**Verification methodology**: All API objects, properties, and methods verified against official TypeScript type definitions extracted from the IDE JS bundle (Portal v0.102.0). See `AGENTS.md` for the definitive reference.

No code was generated by AI without human review. Every handler has been tested via the local simulator and assessed for correctness against the Chaturbate App V2 platform sandbox restrictions.

---

## License

Private — All rights reserved. Developed for use by the app author.
