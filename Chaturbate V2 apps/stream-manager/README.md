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
| Chat Message | `!commands`, `!tipmenu`, `!goal` handlers |
| Chat Message Transform | Filters banned words via settings |
| Tip Received | Tracks tips/goal, top tipper, overlay events |
| Tip Dialog Open | Configurable vote options from settings |
| Broadcast Panel Update | Live 3-row stats panel |
| Fanclub Join | Welcome new/renewing members |
| Media Purchase | Alerts broadcaster with purchase details |
| Callback | 5-min tip goal reminder |
| Shortcut | `Tip Goal` button — shows progress |

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

Runs the full event sequence locally without deploying to the testbed.

## Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| tipGoal | number | 1000 | Session tip goal |
| bannedWords | text | "freak,darn,shoot" | Words to auto-filter |
| voteOptions | table | [Sing, Dance, Joke] | Tip vote choices |
| overlayEnabled | boolean | true | Enable overlay events |
