# TRUTH.md — Chaturbate App V2 Verified Facts

> **Status: DEFINITIVE** — All objects, properties, and methods verified from official TypeScript type definitions extracted from the IDE JS bundle (bundle.5abf7efa.js).
> Source: devportal.cb.dev Portal v0.102.0 — Extracted 2026-07-11

---

## Verification Methodology

1. **Phase 1** — Fetched 3 wiki pages via search engine snippets (introduction, $room, update-panel-template)
2. **Phase 2** — Probed ~28 wiki URLs, all returned SPA shell ("Dev Portal" title) — confirmed auth-gated
3. **Phase 3** — Downloaded IDE JS bundle (~1MB, ~6.5MB), extracted embedded Markdown wiki content and complete TypeScript type definitions
4. **Result** — 55 wiki pages extracted, 25 TypeScript interfaces decoded, full API surface documented

---

## Verified Facts (100% — from TypeScript Types)

### Platform
- **Portal**: devportal.cb.dev — v0.102.0
- **Operator**: Multi Media, LLC (Lake Forest, CA 92630)
- **Language**: JavaScript only (IDE, sandboxed)
- **Sandbox**: No fetch/XHR, no external network
- **Storage**: KV persistent across broadcasts, 90d retention after uninstall
- **Panel**: 270×69 pixels, 3 template types with layers

### Global Objects (15, all verified)
`$app`, `$api`, `$callback`, `$fanclub`, `$kv`, `$limitcam`, `$media`, `$message`, `$overlay`, `$room`, `$settings`, `$shortcut`, `$tip`, `$user`, `console`

### Event Handlers (19, all verified)
App Start/Stop/SettingsChange, Broadcast Start/Stop, RoomStatusChange, User Enter/Leave/Follow/Unfollow, ChatMessage, ChatMessageTransform, TipReceived, TipDialogOpen, BroadcastPanelUpdate, FanclubJoin, MediaPurchase, Callback, Shortcut

### Permissions (3)
`Broadcast panel`, `Tip options`, `Transform messages`

### Broadcast Overlays
Full HTML/CSS/JS support for OBS integration. `$overlay.emit()` from app handlers, `$overlay.on()` in overlay JS. `$kv.get()` read-only.

### Shortcuts
One clickable button per app in chat interface. `$shortcut.name/title/description`. Usable by broadcasters and viewers.

### Advanced Settings
Settings UI with fieldsets (collapsible), nested settings, color picker type, table type.

---

## Changelog (from Bundle)

| Version | Date | Key Changes |
|---------|------|-------------|
| v0.55.0 | Jun 2024 | Broadcast Overlays released; Web Components deprecated |
| v0.51.0 | — | Shortcodes in notices; KV key expiration |
| v0.50.0 | — | Web Components on testbed |
| v0.49.0 | — | Web Components public beta |
| v0.31.0 | Jun 2023 | 4 $user props: colorGroup, isBroadcasting, isOwner, isSilenced |
| v0.23.0 | — | $user.fcAutoRenew added |
| v0.22.0 | — | Room Status Change handler |
| v0.20.0 | — | Advanced Settings feature |

---

## Still Uncertain

- **`$api` object** — Its `[key: string]: (arg) => void` signature suggests a low-level API but use cases are undocumented
- **`v` global** — Appears in autocomplete but not in any type definitions or examples
- **KV storage limits** — Max key length, max store size, rate limits not documented
- **$user.isFollower** — Available in type defs but not in all event handlers (only where $user is present)
- **Broadcast panel width** in theater mode vs split mode — exact dimensions not verified
- **Shortcuts** limit of 1 per app — confirmed but "to be expanded" is vague

---

## Wiki Page Discovery Stats

- **Total pages discovered**: 55
- **Content extracted from JS bundle**: 55 (100%)
- **Search engine accessible**: 3
- **Behind auth wall**: 52
- **API object pages**: 12 + 25 method sub-pages
- **Event handler pages**: 10
- **Feature/guide pages**: 8
