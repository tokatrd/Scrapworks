# Hyperplan: lovense-shenanigans — Execution Plan

## Context

Monorepo at `/home/toka/Documents/Projects` with three workstreams:

1. **Lovense Equinox Bridge** (Python, bleak + aiohttp): Cross-platform BLE bridge. Code is functional but has hardening gaps. The primary actionable workstream.
2. **Oscar X7 Editor** (Borland C++ Builder binary, Win32 PE): Closed-source, zero migration path. Only viable option is Wine compatibility testing.
3. **Lovense OBS Toolset** (C++/Python): 3 components already Linux-converted, 3 structurally Windows-only (DirectShow/COM/CefSharp), 1 borderline (smart-object).

### Codebase State
- Zero tests exist anywhere
- `install-lovense-bridge.zsh:5` — `PROJECT_SRC=/tmp/opencode/lovense-equinox-bridge` (wrong path)
- `magic_motion.py:66` — `BleakClient.connect()` has no timeout for infinite, default to 15s.
- `bridge.py:252-258` — signal handler uses mutable lambda + `asyncio.create_task` in signal context (unsafe — should use `get_running_loop()` and avoid mutable captures)
- `lovense_api.py:142,170` — `_play_pattern` pattern tasks fire-and-forget, no cancellation on Stop
- `lovense-bridge.user.js:25` — `BRIDGE_URL` declared `const` but `setBridgeUrl` on line 210 reassigns it (should be `let`)
- `bridge.py:99-100` — `DirectBLEBackend.disconnect()` no double-disconnect guard
- `bridge.py:196` — `--host` defaults to `0.0.0.0` instead of `127.0.0.1`
- `lovense_api.py:25` — `LovenseAPIServer.__init__` accepts `host` param, verify it passes through from args properly

---

## Decision Points (Blocking Questions, Logged for User)

**User is AFK — these blocking decisions are logged below. Work proceeds on unblocked packages.**

| # | Question | Options | Recommendation | Impact |
|---|----------|---------|----------------|--------|
| D1 | **Bridge scope: minimal hardening vs production reconnect?** | (a) Minimal 4h, (b) Production 12h+ with reconnect FSM | (a) Minimal — no scope creep | WP1 blocked |
| D2 | **Oscar X7: which path?** | (a) Wine test, (b) RE HID protocol, (c) abandon | (a) Wine test (2h), (c) abandon if fails | WP6 blocked |
| D3 | **lovense-smart-object: include?** | (a) Port assessment, (b) skip | (a) Worth evaluating — deps Linux-ready | WP4 blocked |
| D4 | **IPC on Linux: needed?** | (a) Extract schema only, (b) skip entirely | (a) Extract schema/documentation | WP5 blocked |

**Logged at**: `.omo/logs/blocking-questions.md` (created during execution)

---

## Task Dependency Graph

| Task | Depends On | Reason |
|------|------------|--------|
| T1 | None | Starting point |
| T2 | None | Self-contained Python fixes |
| T3 | T2 | Shares bridge file layout knowledge |
| T4 | T2 | Requires fixed bridge code to mock |
| T5 | T2 | Service management for deployed bridge |
| T6 | None | Independent binary execution test |
| T7 | None | Independent dep assessment |
| T8 | None | Independent code reading |
| T9 | T2–T8 | Consolidates all findings |
| T10 | T4, T5, T6, T7, T8, T9 | End-to-end QA gate |

---

## Parallel Execution Graph

**Wave 1** (Start immediately — no dependencies):
```
├── T1: Verify environment + isolate Oscar binary     [unspecified-low]
├── T2: Bridge hardening (core fixes)                  [programming]
├── T6: Wine Oscar test                                 [quick]
├── T7: lovense-smart-object port assessment            [unspecified-high]
├── T8: relay-server schema extraction                  [unspecified-high]
```

**Wave 2** (After T2 completes):
```
├── T3: Fix install script paths                        [quick]
├── T4: Smoke test harness for bridge                   [programming]
├── T5: Bridge systemd unit                             [quick]
```

