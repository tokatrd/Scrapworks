#!/usr/bin/env python3
"""
Lovense Equinox Bridge
======================
Bridges the Lovense ecosystem to Magic Motion (Equinox) devices.

Two operation modes:
  1. Direct BLE: Connects to the Equinox directly via Bluetooth
  2. Buttplug: Connects via Intiface Central (buttplug.io)

The bridge implements the Lovense Game Mode (Standard) API on port 20010,
so any app that supports Lovense toys can control the Equinox instead.
"""

import asyncio
import json
import logging
import signal
import sys
import argparse
from typing import Optional
from aiohttp import web

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
logger = logging.getLogger("bridge")


class DeviceManager:
    def __init__(self):
        self.current_intensity = 0
        self.battery_level = 100
        self._backend = None
        self.toy_id = "equinox-bridge-001"
        self.toy_name = "Equinox"

    def set_backend(self, backend):
        self._backend = backend

    def get_toy_info(self) -> Optional[dict]:
        if self._backend and self._backend.is_connected:
            return {
                "id": self.toy_id,
                "name": self.toy_name,
                "battery": self.battery_level,
                "connected": True,
            }
        return None

    async def set_vibrate(self, level: int):
        lovense_level = max(0, min(20, level))
        self.current_intensity = lovense_level
        if self._backend:
            pct = int((lovense_level / 20.0) * 100)
            await self._backend.set_vibrate(pct)
            logger.info(f"Vibrate: Lovense level={lovense_level}/20 -> {pct}%")


class DirectBLEBackend:
    def __init__(self, address: str = None):
        self.address = address
        self.device = None
        self._connected = False

    async def connect(self):
        from magic_motion import MagicMotionDevice

        self.device = MagicMotionDevice(address=self.address)

        if not self.address:
            candidates = await self.device.discover(timeout=15)
            if not candidates:
                logger.error("No Magic Motion devices found. Try specifying --address")
                return False
            logger.info(f"Found {len(candidates)} device(s):")
            for i, d in enumerate(candidates):
                logger.info(f"  [{i}] {d.name} ({d.address})")
            if len(candidates) == 1:
                chosen = candidates[0]
            else:
                logger.info(f"Auto-selecting first device: {candidates[0].name}")
                chosen = candidates[0]
            self.address = chosen.address

        logger.info(f"Connecting to {self.address}...")
        success = await self.device.connect(self.address)
        self._connected = success
        return success

    async def set_vibrate(self, pct: int):
        if self._connected and self.device:
            await self.device.send_vibrate(pct)

    async def disconnect(self):
        if self.device:
            await self.device.stop()
            await self.device.disconnect()
        self._connected = False

    @property
    def is_connected(self) -> bool:
        return self._connected and self.device is not None and self.device.is_connected


class ButtplugBackend:
    def __init__(self, ws_url: str = "ws://127.0.0.1:12345"):
        self.ws_url = ws_url
        self.client = None
        self._device = None
        self._connected = False

    async def connect(self):
        try:
            from buttplug import ButtplugClient, DeviceOutputCommand, OutputType
        except ImportError:
            logger.error("buttplug library not installed: pip install buttplug")
            return False

        self.client = ButtplugClient("Lovense Equinox Bridge")
        logger.info(f"Connecting to Intiface Central at {self.ws_url}...")
        await self.client.connect(self.ws_url)
        logger.info("Connected to Intiface Central")

        added_event = asyncio.Event()

        async def on_device_added(device):
            logger.info(f"Device found: {device.name}")
            logger.info(f"  Address: {device.address}")
            if "equinox" in device.name.lower() or "magic" in device.name.lower() or "motion" in device.name.lower():
                self._device = device
                added_event.set()
            else:
                logger.info(f"  Device '{device.name}' doesn't match Equinox, "
                            f"but will use it anyway if it has vibrate capability")
                if device.has_output(OutputType.VIBRATE):
                    self._device = device
                    added_event.set()

        self.client.on_device_added = on_device_added

        await self.client.start_scanning()
        logger.info("Scanning for devices (15 seconds)...")
        try:
            await asyncio.wait_for(added_event.wait(), timeout=15.0)
        except asyncio.TimeoutError:
            logger.warning("No compatible device found during scan")
        await self.client.stop_scanning()

        if self._device:
            self._connected = True
            logger.info(f"Using device: {self._device.name}")
            return True
        else:
            logger.warning("No Magic Motion/Equinox devices detected via Intiface")
            logger.info("Make sure Intiface Central is running and the Equinox is in pairing mode")
            return False

    async def set_vibrate(self, pct: int):
        if not self._connected or not self._device:
            return
        try:
            from buttplug import DeviceOutputCommand, OutputType
            normalized = pct / 100.0
            normalized = max(0.0, min(1.0, normalized))
            await self._device.run_output(DeviceOutputCommand(OutputType.VIBRATE, normalized))
        except Exception as e:
            logger.error(f"Buttplug command failed: {e}")

    async def disconnect(self):
        self._connected = False
        if self.client:
            try:
                await self.client.disconnect()
            except Exception:
                pass

    @property
    def is_connected(self) -> bool:
        return self._connected


