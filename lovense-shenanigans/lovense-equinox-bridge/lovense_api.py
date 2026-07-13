"""
Lovense Game Mode (Standard API) server implementation.

Listens on port 20010 (HTTP) and implements the Lovense Standard API:
  POST /command
  Content-Type: application/json

Commands:
  - GetToys      - returns connected toys list
  - GetToyName   - returns simplified names
  - Function     - sends action commands (Vibrate, Rotate, etc.)
  - Preset       - play built-in patterns
  - Pattern      - play custom pattern sequence
"""

import asyncio
import json
import logging
from aiohttp import web

logger = logging.getLogger("lovense_api")


class LovenseAPIServer:
    def __init__(self, device_manager, host: str = "0.0.0.0", port: int = 20010):
        self.device_manager = device_manager
        self.host = host
        self.port = port
        self._pattern_task = None
        self.app = web.Application()
        self.app.router.add_post("/command", self.handle_command)
        self.app.router.add_get("/", self.handle_root)

    async def handle_root(self, request):
        return web.json_response({"status": "Lovense Equinox Bridge", "version": "1.0.0"})

    async def handle_command(self, request):
        try:
            body = await request.json()
        except Exception:
            return web.json_response({"code": 400, "type": "error", "message": "Invalid JSON"}, status=400)

        command = body.get("command", "")
        logger.info(f"Received command: {command} body={body}")

        handlers = {
            "GetToys": self.handle_get_toys,
            "GetToyName": self.handle_get_toy_name,
            "Function": self.handle_function,
            "Preset": self.handle_preset,
            "Pattern": self.handle_pattern,
            "Battery": self.handle_battery,
        }

        handler = handlers.get(command)
        if not handler:
            return web.json_response({"code": 400, "type": "error", "message": f"Unknown command: {command}"}, status=400)

        return await handler(body)

    async def handle_get_toys(self, body):
        toy = self.device_manager.get_toy_info()
        if toy is None:
            return web.json_response({
                "code": 200, "type": "OK",
                "data": {"toys": "{}", "platform": "bridge", "appType": "connect"}
            })

        toy_id = toy["id"]
        toys_json = json.dumps({
            toy_id: {
                "id": toy_id,
                "status": "1",
                "version": "",
                "name": "equinox",
                "battery": toy["battery"],
                "nickName": "Equinox Bridge",
            }
        })

        return web.json_response({
            "code": 200, "type": "OK",
            "data": {
                "toys": toys_json,
                "platform": "bridge",
                "appType": "connect",
            }
        })

    async def handle_get_toy_name(self, body):
        toy = self.device_manager.get_toy_info()
        names = [toy["name"]] if toy else []
        return web.json_response({"code": 200, "data": names, "type": "OK"})

    async def handle_function(self, body):
        action = body.get("action", "")

        if action == "Stop":
            if self._pattern_task is not None:
                self._pattern_task.cancel()
                self._pattern_task = None
            await self.device_manager.set_vibrate(0)
            return web.json_response({"code": 200, "type": "ok"})

        if ":" in action:
            action_name, raw_value = action.split(":", 1)
        else:
            action_name = action
            raw_value = "0"

        try:
            value = int(raw_value)
        except ValueError:
            return web.json_response({"code": 404, "type": "error", "message": "Invalid value"}, status=400)

        action_name_lower = action_name.lower()

        if action_name_lower in ("vibrate", "v"):
            await self.device_manager.set_vibrate(value)
            return web.json_response({"code": 200, "type": "ok"})
        elif action_name_lower in ("stop",):
            if self._pattern_task is not None:
                self._pattern_task.cancel()
                self._pattern_task = None
            await self.device_manager.set_vibrate(0)
            return web.json_response({"code": 200, "type": "ok"})
        elif action_name_lower in ("all",):
            await self.device_manager.set_vibrate(value)
            return web.json_response({"code": 200, "type": "ok"})
        else:
            logger.warning(f"Unsupported action: {action_name_lower}, falling back to vibrate")
            await self.device_manager.set_vibrate(value)
            return web.json_response({"code": 200, "type": "ok"})

    async def handle_preset(self, body):
        name = body.get("name", "pulse")
        time_sec = body.get("timeSec", 9)
        logger.info(f"Pattern {name} for {time_sec}s - playing as vibration sequence")

        patterns = {
            "pulse": [0, 100, 0, 100, 0, 100, 0, 100],
            "wave": [20, 40, 60, 80, 100, 80, 60, 40, 20],
            "fireworks": [100, 0, 80, 0, 100, 0, 60, 0, 100],
            "earthquake": [100, 80, 100, 60, 100, 80, 100],
        }
        pattern = patterns.get(name, patterns["pulse"])
        if self._pattern_task is not None:
            self._pattern_task.cancel()
        self._pattern_task = asyncio.create_task(self._play_pattern(pattern, time_sec))
        return web.json_response({"code": 200, "type": "ok"})

    async def _play_pattern(self, pattern: list, duration_sec: int):
        import time
        interval = max(0.2, duration_sec / max(len(pattern), 1))
        start = time.time()
        while time.time() - start < duration_sec:
            for val in pattern:
                await self.device_manager.set_vibrate(val)
                await asyncio.sleep(interval)
                if time.time() - start >= duration_sec:
                    break
        await self.device_manager.set_vibrate(0)

    async def handle_pattern(self, body):
        strength_str = body.get("strength", "")
        time_sec = body.get("timeSec", 5)

        strengths = []
        for s in strength_str.split(";"):
            try:
                strengths.append(int(s.strip()))
            except ValueError:
                pass

        if strengths:
            if self._pattern_task is not None:
                self._pattern_task.cancel()
            self._pattern_task = asyncio.create_task(self._play_pattern(strengths, time_sec))

        return web.json_response({"code": 200, "type": "ok"})

    async def handle_battery(self, body):
        toy = self.device_manager.get_toy_info()
        batt = toy["battery"] if toy else 100
        return web.json_response({"code": 200, "data": {"battery": batt}, "type": "OK"})
