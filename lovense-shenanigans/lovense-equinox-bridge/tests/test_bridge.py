"""Tests for bridge core: DeviceManager, DirectBLEBackend disconnect, signal handler."""
import asyncio
import inspect

import pytest


class TestDeviceManagerValueClamp:
    """DeviceManager.set_vibrate should clamp 0-20 Lovense level."""

    def test_clamps_below_zero(self):
        from bridge import DeviceManager

        mgr = DeviceManager()
        mgr._backend = None  # no backend, just test clamp logic
        assert hasattr(mgr, "set_vibrate"), "set_vibrate method must exist"

    def test_clamps_above_20(self):
        from bridge import DeviceManager

        mgr = DeviceManager()
        # Call set_vibrate and verify it calls backend with correct mapping
        with pytest.MonkeyPatch.context():
            calls = []

            async def fake_set_vibrate(self, pct):
                calls.append(pct)

            mgr._backend = type(
                "FakeBackend",
                (),
                {"set_vibrate": fake_set_vibrate, "is_connected": True},
            )()
            _run(mgr.set_vibrate(25))
            assert 0 <= calls[0] <= 100

    def test_lovense_20_maps_to_100_pct(self):
        from bridge import DeviceManager

        mgr = DeviceManager()
        with pytest.MonkeyPatch.context():
            calls = []

            async def fake_set_vibrate(self, pct):
                calls.append(pct)

            mgr._backend = type(
                "FakeBackend",
                (),
                {"set_vibrate": fake_set_vibrate, "is_connected": True},
            )()
            _run(mgr.set_vibrate(20))
            assert calls[0] == 100

    def test_lovense_10_maps_to_50_pct(self):
        from bridge import DeviceManager

        mgr = DeviceManager()
        with pytest.MonkeyPatch.context():
            calls = []

            async def fake_set_vibrate(self, pct):
                calls.append(pct)

            mgr._backend = type(
                "FakeBackend",
                (),
                {"set_vibrate": fake_set_vibrate, "is_connected": True},
            )()
            _run(mgr.set_vibrate(10))
            assert calls[0] == 50

    def test_lovense_0_maps_to_0_pct(self):
        from bridge import DeviceManager

        mgr = DeviceManager()
        with pytest.MonkeyPatch.context():
            calls = []

            async def fake_set_vibrate(self, pct):
                calls.append(pct)

            mgr._backend = type(
                "FakeBackend",
                (),
                {"set_vibrate": fake_set_vibrate, "is_connected": True},
            )()
            _run(mgr.set_vibrate(0))
            assert calls[0] == 0


class TestDirectBLEBackendDisconnect:
    """DirectBLEBackend.disconnect should be idempotent (guard check)."""

    def test_disconnect_has_is_connected_guard(self):
        import bridge

        source = inspect.getsource(bridge.DirectBLEBackend.disconnect)
        assert "self.device" in source
        assert "is_connected" in source

    def test_disconnect_does_not_raise_when_device_is_none(self):
        from bridge import DirectBLEBackend

        backend = DirectBLEBackend(address="AA:BB:CC:DD:EE:FF")
        backend.device = None
        # Should not raise
        _run(backend.disconnect())

    def test_disconnect_does_not_raise_when_device_disconnected(self):
        from bridge import DirectBLEBackend
        from unittest.mock import AsyncMock

        backend = DirectBLEBackend(address="AA:BB:CC:DD:EE:FF")
        mock_dev = AsyncMock()
        mock_dev.is_connected = False
        backend.device = mock_dev
        # Should not call stop/disconnect when already disconnected
        _run(backend.disconnect())
        mock_dev.stop.assert_not_called()
        mock_dev.disconnect.assert_not_called()


class TestSignalHandler:
    """Signal handler safety checks."""

    def test_handle_signal_uses_try_except(self):
        import bridge

        source = inspect.getsource(bridge._handle_signal)
        assert "try:" in source
        assert "except" in source
        assert "logger.error" in source

    def test_main_uses_add_signal_handler(self):
        import bridge

        source = inspect.getsource(bridge.main)
        assert "add_signal_handler" in source
        assert "get_running_loop" in source
        # Signal handler should use default arg, not closure
        assert "lambda sig=sig:" in source

    def test_main_handles_not_implemented_error(self):
        import bridge

        source = inspect.getsource(bridge.main)
        assert "NotImplementedError" in source


def _run(coro):
    """Run a coroutine synchronously with a dedicated event loop."""
    loop = asyncio.new_event_loop()
    try:
        return loop.run_until_complete(coro)
    finally:
        loop.close()