async def main():
    parser = argparse.ArgumentParser(
        description="Lovense Equinox Bridge - use your Magic Motion Equinox with Lovense software"
    )
    parser.add_argument("--mode", choices=["ble", "buttplug"], default="ble",
                        help="Connection backend (default: ble)")
    parser.add_argument("--address", type=str, default=None,
                        help="BLE MAC address of the Equinox (omit for auto-discovery)")
    parser.add_argument("--ws-url", type=str, default="ws://127.0.0.1:12345",
                        help="Intiface Central WebSocket URL (default: ws://127.0.0.1:12345)")
    parser.add_argument("--port", type=int, default=20010,
                        help="Lovense API server port (default: 20010)")
    parser.add_argument("--host", type=str, default="0.0.0.0",
                        help="Lovense API server host (default: 0.0.0.0)")
    args = parser.parse_args()

    device_mgr = DeviceManager()

    if args.mode == "ble":
        backend = DirectBLEBackend(address=args.address)
    else:
        backend = ButtplugBackend(ws_url=args.ws_url)

    device_mgr.set_backend(backend)

    connected = await backend.connect()
    if not connected:
        logger.error(f"Failed to connect using {args.mode} backend")
        logger.info("Troubleshooting:")
        if args.mode == "ble":
            logger.info("  1. Make sure your Equinox is turned on and in pairing mode")
            logger.info("  2. Try specifying the BLE address with --address")
            logger.info("  3. On Linux, try: sudo hcitool lescan")
        else:
            logger.info("  1. Make sure Intiface Central is running")
            logger.info("  2. Check the WebSocket URL (default: ws://127.0.0.1:12345)")
            logger.info("  3. Make sure your Equinox is in pairing mode")
            logger.info("  4. Enable device scanning in Intiface Central")
        sys.exit(1)

    logger.info(f"=== Lovense Equinox Bridge Ready ===")
    logger.info(f"Listening on http://{args.host}:{args.port}/command")
    logger.info(f"Point your Lovense-compatible app to this address")
    logger.info("")
    logger.info("Integration paths for camming:")
    logger.info("  1. Cam sites with 'Lovense Game Mode' option: enter this server's IP:port")
    logger.info("  2. Lovense Cam Extension: see README for proxy/userscript approach")
    logger.info("  3. XToys.app: already supports Equinox directly")
    logger.info("")
    logger.info("Press Ctrl+C to stop")

    from lovense_api import LovenseAPIServer
    server = LovenseAPIServer(device_mgr, host=args.host, port=args.port)

    stop_event = asyncio.Event()

    async def shutdown():
        logger.info("Shutting down...")
        await backend.disconnect()
        stop_event.set()

    try:
        runner = web.AppRunner(server.app)
        await runner.setup()
        site = web.TCPSite(runner, args.host, args.port)
        await site.start()
        logger.info(f"Lovense API server running on http://{args.host}:{args.port}")

        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                asyncio.get_event_loop().add_signal_handler(
                    sig, lambda: asyncio.create_task(shutdown())
                )
            except NotImplementedError:
                pass

        await stop_event.wait()
    except KeyboardInterrupt:
        pass
    finally:
        await backend.disconnect()
        await runner.cleanup()
        logger.info("Bridge stopped")


async def shutdown_wrapper(stop_event):
    logger.info("Shutdown signal received")
    stop_event.set()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