**Wave 3** (After Waves 1–2 complete):
```
├── T9: Update AGENTS.md with findings                  [writing]
```

**Wave 4** (After T9 completes):
```
├── T10: Final verification + lint                      [quick]
```

**Critical Path**: T2 → T4 → T9 → T10 (~6.5h wall clock)
**Parallel Speedup**: ~60% saved over sequential

---

## Tasks

### T1: Verify Environment + Isolate Oscar Binary
- **What**: Verify Python 3.10+, pip packages (aiohttp, bleak, buttplug), BLE permissions, wine availability. Create `.omo/logs/` directory.
- **Category**: `unspecified-low`
- **Skills**: none
- **Depends**: None
- **QA**: Python 3.10+ confirmed, `pip install -r requirements.txt` works, `wine --version` (or logged absent), Oscar binary identified as PE32

### T2: Bridge Hardening (Core Fixes)
- **What**: Apply 6 fixes to Python bridge code. **Test-first**: write failing assertion before each fix.
  1. `magic_motion.py:66` — `BleakClient.connect(timeout=15.0)`
  2. `bridge.py:252-258` — `get_running_loop().add_signal_handler(...)`, avoid mutable lambda
  3. `lovense_api.py:142,170` — Track pattern Task, cancel on Stop
  4. `bridge.py:99-100` — `if self.client and self.client.is_connected` guard
  5. `lovense-bridge.user.js:25` — `const`→`let`
  6. `bridge.py:196` — default to `127.0.0.1` instead of `0.0.0.0`
- **Category**: `programming`
- **Skills**: [`programming`] — Python async, bleak, aiohttp
- **Depends**: None
- **QA**: Each fix has a corresponding test case proving it works

### T3: Fix Install Script Paths
- **What**: `install-lovense-bridge.zsh:5` change `PROJECT_SRC=/tmp/opencode/lovense-equinox-bridge` → absolute path to `lovense-equinox-bridge/` in repo. Also fix `BRIDGE_DIR` default.
- **Category**: `quick`
- **Skills**: none
- **Depends**: T2
- **QA**: Script copies from correct source dir

### T4: Smoke Test Harness for Bridge
- **What**: `tests/conftest.py` with mock BLE backend. `tests/test_bridge.py` covering: DeviceManager value clamp, DirectBLEBackend.disconnect idempotence, signal handler safety. `tests/test_lovense_api.py` covering: GetToys, Function, Stop, unknown command, pattern cancellation. `tests/test_magic_motion.py` covering: command bytes construction.
- **Category**: `programming`
- **Skills**: [`programming`] — pytest, aiohttp test_client, async mocking
- **Depends**: T2 (needs fixed code)
- **QA**: `pytest tests/ -v` passes. Coverage ≥70% on bridge + lovense_api.

### T5: Bridge systemd Unit
- **What**: Create `.omo/deploy/lovense-equinox-bridge.service` with `After=bluetooth.target`, `Restart=on-failure`, `WantedBy=default.target`.
- **Category**: `quick`
- **Skills**: none
- **Depends**: T2
- **QA**: `systemd-analyze verify` passes, ExecStart points to correct Python

### T6: Wine Oscar Test
- **What**: Install wine if missing (pacman/apt). Run `wine OscarEditor.exe`. Log result: launches, crash, missing DLL. Document the Wine prefix used.
- **Category**: `quick`
- **Skills**: none
- **Depends**: None
- **QA**: Result documented in AGENTS.md: "Works with Wine X.Y" or specific failure with error log

### T7: lovense-smart-object Port Assessment
- **What**: Read CMakeLists.txt, source files, check deps (dlib, OpenCV, ONNX). Determine if buildable on Linux natively or needs structural rewrite.
- **Category**: `unspecified-high`
- **Skills**: [`programming`] — C++/CMake, dlib/OpenCV
- **Depends**: None
- **QA**: Assessment written to AGENTS.md with actionable summary

