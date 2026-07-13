# AGENTS.md — Chaturbate App V2 Platform Context

> Agent-facing documentation for the Chaturbate App V2 ecosystem.
> Source: https://devportal.cb.dev (Portal v0.102.0)
> Last updated: 2026-07-11
> **Status: DEFINITIVE** — All API objects verified from official TypeScript type definitions extracted from IDE JS bundle.

---

## System Overview

Chaturbate App V2 is a JavaScript execution environment embedded in broadcaster chat rooms. Apps are event-driven scripts that react to room lifecycle events and manipulate room state through a set of global SDK objects. The platform provides an IDE at `devportal.cb.dev` for development, testing, and deployment.

**Key platform characteristics:**
- Single-language (JavaScript/ES)
- Event-driven architecture (no polling)
- Persistent key-value storage (`$kv`) across broadcast sessions
- Graphical settings UI for end-users
- No external network calls allowed (sandboxed)
- Max panel size: 270×69 pixels (broadcast panel)
- Broadcast Overlays: HTML/CSS/JS overlays for OBS integration

---

## Architecture

### Execution Model
```
Broadcaster Room Events
    ↓ (WebSocket/Server push)
App Runtime (sandboxed JS)
    ↓ (event dispatched)
Registered Handler Function
    ↓ (executes)
API calls → $room, $kv, $settings, $overlay, etc.
    ↓ (UI updates)
Broadcast Panel / Chat / Notices / Overlays
```

### App Lifecycle States
1. **Created** — App registered in IDE, no events fire
2. **Installed** — Broadcaster adds app to room
3. **App Start** — `onAppStart` fires; initialize KV, reset state
4. **Running** — Event handlers active; reacts to room events
5. **App Stop** — `onAppStop` fires; optional cleanup
6. **Settings Change** — `onAppSettingsChange` fires when broadcaster modifies settings

---

## IDE Autocomplete — Complete Global Object Map

The following globals are confirmed available in the IDE editor (extracted from JS bundle type definitions):

```
$app          $api          $callback     $fanclub
$kv           $limitcam     $media        $message
$overlay      $room         $settings     $shortcut
$tip          $user          Advanced Settings
console       v
```

> **Note**: `$tipper` does not exist in V2. Replaced by `$tip`. The `$api` object (lowercase) exists — not `API`. `v` appears to be a framework version variable.

---

## Event Handler Reference

