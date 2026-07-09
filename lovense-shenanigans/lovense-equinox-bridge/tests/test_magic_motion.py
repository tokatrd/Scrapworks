"""Tests for MagicMotion BLE protocol command bytes construction."""

import asyncio
import inspect

import pytest


class TestBuildVibrateCommand:
    """build_vibrate_command should produce correct byte sequences."""

    def test_zero_intensity(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(0)
        assert cmd == bytes([0x04, 0x08, 0x00, 0x64, 0x00])

    def test_full_intensity(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(100)
        assert cmd == bytes([0x04, 0x08, 0x64, 0x64, 0x00])

    def test_mid_intensity(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(50)
        assert cmd == bytes([0x04, 0x08, 0x32, 0x64, 0x00])

    def test_clamps_above_100(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(150)
        assert cmd == bytes([0x04, 0x08, 0x64, 0x64, 0x00])

    def test_clamps_below_0(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(-10)
        assert cmd == bytes([0x04, 0x08, 0x00, 0x64, 0x00])

    def test_intensity_25(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(25)
        assert cmd == bytes([0x04, 0x08, 0x19, 0x64, 0x00])

    def test_returns_bytes(self):
        from magic_motion import build_vibrate_command
        cmd = build_vibrate_command(42)
        assert isinstance(cmd, bytes)
        assert len(cmd) == 5

    def test_multiple_calls_independent(self):
        """Calling build_vibrate_command multiple times should not mutate state."""
        from magic_motion import build_vibrate_command
        cmd1 = build_vibrate_command(50)
        cmd2 = build_vibrate_command(75)
        cmd3 = build_vibrate_command(50)
        assert cmd1 == cmd3
        assert cmd1 != cmd2


class TestMagicMotionDevice:
    """MagicMotionDevice class invariants."""

    def test_has_required_uuids(self):
        from magic_motion import (
            MAGIC_MOTION_SERVICE_UUID,
            MAGIC_MOTION_WRITE_UUID,
            MAX_INTENSITY,
        )
        assert MAGIC_MOTION_SERVICE_UUID == "78667579-7b48-43db-b8c5-7928a6b0a335"
        assert MAGIC_MOTION_WRITE_UUID == "78667579-a914-49a4-8333-aa3c0cd8fedc"
        assert MAX_INTENSITY == 100

    def test_connect_has_timeout(self):
        from magic_motion import MagicMotionDevice
        source = inspect.getsource(MagicMotionDevice.connect)
        assert "timeout=15.0" in source

    def test_disconnect_guard(self):
        from magic_motion import MagicMotionDevice
        source = inspect.getsource(MagicMotionDevice.disconnect)
        assert "is_connected" in source
        assert "self.client" in source

    def test_send_vibrate_raises_when_not_connected(self):
        from magic_motion import MagicMotionDevice
        device = MagicMotionDevice(address="AA:BB:CC:DD:EE:FF")
        loop = asyncio.new_event_loop()
        try:
            with pytest.raises(RuntimeError, match="Not connected"):
                loop.run_until_complete(device.send_vibrate(50))
        finally:
            loop.close()


@pytest.mark.asyncio
async def test_magic_motion_device_connect_defaults():
    """Verify MagicMotionDevice can be constructed with defaults."""
    from magic_motion import MagicMotionDevice
    device = MagicMotionDevice()
    assert device.address is None
    assert device.name_filter is None
    assert device.client is None
    assert device._write_char is None