### T8: relay-server Schema Extraction
- **What**: Read `ipc/rs_ipc_handler.cpp`, `http/rs_http_server.cpp`, `toolset/lovense_toolset_custom.cpp`. Extract IPC message format, HTTP API endpoints, UI string resources. Document for future reimplementation.
- **Category**: `unspecified-high`
- **Skills**: [`programming`] — C++ reading
- **Depends**: None
- **QA**: IPC message types, HTTP routes, and UI string categories documented in AGENTS.md

### T9: Update AGENTS.md
- **What**: Rewrite AGENTS.md with: bridge test/sytemd status, Oscar Wine result, smart-object assessment, relay-server schema reference, install script fix. Update component status table.
- **Category**: `writing`
- **Skills**: none
- **Depends**: T2–T8
- **QA**: All component statuses correct, all workstream findings documented

### T10: Final Verification + Lint
- **What**: `pytest tests/`, `ruff check *.py lovense-equinox-bridge/*.py`, `shellcheck install-lovense-bridge.zsh`. Review diffs for regressions.
- **Category**: `quick`
- **Skills**: none
- **Depends**: T4, T5, T6, T7, T8, T9
- **QA**: All checks pass, git diff reviewed

---

## Commit Strategy

```
commit 1  "fix(bridge): BLE timeout, signal handler, pattern cancel, disconnect guard, BRIDGE_URL const→let"
commit 2  "fix(install): correct source path in install script"
commit 3  "test(bridge): smoke test harness with mock BLE backend"
commit 4  "feat(bridge): systemd user service unit"
commit 5  "docs(oscar): Wine compatibility test results"
commit 6  "docs(smart-object,relay): port assessment and schema"
commit 7  "docs: update AGENTS.md with all workstream status"
commit 8  "chore: final verification — lint, typecheck, test"
```

Style: conventional commits, one commit per logical change, no staging unrelated files.

---

## Success Criteria

1. **Bridge**: All 6 hardening fixes verified by passing tests
2. **Install**: Script copies from correct source path
3. **Tests**: `pytest tests/` passes, coverage ≥70%
4. **Systemd**: `systemd-analyze verify` passes
5. **Oscar**: Compatibility result documented
6. **smart-object**: Port assessment in AGENTS.md
7. **relay-server**: Schema extracted to AGENTS.md
8. **AGENTS.md**: Accurate, current, reflects all states
9. **No regressions**: All checks pass, no broken behavior

---

## Blocking Questions Log

Created at execution start: `.omo/logs/blocking-questions.md`

```
# Blocking Questions (AFK — Answers Needed)

## D1: Bridge Scope
Minimal hardening (4h, no reconnect state machine) vs Production-grade (12h+, reconnect FSM)?
→ Recommendation: Minimal. No scope creep.

## D2: Oscar X7
(a) Wine compatibility test, (b) Reverse-engineer HID protocol for native driver, (c) Abandon
→ Recommendation: (a) Wine test. If fails, (c) abandon.

## D3: lovense-smart-object
Include port assessment? (Deps are Linux-ready — dlib, OpenCV, ONNX)
→ Recommendation: Yes, assess portability.

## D4: IPC on Linux
Extract relay-server schema for future reimplementation, or skip entirely?
→ Recommendation: Extract schema + UI strings only.
```

---

## Estimates

| Task | Est. | Parallelism |
|------|------|-------------|
| T1 | 0.5h | Wave 1 (parallel with T2, T6, T7, T8) |
| T2 | 4h | Wave 1 |
| T3 | 0.5h | Wave 2 (after T2) |
| T4 | 2h | Wave 2 (after T2) |
| T5 | 0.5h | Wave 2 (after T2) |
| T6 | 2h | Wave 1 (parallel) |
| T7 | 4h | Wave 1 (parallel) |
| T8 | 4h | Wave 1 (parallel) |
| T9 | 1h | Wave 3 (after T2–T8) |
| T10 | 0.5h | Wave 4 (after T9) |
| **Total** | **~19h** | |
| **Wall clock** | **~6.5h** | |
