# Chaturbate App V2 — Quick Reference

> Short-form human-readable docs. All info verified from official TypeScript type definitions.
> Portal: https://devportal.cb.dev — v0.102.0 — Updated 2026-07-11

---

## Globals (IDE Autocomplete — 15 Objects)

```
$app  $api  $callback  $fanclub  $kv  $limitcam  $media  $message
$overlay  $room  $settings  $shortcut  $tip  $user  console
```

---

## Event Handlers (19 Total)

| Handler | Trigger | Key Payloads |
|---------|---------|--------------|
| App Start | App initialized | `$app`, `$callback`, `$kv`, `$room` |
| App Stop | App shutting down | `$app`, `$callback`, `$kv`, `$room` |
| App Settings Change | Settings modified | `$app`, `$callback`, `$kv`, `$room` |
| Broadcast Start | Goes live | `$app`, `$callback`, `$kv`, `$room` |
| Broadcast Stop | Ends stream | `$app`, `$callback`, `$kv`, `$room` |
| Room Status Change | Visibility changes | `$app`, `$callback`, `$kv`, `$room` |
| User Enter | Member enters | `$app`, `$callback`, `$kv`, `$room`, `$user` |
| User Leave | Member leaves | Same as Enter |
| User Follow | User follows | Same as Enter |
| User Unfollow | User unfollows | Same as Enter |
| Chat Message | Message published | `$app`, `$callback`, `$kv`, `$message`, `$room`, `$user` |
| Chat Message Transform | Before publish | `$app`, `$kv`, `$message`, `$room` (owner only), `$user` — req: Transform messages |
| Tip Received | Tip sent | `$app`, `$callback`, `$kv`, `$room`, `$tip`, `$user` |
| Tip Dialog Open | Tip dialog opened | `$app`, `$callback`, `$kv`, `$room`, `$user` — req: Tip options |
| Broadcast Panel Update | Panel reload | `$app`, `$kv`, `$limitcam`, `$room`, `$user` — req: Broadcast panel |
| Fanclub Join | User joins fanclub | Same as Enter + `$fanclub` |
| Media Purchase | Media bought | Same as Enter + `$media` |
| Callback | Timer fires | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` |
| Shortcut | Button clicked | `$app`, `$callback`, `$kv`, `$limitcam`, `$overlay`, `$room`, `$shortcut`, `$user` |

---

## API Objects Quick Reference

### `$app` — App Metadata
`name`, `version`, `summary`, `description`, `permission` (chatMessageTransform, broadcastPanelUpdate, tipDialogOpen)

### `$callback` — Callback Timer
`create(label, delay?, repeating?)` — delay in secs. `cancel(label)`. `label` (in handler).

### `$fanclub` — Fan Club
`isNew: boolean` (available in Fanclub Join handler)

### `$kv` — Key-Value Storage
`get(key, default?)` `set(key, value, expire?)` `incr(key, amt?)` `decr(key, amt?)`
`remove(key)` `clear()` `iter(prefix?)`
Iterator: `next()` `key()` `value()` `seek(key)` `delete()`

### `$limitcam` — Limitcam
`active`, `users[]`, `start(msg, users?)`, `stop()`, `add(users)`, `remove(users)`, `removeAll()`, `hasAccess(user)`

### `$media` — Media Purchase Data
`id`, `name`, `tokens`, `type` (in Media Purchase handler)

### `$message` — Chat Message
`body`, `bgColor`, `color`, `font`, `isSpam`, `orig`
Methods (Transform): `setBody()`, `setBgColor()`, `setColor()`, `setFont()`, `setSpam()`
Available fonts: Default, Arial, Bookman Old Style, Comic Sans, Courier, Lucida, Palantino, Tahoma, Times New Roman

### `$overlay` — Overlay Events
`emit(eventName, data)` — send to overlays. `on(eventName, callback)` — receive in overlays.

### `$room` — Room SDK
**Properties**: `anonCount`, `chatAllowedBy`, `fanclubPrice`, `followerCount`, `genders[]`, `owner`, `ownerGender`, `psEnabled`, `psMinBalance`, `psMinTime`, `psPrice`, `psRecEnabled`, `psSpyPrice`, `status`, `subject`, `users[]`
**Status enum**: public | private | hidden | away | offline | password protected
**Methods**: `reloadPanel()`, `sendNotice(msg, opts?)`, `setPanelTemplate(opts)`, `setSubject(subject)`, `setTipOptions(opts)`
**sendNotice opts**: `toUsername`, `color`, `bgColor`, `fontWeight`, `toColorGroup`

### `$settings` — App Settings
`$settings.propertyName` — types: string, number, boolean, select, textarea, color, table

### `$shortcut` — Shortcut Button
`name`, `title`, `description` (in Shortcut handler)

### `$tip` — Tip Data
`tokens: number`, `message: string`, `isAnon: boolean`

### `$user` — User Object (20 properties)
`username`, `gender`, `subgender`, `colorGroup`, `language`, `inFanclub`, `fcAutoRenew`, `hasTokens`, `hasDarkmode`, `isMod`, `isOwner`, `isFollower`, `isBroadcasting`, `isSilenced`, `inPrivateShow`, `tippedRecently`, `tippedAlotRecently`, `tippedTonsRecently`, `recentTips` (5-value enum)

### `console` — Console API
`console.log(...args)` — standard API, visible with `/debug`, throttled 1/10s

---

## Broadcast Panel (270×69 px)

Templates: `3_rows_of_labels` (label:value), `image_template` with `layers`[] or `table`
Layer props: type, fileID, text, left, top, opacity, color, width, font-size, font-weight, max-width, background-color, text-align, font-style

---

## Shortcuts
One clickable button per app in chat. `onShortcut` handler fires with `$shortcut` context. Usable by broadcasters and viewers.

---

## Broadcast Overlays
Write HTML/CSS/JS for OBS. Use `$overlay.on()` to receive events. `$kv.get()` read-only. Configure via settings in IDE.

---

## Permissions Required
- `Broadcast panel` → Broadcast Panel Update handler
- `Tip options` → Tip Dialog Open handler
- `Transform messages` → Chat Message Transform handler

---

## Key Restrictions
No fetch/XHR. No capturing stream. No forcing tips. No Bio/Settings tab modification.
Data deleted 90d after uninstall. Max panel 270×69.

---

## Utility
- `setTimeout(fn, ms)` / `cancelTimeout(id)` — Timer management
- `$room.log(msg)` — Legacy debug log
- `console.log(...args)` — Standard console API (both visible with `/debug`)

---

## IDE
- URL: https://devportal.cb.dev — v0.102.0
- Testbed: https://testbed.cb.dev (embedded)
- Features: Code editor, handler selection, settings schema editor, advanced settings, broadcast overlays, shortcuts, testbed, app directory

---

## Wiki (55 pages — all extracted from JS bundle)

See `AGENTS.md → Wiki Page Map` for full list. Key pages:
- `/wiki/api/$room`, `/$kv`, `/$user`, `/$tip`, `/$media`, `/$message`, `/$overlay`, `/$shortcut`, `/$fanclub`, `/$app`, `/$callback`
- `/wiki/event-handlers/` (10 handler pages)
- `/wiki/broadcast-overlays`, `/wiki/shortcuts`, `/wiki/testbed`, `/wiki/advanced-settings`, `/wiki/shared`
