# Lovense Equinox Bridge — User Manual

Turn tips from your cam shows into real vibrations on your Magic Motion Equinox toy — no expensive Lovense hardware needed.

## What It Does

The Lovense Equinox Bridge sits between your cam site and your Equinox toy:

```
Cam site / app  →  Bridge (port 20010)  →  BLE  →  Equinox
```

When a viewer tips, the cam site tells the bridge, and the bridge tells your Equinox to vibrate — at a strength that matches the tip amount.

Works with any cam site or app that supports Lovense toys (Chaturbate, Stripchat, BongaCams, and many more).

---

## What You Need

### Hardware
- **Magic Motion Equinox** (or other Magic Motion-compatible toy)
- A computer with **Bluetooth Low Energy (BLE)** — most laptops have this built-in
- USB charger for the Equinox

### Software
- **Linux** (tested on Arch, Ubuntu, Debian)
- **Python 3.10 or newer**
- **BLE permissions** (one-time setup, covered below)

---

## Installation

### Method 1: Quick Install (recommended)

Open a terminal and run:

```bash
cd ~/Documents/Projects/lovense-shenanigans
./install-lovense-bridge.zsh
```

This will:
- Install Python dependencies (`aiohttp`, `bleak`, `buttplug`)
- Copy the bridge files to `~/Documents/lovense-equinox-bridge/`
- Set up BLE permissions (needed to access Bluetooth)
- Create a systemd service (so the bridge auto-starts)
- Add a `lovense-bridge` command alias to your shell

### Method 2: Manual Install

```bash
# 1. Install Python dependencies
pip install aiohttp bleak buttplug

# 2. Copy bridge files
cp -r lovense-equinox-bridge ~/Documents/lovense-equinox-bridge

# 3. Set up BLE permissions (so you don't need sudo)
sudo setcap cap_net_raw,cap_net_admin+eip $(which python3)

# 4. Add yourself to the bluetooth group (log out and back in after)
sudo usermod -aG bluetooth $USER
```

---

## Running the Bridge

### Quick Start

```bash
lovense-bridge --mode ble
```

Or if you haven't installed the alias:

```bash
cd ~/Documents/lovense-equinox-bridge
python3 bridge.py --mode ble
```

The bridge will:
1. Scan for your Equinox via Bluetooth
2. Connect automatically
3. Start the Lovense API server on `http://127.0.0.1:20010`

### Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--mode` | `ble` | Connection mode: `ble` (direct Bluetooth) or `buttplug` (via Intiface Central) |
| `--address` | auto | BLE MAC address of your Equinox (skip auto-discovery) |
| `--host` | `127.0.0.1` | IP address the API server listens on |
| `--port` | `20010` | Port the API server listens on |
| `--ws-url` | `ws://127.0.0.1:12345` | Intiface Central WebSocket URL (buttplug mode only) |

### Examples

```bash
# Auto-discover and connect
python3 bridge.py --mode ble

# Connect to a specific Equinox (find its address first)
python3 bridge.py --mode ble --address AA:BB:CC:DD:EE:FF

# Listen on all network interfaces (for other devices on your network)
python3 bridge.py --mode ble --host 0.0.0.0

# Use Buttplug.io via Intiface Central
python3 bridge.py --mode buttplug
```

### Finding Your Local IP

If you need to enter the bridge address in a cam site's settings:

```bash
# Method 1
hostname -I

# Method 2
ip addr | grep 'inet '
```

Look for something like `192.168.1.xxx` — that's your local IP.

---

## Connecting to Cam Sites

There are four ways to connect the bridge to your cam shows.

### Option A: Lovense Game Mode (easiest, no browser extension needed)

Some cam sites let you enter a custom Lovense Game Mode server. If your site supports it:

