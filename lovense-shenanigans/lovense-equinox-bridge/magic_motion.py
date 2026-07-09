"""
Magic Motion BLE protocol implementation for Equinox and compatible devices.

Protocol documentation from buttplug.io/stpihkal/protocols/magic-motion:
  Service UUID:    78667579-7b48-43db-b8c5-7928a6b0a335
  Write Char UUID: 78667579-a914-49a4-8333-aa3c0cd8fedc

Single vibration command (one motor):
  bytes: 04 08 <intensity> 64 00
  intensity: 0x00 (off) to 0x64 (100%)
  do not exceed 0x64 (100)

Two-motor command (Equinox has one motor, but protocol supports two):
  10 ff 04 0a <v1> <v1> 00 04 08 <v2> 64 00 04 08 <v3> 64 01
  v1, v2, v3 all 0x00-0x64

Multi-argument command structure:
  0b ff 04 <v2> <v2> 0a 00 04 08 <v1> 64 00
"""

import asyncio
import logging
from bleak import BleakScanner, BleakClient

logger = logging.getLogger("magic_motion")

MAGIC_MOTION_SERVICE_UUID = "78667579-7b48-43db-b8c5-7928a6b0a335"
MAGIC_MOTION_WRITE_UUID = "78667579-a914-49a4-8333-aa3c0cd8fedc"
LOVENSE_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
LOVENSE_TX_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"

MAX_INTENSITY = 100


def build_vibrate_command(intensity: int) -> bytes:
    clamped = max(0, min(MAX_INTENSITY, intensity))
    return bytes([0x04, 0x08, clamped, 0x64, 0x00])


class MagicMotionDevice:
    def __init__(self, address: str = None, name_filter: str = None):
        self.address = address
        self.name_filter = name_filter
        self.client = None
        self._write_char = None

    async def discover(self, timeout: float = 10.0) -> list:
        logger.info("Scanning for Magic Motion devices...")
        devices = await BleakScanner.discover(timeout=timeout)
        candidates = []
        for d in devices:
            name = d.name or ""
            if any(kw in name.lower() for kw in ["equinox", "magic", "motion", "solstice", "awaken", "ponder"]):
                candidates.append(d)
                logger.info(f"  Found candidate: {d.name} ({d.address})")
            elif d.address and self.address and d.address.lower() == self.address.lower():
                candidates.append(d)
                logger.info(f"  Found by address: {d.name} ({d.address})")
        return candidates

    async def connect(self, address: str = None) -> bool:
        addr = address or self.address
        if not addr:
            raise ValueError("No device address provided")
        self.client = BleakClient(addr)
        await self.client.connect(timeout=15.0)
        logger.info(f"Connected to {addr}")

        for service in self.client.services:
            if service.uuid.lower() == MAGIC_MOTION_SERVICE_UUID.lower():
                for char in service.characteristics:
                    if char.uuid.lower() == MAGIC_MOTION_WRITE_UUID.lower():
                        self._write_char = char
                        logger.info(f"Found write characteristic: {char.uuid}")
                        return True

        logger.warning("Could not find Magic Motion service via UUID, trying service discovery...")
        for service in self.client.services:
            logger.info(f"  Service: {service.uuid}")
            for char in service.characteristics:
                props = char.properties
                logger.info(f"    Char: {char.uuid} props={props}")
                if "write" in props or "write-without-response" in props:
                    self._write_char = char
                    logger.info(f"Using fallback write char: {char.uuid}")
                    return True

        raise RuntimeError("Could not find suitable write characteristic")

    async def send_vibrate(self, intensity: int):
        if not self.client or not self.client.is_connected:
            raise RuntimeError("Not connected")
        cmd = build_vibrate_command(intensity)
        logger.info(f"Sending vibrate: intensity={intensity} cmd={cmd.hex()}")
        await self.client.write_gatt_char(self._write_char.uuid, cmd, response=False)

    async def stop(self):
        await self.send_vibrate(0)

    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            logger.info("Disconnected")

    async def get_battery(self) -> int:
        return 100  # Equinox doesn't expose battery via standard protocol

    @property
    def is_connected(self) -> bool:
        return self.client is not None and self.client.is_connected