| Handler ID | Trigger | Available Objects | Required Permission |
|---|---|---|---|
| `onAppStart` | App initialized (on install/broadcast start) | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onAppStop` | App shutting down | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onAppSettingsChange` | Settings modified by broadcaster | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onBroadcastStart` | Broadcaster goes live | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onBroadcastStop` | Broadcaster ends stream | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onRoomStatusChange` | Room visibility changes | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onUserEnter` | Registered member enters | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$user` | — |
| `onUserLeave` | Registered member leaves | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$user` | — |
| `onUserFollow` | User follows | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$user` | — |
| `onUserUnfollow` | User unfollows | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$user` | — |
| `onChatMessage` | Message published in chat | `$app`, `$callback`, `$kv`, `$limitcam`, `$message`, `$room`, `$user` | — |
| `onChatMessageTransform` | Before message published | `$app`, `$kv`, `$message`, `$room` (owner only), `$user` | `Transform messages` |
| `onTipReceived` | Tip received | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$tip`, `$user` | — |
| `onTipDialogOpen` | Tip dialog opened | `$app`, `$callback`, `$kv`, `$limitcam`, `$room`, `$user` | `Tip options` |
| `onBroadcastPanelUpdate` | Panel reload triggered | `$app`, `$kv`, `$limitcam`, `$room`, `$user` | `Broadcast panel` |
| `onFanclubJoin` | Fan club join | `$app`, `$callback`, `$fanclub`, `$kv`, `$limitcam`, `$room`, `$user` | — |
| `onMediaPurchase` | Media purchased | `$app`, `$callback`, `$kv`, `$limitcam`, `$media`, `$room`, `$user` | — |
| `onCallback` | Callback timer fires | `$app`, `$callback`, `$kv`, `$limitcam`, `$room` | — |
| `onShortcut` | Shortcut button clicked | `$app`, `$callback`, `$kv`, `$limitcam`, `$overlay`, `$room`, `$shortcut`, `$user` | — |

---

## API Object Reference (Definitive — from TypeScript Types)

### `$app` — App Metadata

```typescript
interface App {
    name: string,
    version: string,
    summary: string,
    description: string,
    permission: AppPermission,
}
```

| Property | Type | Description |
|---|---|---|
| `$app.name` | `string` | Name of app |
| `$app.version` | `string` | Version of app |
| `$app.summary` | `string` | Short summary |
| `$app.description` | `string` | Full description |
| `$app.permission` | `AppPermission` | Object of broadcaster permission grants |

**`$app.permission` (AppPermission):**
| Key | Type | Description |
|---|---|---|
| `chatMessageTransform` | `boolean` | True if broadcaster granted Transform messages |
| `broadcastPanelUpdate` | `boolean` | True if broadcaster granted Broadcast panel |
| `tipDialogOpen` | `boolean` | True if broadcaster granted Tip options |

---

### `$api` — API Namespace

```typescript
interface Api {
    [key: string]: (arg: any) => void,
}
```

Low-level API namespace. Properties/methods are string-keyed. Nature partially understood.

---

### `$callback` — Callback/Timer Object

```typescript
interface Callback {
    create(label: string, delay?: number, repeating?: boolean): void,
    cancel(label: string): void,
    label: string,
}
```

| Property/Method | Description |
|---|---|
| `$callback.create(label, delay?, repeating?)` | Schedule a callback with label, delay in seconds. Default delay=1. Passing true for repeating makes it repeat indefinitely. |
| `$callback.cancel(label)` | Cancel a pending callback by label |
| `$callback.label` | `string` — The label of the currently firing callback (available in Callback event handler) |

> Callbacks pause when broadcast goes offline and resume when it comes back online.

---

### `console` — Standard Console API

```typescript
interface Console {
    log(...data: any[]): void;
}
```

- Works on testbed (browser console) and live site (appears in chat with `/debug`)
- Duplicate logs throttled to max 1 per 10 seconds across all handlers
- Also accessible via `console.log()`

---

### `$fanclub` — Fan Club Object

```typescript
interface Fanclub {
    isNew: boolean,
}
```

Available only in `onFanclubJoin` handler.

| Property | Type | Description |
|---|---|---|
| `$fanclub.isNew` | `boolean` | True if user is new fanclub member; false if extending existing membership |

---

### `$kv` — Key-Value Storage

```typescript
interface Kv {
    get(key: string, defaultValue?: any): any,
    set(key: string, value: any, expire?: number): boolean,
    incr(key: string, amount?: number): void,
    decr(key: string, amount?: number): void,
    remove(key: string): boolean,
    clear(): boolean,
    iter(prefix?: string): KvIterator,
}

