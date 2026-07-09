# Lovense Equinox Bridge

Use your **Magic Motion Equinox** (or other Magic Motion toys) with the Lovense Cam Extension and Lovense-compatible software, without modifying any hardware.

## How It Works

```
Cam Site / App  →  Lovense Game Mode API  →  Bridge  →  BLE  →  Equinox
                        (port 20010)
```

The bridge implements the **Lovense Standard API (Game Mode)** locally. Any app that supports Lovense toys can send commands to this bridge, which translates them to the Magic Motion BLE protocol and controls your Equinox.

## Quick Start

### 1. Install

```bash
pip install -r requirements.txt
```

### 2. Run (Direct BLE Mode)

```bash
# Auto-discover your Equinox
python bridge.py --mode ble

# Or specify the BLE address directly
python bridge.py --mode ble --address XX:XX:XX:XX:XX:XX
```

### 3. Run (Buttplug.io Mode)

```bash
# First, start Intiface Central (https://intiface.com/central/)
# Then run:
python bridge.py --mode buttplug
```

## Integration with Cam Software

### Option A: Cam sites with Lovense Game Mode support

Some cam sites let you enter a custom **Game Mode server IP**. Configure yours to point to:

```
http://YOUR_COMPUTER_IP:20010
```

Find your IP with: `ip addr` (Linux), `ipconfig` (Windows), or `ifconfig` (Mac).

### Option B: Lovense Cam Extension (via userscript)

If your cam site uses the Lovense Cam Extension, install the **Tampermonkey** userscript. It intercepts the `CamExtension` and `lovense` JavaScript objects and redirects commands to the bridge.

1. Install the [Tampermonkey](https://www.tampermonkey.net/) browser extension
2. Open Tampermonkey → **Create a new script**
3. Replace the default template with the contents of [`lovense-bridge.user.js`](lovense-bridge.user.js)
4. Save (`Ctrl+S`)

The script runs at `document-start` on supported cam sites and intercepts:
- `window.CamExtension` (Cam Extension for Chrome) — proxies constructor and instance methods
- `window.lovense` (Cam Kit for Web) — wraps `receiveTip` and maps tip amounts to vibration levels
- `postMessage` events with Lovense-type commands

Tips are mapped to vibration levels using the model's settings when available, or a default formula (level = tip ÷ 10, clamped 1–20).

A developer console bridge check runs on page load. You can also use these console helpers:
```js
window.__lovenseBridge.test()       // send a 2-second vibe at level 10
window.__lovenseBridge.sendVibrate(15, 3)  // vibe at level 15 for 3s
window.__lovenseBridge.stop()       // stop immediately
```

### Option C: XToys.app (easiest, no bridge needed)

XToys.app natively supports Magic Motion devices including the Equinox. Connect your toy there and use XToys with supported cam sites.

### Option D: OBS Toolset / Streamster

Streamster has native Bluetooth toy support that may work directly with the Equinox. Try connecting via Streamster's "Toys" tab.

## Commands Reference

The bridge accepts these POST requests to `http://localhost:20010/command`:

```bash
# Get connected toys
curl -X POST http://localhost:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"GetToys"}'

# Vibrate at level 10 (0-20)
curl -X POST http://localhost:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Function","action":"Vibrate:10","timeSec":5,"apiVer":1}'

# Stop
curl -X POST http://localhost:20010/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"Function","action":"Stop","timeSec":0,"apiVer":1}'
```

Vibration levels: Lovense uses 0-20. The bridge auto-converts to 0-100% for the Equinox.

## Troubleshooting

**Can't find the Equinox via BLE:**
- Make sure it's charged and powered on (LED should be blinking)
- Run `python -c "from magic_motion import MagicMotionDevice; import asyncio; print(asyncio.run(MagicMotionDevice().discover()))"` to scan
- On Linux, you may need `sudo` or `setcap cap_net_raw,cap_net_admin+eip $(which python3)`

**Bridge connects but cam site doesn't respond:**
- Verify the bridge is running: `curl http://localhost:20010/command -d '{"command":"GetToys"}'`
- Make sure your cam site supports Lovense Game Mode integration
- Try the userscript approach (Option B above)

**Using Buttplug mode:**
- Download and start [Intiface Central](https://intiface.com/central/)
- Enable Bluetooth engine in Intiface settings
- Start scanning in Intiface before launching the bridge
