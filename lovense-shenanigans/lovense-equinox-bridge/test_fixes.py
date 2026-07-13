"""Tests for the 6 specific fixes to the Lovense Equinox Bridge."""


import pytest


# ── FIX 1: magic_motion.py line 66 — connect() should pass timeout=15.0 ──


class TestFix1MagicMotionConnectTimeout:
    @pytest.mark.asyncio
    async def test_connect_calls_with_timeout(self):
        from magic_motion import MagicMotionDevice

        device = MagicMotionDevice(address="AA:BB:CC:DD:EE:FF")
        # Don't mock BleakClient.connect at the import level —
        # test by checking the MagicMotionDevice.connect method source
        import inspect
        source = inspect.getsource(device.connect)
        assert "timeout=15.0" in source, (
            "FIX 1 FAILED: MagicMotionDevice.connect() must call "
            "await self.client.connect(timeout=15.0), but got:\n" + source
        )

    @pytest.mark.asyncio
    async def test_connect_does_not_use_bare_connect(self):
        from magic_motion import MagicMotionDevice
        import inspect
        source = inspect.getsource(MagicMotionDevice.connect)
        lines = source.split("\n")
        bare_connect_lines = [
            line.strip() for line in lines
            if "self.client.connect(" in line and "timeout=" not in line
        ]
        assert not bare_connect_lines, (
            f"FIX 1 FAILED: bare self.client.connect() found "
            f"(no timeout=15.0 argument): {bare_connect_lines}"
        )


# ── FIX 2: bridge.py lines 252-258 — use get_running_loop, wrap lambda in try/except ──


class TestFix2RunningLoopAndTryExcept:
    def test_uses_get_running_loop(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.main)
        assert "asyncio.get_running_loop()" in source, (
            "FIX 2 FAILED: must use asyncio.get_running_loop() "
            "instead of asyncio.get_event_loop()"
        )
        assert "asyncio.get_event_loop()" not in source, (
            "FIX 2 FAILED: asyncio.get_event_loop() still present — "
            "should be replaced with asyncio.get_running_loop()"
        )

    def test_lambda_wrapped_in_try_except(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.main)
        assert "try:" in source and "except" in source, (
            "FIX 2 FAILED: signal handler lambda must be wrapped in try/except"
        )
        assert "logger.error" in source or "logging" in source, (
            "FIX 2 FAILED: except block must log the error"
        )


# ── FIX 3: lovense_api.py lines 142,170 — store task ref, cancel existing, cancel on Stop ──


class TestFix3PatternTaskReference:
    def test_has_pattern_task_attribute(self):
        import lovense_api
        server = lovense_api.LovenseAPIServer(None)
        assert hasattr(server, "_pattern_task"), (
            "FIX 3 FAILED: LovenseAPIServer instance must have _pattern_task attribute"
        )

    def test_pattern_task_stored_in_handle_preset(self):
        import lovense_api
        import inspect
        source = inspect.getsource(lovense_api.LovenseAPIServer.handle_preset)
        assert "self._pattern_task" in source, (
            "FIX 3 FAILED: handle_preset must store task on self._pattern_task"
        )

    def test_pattern_task_cancelled_before_new(self):
        import lovense_api
        import inspect
        source = inspect.getsource(lovense_api.LovenseAPIServer.handle_preset)
        assert "cancel()" in source, (
            "FIX 3 FAILED: must cancel existing _pattern_task before creating new one"
        )

    def test_pattern_task_stored_in_handle_pattern(self):
        import lovense_api
        import inspect
        source = inspect.getsource(lovense_api.LovenseAPIServer.handle_pattern)
        assert "self._pattern_task" in source, (
            "FIX 3 FAILED: handle_pattern must store task on self._pattern_task"
        )

    def test_stop_cancels_pattern_task(self):
        import lovense_api
        import inspect
        source = inspect.getsource(lovense_api.LovenseAPIServer.handle_function)
        assert "_pattern_task" in source, (
            "FIX 3 FAILED: Stop command in handle_function must cancel pattern task"
        )
        assert "cancel()" in source, (
            "FIX 3 FAILED: Stop command must call cancel() on pattern task"
        )


# ── FIX 4: bridge.py lines 96-100 — add is_connected guard before disconnect ──


class TestFix4DisconnectGuard:
    def test_disconnect_checks_is_connected(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.DirectBLEBackend.disconnect)
        assert "is_connected" in source, (
            "FIX 4 FAILED: disconnect() must check self.device.is_connected "
            "before calling stop()/disconnect()"
        )

    def test_disconnect_checks_device_and_is_connected(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.DirectBLEBackend.disconnect)
        assert "self.device" in source and "self.device.is_connected" in source, (
            "FIX 4 FAILED: guard must check both self.device and "
            "self.device.is_connected"
        )


# ── FIX 5: lovense-bridge.user.js line 25 — change const to let for BRIDGE_URL ──


class TestFix5ConstToLet:
    def test_bridge_url_declared_with_let(self):
        with open("lovense-bridge.user.js") as f:
            content = f.read()
        # Find the BRIDGE_URL declaration line
        for line in content.split("\n"):
            stripped = line.strip()
            if "BRIDGE_URL" in stripped and ("=" in stripped):
                assert stripped.startswith("let BRIDGE_URL"), (
                    f"FIX 5 FAILED: BRIDGE_URL must be declared with 'let'. "
                    f"Found: {stripped}"
                )
                return
        pytest.fail("FIX 5 FAILED: BRIDGE_URL declaration not found in file")

    def test_bridge_url_reassignable(self):
        with open("lovense-bridge.user.js") as f:
            content = f.read()
        assert "BRIDGE_URL = " in content.replace("let ", "").replace("const ", ""), (
            "FIX 5 FAILED: No reassignment of BRIDGE_URL found. "
            "Expected line like 'BRIDGE_URL = url' in setBridgeUrl"
        )


# ── FIX 6: bridge.py line 196 — change --host default from 0.0.0.0 to 127.0.0.1 ──


class TestFix6HostDefault:
    def test_host_default_is_127_0_0_1(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.main)
        assert '"--host"' in source, "FIX 6 FAILED: --host argument not found"
        assert '"127.0.0.1"' in source, (
            "FIX 6 FAILED: --host default must be '127.0.0.1', not '0.0.0.0'"
        )

    def test_no_0_0_0_0_in_host_default(self):
        import bridge
        import inspect
        source = inspect.getsource(bridge.main)
        # Find the --host line
        for line in source.split("\n"):
            if "--host" in line and "default" in line:
                assert "0.0.0.0" not in line, (
                    f"FIX 6 FAILED: --host default is still 0.0.0.0: {line}"
                )