interface KvIterator {
    next(): boolean,
    key(): string,
    value(): any,
    seek(key: string): void,
    delete(): boolean,
}
```

| Method | Signature | Description |
|---|---|---|
| `$kv.get` | `(key, defaultValue?)` | Retrieve value. Returns default if provided, raises error if key missing |
| `$kv.set` | `(key, value, expire?)` | Store value. Optional expire in seconds. Returns true on success |
| `$kv.incr` | `(key, amount?)` | Atomically increase integer (default amount=1). Auto-creates key |
| `$kv.decr` | `(key, amount?)` | Atomically decrease integer (default amount=1). Auto-creates key |
| `$kv.remove` | `(key)` | Delete key/value. Returns true on success |
| `$kv.clear` | `()` | Delete all entries. Returns true on success |
| `$kv.iter` | `(prefix?)` | Get iterator to scan entries with optional prefix filter |

**KvIterator methods:**
| Method | Description |
|---|---|
| `next()` | Fetch next key, returns true if available (must call before key()/value()) |
| `key()` | Current key (string) |
| `value()` | Current value (any) |
| `seek(key)` | Move cursor to specific key |
| `delete()` | Delete current entry, returns true on success |

**KV Rules:**
- Data scoped per-app, per-broadcaster
- Cannot read another app's data or another broadcaster's data
- Data deleted 90 days after app uninstall
- Broadcast Overlays have read-only access (`get` only)
- In Broadcast Panel Update handler, `$kv` writes may be cached (not immediately visible)
- All JS primitives supported except `Infinity` and `NaN`

---

### `$limitcam` — Limitcam Control

```typescript
interface Limitcam {
    active: boolean,
    users: string[],
    add(users: string[]): void,
    hasAccess(username: string): boolean,
    remove(users: string[]): void,
    removeAll(): void,
    start(message: string, users?: string[]): void,
    stop(): void,
}
```

| Property/Method | Description |
|---|---|
| `active` | `boolean` — Whether limitcam session is currently active |
| `users` | `string[]` — All users in the allow-list |
| `add(users)` | Add users to limitcam allow-list |
| `hasAccess(username)` | Check if user can see cam → boolean |
| `remove(users)` | Revoke access for users |
| `removeAll()` | Clear all users from allow-list |
| `start(message, users?)` | Start limitcam with optional message and initial users |
| `stop()` | Stop limitcam, restore cam for everyone |

> Limitcam session remains active when broadcast stops. Cannot start if broadcast is offline.

---

### `$media` — Media Purchase Object

```typescript
interface Media {
    id: string,
    name: string,
    tokens: number,
    type: string,
}
```

Available only in `onMediaPurchase` handler.

| Property | Type | Description |
|---|---|---|
| `$media.id` | `string` | ID of media set purchased |
| `$media.name` | `string` | Name of media set |
| `$media.tokens` | `number` | Tokens spent |
| `$media.type` | `string` | Media type (photo, video, etc.) |

---

### `$message` — Chat Message Object

```typescript
interface Message {
    bgColor: string,
    body: string,
    color: string,
    font: string,
    isSpam: boolean,
    orig: string,
    setBgColor(color: string): void,
    setBody(body: string): void,
    setColor(color: string): void,
    setFont(font: string): void,
    setSpam(isSpam: boolean): void,
}
```

| Property | Type | Description |
|---|---|---|
| `$message.bgColor` | `string` | Background color (hex or CSS name) |
| `$message.body` | `string` | Message text |
| `$message.color` | `string` | Text color |
| `$message.font` | `string` | Font name |
| `$message.isSpam` | `boolean` | True = hidden from chat |
| `$message.orig` | `string` | Original body before transformations |

**Available fonts:** Default, Arial, Bookman Old Style, Comic Sans, Courier, Lucida, Palantino, Tahoma, Times New Roman

**Methods (Transform handler only):**
| Method | Description |
|---|---|
| `setBody(text)` | Replace message body |
| `setColor(color)` | Set text color |
| `setBgColor(color)` | Set background color |
| `setFont(font)` | Set font |
| `setSpam(bool)` | Mark as spam (hidden) |

---

### `$overlay` — Broadcast Overlay Object

```typescript
interface Overlay {
    emit: (eventName: string, payload: any) => void,
    on: (eventName: string, callback: (data: any) => void) => void,
}
```

| Method | Description |
|---|---|
| `$overlay.emit(eventName, data)` | Send event to Broadcast Overlays (available in any handler) |
| `$overlay.on(eventName, callback)` | Receive events in Broadcast Overlay JS |

> **Broadcast Overlays** is a separate feature allowing HTML/CSS/JS to be written in the app for OBS integration. Overlays can listen for events via `$overlay.on()` and display tip alerts, follow notices, etc.

---

### `$room` — Room SDK Object

```typescript
interface Room {
    anonCount: number,
    chatAllowedBy: string,
    fanclubPrice: number,
    followerCount: number,
    genders: string[],
    owner: string,
    ownerGender: string,
    psEnabled: boolean,
    psMinBalance: number,
    psMinTime: number,
    psPrice: number,
    psRecEnabled: boolean,
    psSpyPrice: number,
    status: string,
    subject: string,
    users: string[],
    reloadPanel(): void,
    sendNotice(message: string, options?: object): void,
    setPanelTemplate(options: object): boolean,
    setSubject(subject: string): void,
    setTipOptions(options: object): void,
}
```

**Properties:**
| Property | Type | Description |
|---|---|---|
| `$room.anonCount` | `number` | Anonymous users in room |
| `$room.chatAllowedBy` | `string` | Chat permission mode |
| `$room.fanclubPrice` | `number` | Fan club join price (0 if disabled) |
| `$room.followerCount` | `number` | Follower count |
| `$room.genders` | `string[]` | Available genders |
| `$room.owner` | `string` | Broadcaster username |
| `$room.ownerGender` | `string` | Broadcaster gender |
| `$room.psEnabled` | `boolean` | Private shows enabled |
| `$room.psMinBalance` | `number` | Min tokens for private request |
| `$room.psMinTime` | `number` | Min duration (minutes) |
| `$room.psPrice` | `number` | Tokens per minute (private) |
| `$room.psRecEnabled` | `boolean` | Private show recording allowed |
| `$room.psSpyPrice` | `number` | Tokens per minute (spy) |
| `$room.status` | `string` | Room state |
| `$room.subject` | `string` | Room subject/title |
| `$room.users` | `string[]` | Usernames in room |

**`$room.status` values:** `"public"` | `"private"` | `"hidden"` | `"away"` | `"offline"` | `"password protected"`

**Methods:**
| Method | Description |
|---|---|
| `$room.reloadPanel()` | Force all users to refresh broadcast panels |
| `$room.sendNotice(message, options?)` | Send notice to room or specific user |
| `$room.setPanelTemplate(options)` | Set broadcast panel content → boolean |
| `$room.setSubject(subject)` | Set room subject (max 200 chars, truncated) |
| `$room.setTipOptions(options)` | Set tip dialog options |

**`$room.sendNotice()` options:**
| Option | Type | Description |
|---|---|---|
| `toUsername` | `string` | Target specific user |
| `color` | `string` | Text color (hex) |
| `bgColor` | `string` | Background color (hex) |
| `fontWeight` | `string` | Font weight |
| `toColorGroup` | `string` | Target color group |

**`$room.setTipOptions()` options:**
```typescript
interface TipOptions {
    label: string,
    options: { label: string }[],
}
```

---

### `$settings` — App Settings

```typescript
interface Settings {
    [key: string]: any,
}
```

Settings are defined in the IDE (JSON schema). Types: `string`, `number`, `boolean`, `select`/`choice`, `textarea`, `color`, `table`.

**Advanced Settings features** (accessible in IDE via "Advanced Settings" toggle):
- **Fieldsets**: Group settings under headers with optional collapsible/collapsed states
- **Nested settings**: Fieldsets can contain sub-settings
- **Color type**: Settings with `type: "color"` show color picker; supports `enum` of hex values with `default`
- **Table type**: Settings with `type: "table"` allow two-column key-value entries
  - `columns: string[]` — Exactly 2 column names
  - `default: string[][]` — Default entries (array of pairs)

---

### `$shortcut` — Shortcut Buttons Object

```typescript
interface Shortcut {
    description: string,
    name: string,
    title: string,
}
```

Available in `onShortcut` event handler. Buttons appear in chat interface for broadcasters and viewers.

| Property | Type | Description |
|---|---|---|
| `$shortcut.name` | `string` | Unique identifier for the shortcut |
| `$shortcut.title` | `string` | Display title of the button |
| `$shortcut.description` | `string` | Description of the shortcut |

> Currently apps can only define one Shortcut (limit to be expanded).

---

### `$tip` — Tip Event Object

```typescript
interface Tip {
    isAnon: boolean,
    tokens: number,
    message: string,
}
```

Available in `onTipReceived` handler.

| Property | Type | Description |
|---|---|---|
| `$tip.tokens` | `number` | Amount of tokens in tip |
| `$tip.message` | `string` | Optional tip message (tip option appended if selected) |
| `$tip.isAnon` | `boolean` | True if tipper requests anonymity — must honor this |

---

### `$user` — User Object (Event Handler Scope)

```typescript
interface User {
    colorGroup: string,
    fcAutoRenew: boolean,
    gender: string,
    hasDarkmode: boolean,
    hasTokens: boolean,
    inFanclub: boolean,
    inPrivateShow: boolean,
    isBroadcasting: boolean,
    isFollower: boolean,
    isMod: boolean,
    isOwner: boolean,
    isSilenced: boolean,
    language: string,
    recentTips: string,
    subgender: string,
    tippedAlotRecently: boolean,
    tippedRecently: boolean,
    tippedTonsRecently: boolean,
    username: string,
}
```

| Property | Type | Description |
|---|---|---|
| `$user.username` | `string` | Display name |
| `$user.gender` | `string` | Gender |
| `$user.subgender` | `string\|null` | Subgender |
| `$user.colorGroup` | `string\|null` | Chat color group |
| `$user.language` | `string\|null` | Language preference |
| `$user.inFanclub` | `boolean` | Is fan club member |
| `$user.fcAutoRenew` | `boolean` | Fan club auto-renewal enabled |
| `$user.hasTokens` | `boolean` | Has tokens in account |
| `$user.hasDarkmode` | `boolean` | Dark mode enabled |
| `$user.isMod` | `boolean` | Is moderator |
| `$user.isOwner` | `boolean` | Is room owner |
| `$user.isFollower` | `boolean` | Is following broadcaster |
| `$user.isBroadcasting` | `boolean` | Currently broadcasting |
| `$user.isSilenced` | `boolean` | Silenced by moderator |
| `$user.inPrivateShow` | `boolean` | In private show |
| `$user.tippedRecently` | `boolean` | Tipped recently |
| `$user.tippedAlotRecently` | `boolean` | Tipped significantly |
| `$user.tippedTonsRecently` | `boolean` | Tipped very significantly |
| `$user.recentTips` | `literal` | `"none"` \| `"few"` \| `"some"` \| `"lots"` \| `"tons"` |

---

## Broadcast Panel Templates

Panel is 270×69 pixels.

### Template: `3_rows_of_labels`
```javascript
{
  row1_label: "Label 1", row1_value: "Value 1",
  row2_label: "Label 2", row2_value: "Value 2",
  row3_label: "Label 3", row3_value: "Value 3",
}
```

### Template: `image_template` with layers
```javascript
{
  template: "image_template",
  layers: [
    {
      type: "image", fileID: "...", left: 0, top: 5,
      opacity: 1, width: 100,
    },
    {
      type: "text", text: "Hello", left: 61, top: 5,
      "font-size": 14, color: "orange", "font-weight": "bold",
      "max-width": 200, "text-align": "center", "font-style": "normal",
      "background-color": "transparent",
    },
  ],
}
```

### Template: `image_template` with table
```javascript
{
  template: "image_template",
  table: {
    row_1: {
      "background-color": "orange",
      col_1: { value: "label", color: "black", "text-align": "center", "font-weight": "bold" },
      col_2: { value: "Value 1", color: "white", "text-align": "center" },
    },
  },
}
```

**Layer properties:**
`type` (image|text), `fileID`, `filename`, `image_url`, `text`, `left`, `top`, `opacity`, `color`, `width`, `font-size`, `font-family`, `font-weight`, `max-width`, `background-color`, `text-align`, `font-style` (normal|italic)

---

## Shortcuts

Apps can define one shortcut button that appears in the chat interface. When clicked, the `onShortcut` event handler fires with `$shortcut` available. Buttons are usable by both broadcasters and viewers.

---

## Broadcast Overlays

Apps can include HTML, CSS, and JavaScript for OBS integration. Overlays can:
- Use `$overlay.on()` to receive events from the app
- Display tip alerts, follow notices, etc. directly in the stream
- Use `$kv.get()` (read-only) for data
- Be configured by broadcasters via settings
- Upload screenshots to showcase overlays in the app listing

---

## Requirements & Restrictions

### Permissions
| Permission | Used By |
|---|---|
| `Broadcast panel` | `onBroadcastPanelUpdate` |
| `Tip options` | `onTipDialogOpen` |
| `Transform messages` | `onChatMessageTransform` |

### Prohibited
- External network calls (`fetch`, `XMLHttpRequest` blocked)
- Recording or capturing the stream
- Forcing tips, stealing tips
- Giving away access to photos/videos
- Modifying Bio tab or Settings & Privacy tab
- Exfiltrating KV/settings data without documentation

### Required
- Document any non-broadcaster access to app settings or KV data
- Use KV storage efficiently

---

## Wiki Page Map — Full Verified List

All pages extracted from the IDE JS bundle as embedded Markdown content:

| Wiki Path | Type |
|---|---|
| `/wiki/api/$app` | API Object |
| `/wiki/api/$callback` | API Object |
| `/wiki/api/$fanclub` | API Object |
| `/wiki/api/$kv` | API Object |
| `/wiki/api/$limitcam` | API Object |
| `/wiki/api/$media` | API Object |
| `/wiki/api/$message` | API Object |
| `/wiki/api/$overlay` | API Object |
| `/wiki/api/$room` | API Object |
| `/wiki/api/$shortcut` | API Object |
| `/wiki/api/$tip` | API Object |
| `/wiki/api/$user` | API Object |
| `/wiki/api/callback-cancel` | Method |
| `/wiki/api/callback-create` | Method |
| `/wiki/api/kv-clear` | Method |
| `/wiki/api/kv-decr` | Method |
| `/wiki/api/kv-get` | Method |
| `/wiki/api/kv-incr` | Method |
| `/wiki/api/kv-iter` | Method |
| `/wiki/api/kv-remove` | Method |
| `/wiki/api/kv-set` | Method |
| `/wiki/api/limitcam-add` | Method |
| `/wiki/api/limitcam-has-access` | Method |
| `/wiki/api/limitcam-remove` | Method |
| `/wiki/api/limitcam-remove-all` | Method |
| `/wiki/api/limitcam-start` | Method |
| `/wiki/api/limitcam-stop` | Method |
| `/wiki/api/message-body` | Method |
| `/wiki/api/message-set-bg-color` | Method |
| `/wiki/api/message-set-color` | Method |
| `/wiki/api/message-set-font` | Method |
| `/wiki/api/message-set-spam` | Method |
| `/wiki/api/room-reload-panel` | Method |
| `/wiki/api/room-send-notice` | Method |
| `/wiki/api/room-set-panel-template` | Method |
| `/wiki/api/room-set-subject` | Method |
| `/wiki/api/room-set-tip-options` | Method |
| `/wiki/advanced-settings` | Feature |
| `/wiki/app-lifecycle` | Guide |
| `/wiki/broadcast-overlays` | Feature |
| `/wiki/event-handlers/app-start` | Handler |
| `/wiki/event-handlers/app-stop` | Handler |
| `/wiki/event-handlers/callback` | Handler |
| `/wiki/event-handlers/fanclub-join` | Handler |
| `/wiki/event-handlers/media-purchase` | Handler |
| `/wiki/event-handlers/message` | Handler |
| `/wiki/event-handlers/tip` | Handler |
| `/wiki/event-handlers/transform-message` | Handler |
| `/wiki/event-handlers/update-panel-template` | Handler |
| `/wiki/event-handlers/update-tip-options` | Handler |
| `/wiki/faq` | Guide |
| `/wiki/shared` | Guide |
| `/wiki/shortcuts` | Feature |
| `/wiki/testbed` | Guide |
| `/wiki/web-components` | Feature (deprecated) |
| `/wiki/screenshots` | Feature |
| `/wiki/shortcode-system-for-chat` | Feature |

**Total: 55 wiki pages** — all content extracted from JS bundle.

---

## External Ecosystem

### Chaturbate-AppV2-DevKit (Community)
- GitHub: `recursivedesire/Chaturbate-AppV2-DevKit`
- API target: v0.73.0 (may lag behind portal v0.102.0)

### Events API (External — NOT in sandbox)
- Endpoint: `https://eventsapi.chaturbate.com/events/{username}/{token}/`
- Long-polling JSON, 2000 req/min limit
- Token creation: chaturbate.com/statsapi/authtoken/
- Event types: TIP, FANCLUB_JOIN, MEDIA_PURCHASE, CHAT_MESSAGE, etc.

### Testbed
- URL: `https://testbed.cb.dev` (within IDE)
- Simulates room events for testing without going live

---

## Changelog Highlights (from Bundle)
- **v0.55.0** (Jun 2024): Broadcast Overlays released; Web Components deprecated
- **v0.51.0**: Shortcodes in chat notices; KV set key expiration added
- **v0.50.0**: Web Components on testbed
- **v0.49.0**: Web Components public beta
- **v0.31.0** (Jun 2023): 4 new $user properties (colorGroup, isBroadcasting, isOwner, isSilenced)
- **v0.23.0**: $user.fcAutoRenew
- **v0.22.0**: Room Status Change handler added
- **v0.20.0**: Advanced Settings feature added