1. Go to your Lovense settings on the cam site
2. Find the **Game Mode** or **Lovense Server** option
3. Enter: `http://YOUR_IP:20010` (replace `YOUR_IP` with your computer's local IP)
4. Save — the site will now send tips directly to your bridge

Works on: Chaturbate (Game Mode), sites with Lovense API integration.

### Option B: Userscript (for sites with Lovense Browser Extension)

If your cam site uses the Lovense Cam Extension (Chrome extension):

1. **Install Tampermonkey** — a browser extension that runs user scripts:
   - [Chrome: Tampermonkey](https://chrome.google.com/webstore/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo)
   - [Firefox: Tampermonkey](https://addons.mozilla.org/en-US/firefox/addon/tampermonkey/)

2. **Install the bridge userscript**:
   - Open Tampermonkey → **Create a new script**
   - Delete all the template code
   - Copy the entire contents of `lovense-bridge.user.js` into the editor
   - Save (Ctrl+S or Cmd+S)

3. **Start the bridge** before opening your cam site

4. The script intercepts the Lovense extension on these sites automatically:
   - chaturbate.com
   - stripchat.com
   - bongacams.com
   - cams.com
   - cam4.com
   - myfreecams.com
   - livejasmin.com
   - streamate.com
   - camsoda.com
   - xmodels.com

### Option C: XToys.app (easiest overall)

XToys.app natively supports Magic Motion devices including the Equinox. You don't need this bridge at all if you use XToys.

1. Go to [XToys.app](https://xtoys.app)
2. Connect your Equinox via Bluetooth
3. Use XToys with your cam site

### Option D: OBS Toolset / Streamster

Streamster has native Bluetooth toy support that may work directly with the Equinox. Try connecting via Streamster's **Toys** tab.

---

## How Tipping Works

When a viewer tips, the flow is:

```
Viewer tips $5
  → Cam site tells Lovense extension: "receiveTip($5, tipper)"
  → Userscript intercepts and calculates vibration level
  → Sends to bridge: POST /command {"Function", "Vibrate:10"}
  → Bridge sends BLE command to Equinox
  → Toy vibrates at 50% intensity for 5 seconds
```

### Tip-to-Vibration Mapping

The userscript maps tip amounts to vibration levels (1–20):

**Default mapping** (no model settings configured):

| Tip Amount | Vibration Level | Intensity |
|------------|----------------|-----------|
| $1 | 1 | 5% |
| $5 | 5 | 25% |
| $10 | 10 | 50% |
| $20 | 20 (max) | 100% |

Formula: `level = max(1, min(20, round(amount / 10)))`

**With model settings**: If you've configured vibration levels in the Lovense extension, the userscript reads those settings and maps tips to your preferred levels.

### Debug Console Helpers

With the userscript active, open your browser's developer console (F12) and use:

```js
// Test the bridge with a 2-second vibe at level 10
window.__lovenseBridge.test()

// Send a custom vibration: level 15 for 3 seconds
window.__lovenseBridge.sendVibrate(15, 3)

// Stop immediately
window.__lovenseBridge.stop()

// Change the bridge URL (if running on a different machine)
window.__lovenseBridge.setBridgeUrl('http://192.168.1.100:20010/command')
```

You should see `[LovenseBridge] Bridge connected` in the console when the bridge is running.

---

## API Reference

The bridge speaks the Lovense Standard API on `http://127.0.0.1:20010/command`.

### Test the Bridge

```bash
curl http://127.0.0.1:20010/
```
→ `{"status": "Lovense Equinox Bridge", "version": "1.0.0"}`

### Get Connected Toys

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"GetToys"}'
```

### Vibrate

```bash
# Vibrate at level 10 (0-20) for 5 seconds
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Function","action":"Vibrate:10","timeSec":5,"apiVer":1}'
```

### Stop

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Function","action":"Stop","timeSec":0,"apiVer":1}'
```

### Play a Preset Pattern

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Preset","name":"wave","timeSec":10}'
```

Available presets: `pulse`, `wave`, `fireworks`, `earthquake`

### Custom Pattern

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Pattern","strength":"20;50;80;100;80;50;20","timeSec":10}'
```

### Check Battery

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Battery"}'
```

### API Quick Reference

| Command | Action | Example |
|---------|--------|---------|
| `GetToys` | — | `{"command":"GetToys"}` |
| `Function` | `Vibrate:N` (0-20) | `{"command":"Function","action":"Vibrate:10"}` |
| `Function` | `Stop` | `{"command":"Function","action":"Stop"}` |
| `Preset` | `pulse`/`wave`/`fireworks`/`earthquake` | `{"command":"Preset","name":"wave"}` |
| `Pattern` | custom strength list | `{"command":"Pattern","strength":"50;100;50"}` |
| `Battery` | — | `{"command":"Battery"}` |

---

## Install as a System Service

Make the bridge start automatically when you log in:

```bash
# Copy the service file
sudo cp ~/Documents/Projects/.omo/deploy/lovense-equinox-bridge.service \
  /etc/systemd/system/

# Enable and start
systemctl --user enable lovense-equinox-bridge
systemctl --user start lovense-equinox-bridge

# Check status
systemctl --user status lovense-equinox-bridge
```

The service will:
- Start after Bluetooth is ready
- Restart automatically if it crashes
- Log to the system journal (`journalctl --user -u lovense-equinox-bridge`)

---

## Troubleshooting

### "No Magic Motion devices found"
```
ERROR: No Magic Motion devices found. Try specifying --address
```

**Causes and fixes:**
- **Equinox not in pairing mode**: Hold the power button until the LED blinks rapidly
- **Equinox battery dead**: Charge for 30 minutes and try again
- **No BLE permissions**: Run `sudo setcap cap_net_raw,cap_net_admin+eip $(which python3)` then try again
- **Still not working**: Try with `sudo` once to verify: `sudo python3 bridge.py --mode ble`

### "Bridge unreachable" in browser console

The userscript shows `[LovenseBridge] Bridge not running` or `Bridge unreachable`.

- Make sure the bridge is running: `curl http://127.0.0.1:20010/`
- If running on a different port/host, update the bridge URL in the console:
  ```js
  window.__lovenseBridge.setBridgeUrl('http://YOUR_IP:PORT/command')
  ```
- HTTPS mixed content: `http://127.0.0.1` should work from HTTPS sites in modern browsers. If not, try running with `--host 0.0.0.0` and using `http://localhost:20010`

### Userscript not intercepting tips

- Is Tampermonkey installed and enabled?
- Is the script active? Check Tampermonkey's dashboard
- Open the browser console (F12) — you should see `[LovenseBridge] Interceptor installed`
- The cam site might use a different Lovense API than what the script expects

### Port already in use

```
OSError: [Errno 98] Address already in use
```

Something else is using port 20010. Either stop that service or use a custom port:

```bash
python3 bridge.py --port 20011
```

Then update your cam site settings or userscript to use port 20011.

### Equinox disconnects during use

- Move the Equinox closer to your computer (BLE range is ~10 meters)
- Check battery level
- USB/Bluetooth interference — try a USB extension cable for your Bluetooth dongle

### Bridge starts but no vibration

Test the bridge directly:

```bash
curl -X POST http://127.0.0.1:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Function","action":"Vibrate:20","timeSec":5}'
```

If this works but the cam site doesn't trigger vibration, the issue is in the connection between the cam site and the bridge, not the bridge itself.

---

## Development & Testing

### Run Tests

```bash
cd ~/Documents/lovense-equinox-bridge
python -m pytest tests/ -v
```

48 tests covering all bridge functionality.

### Code Style

```bash
ruff check .
```

Zero warnings expected.

### Project Structure

```
lovense-equinox-bridge/
├── bridge.py              # Main entry point, backends, signal handling
├── lovense_api.py         # Lovense API server (port 20010)
├── magic_motion.py        # BLE protocol for Magic Motion devices
├── lovense-bridge.user.js # Tampermonkey userscript
├── requirements.txt       # Python dependencies
├── tests/                 # Pytest test suite
│   ├── test_bridge.py
│   ├── test_lovense_api.py
│   └── test_magic_motion.py
├── test_fixes.py          # Regression tests for hardening fixes
├── USER_MANUAL.md         # This document
└── README.md              # Quick-start guide
```

---

*Bridge version 1.0.0 — Built for the Magic Motion Equinox. Works with any Lovense-compatible cam site or app.*
