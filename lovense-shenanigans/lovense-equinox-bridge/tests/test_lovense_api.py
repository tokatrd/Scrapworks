"""Tests for LovenseAPIServer command handling using aiohttp test client."""

import pytest


@pytest.mark.asyncio
async def test_get_toys_returns_toys(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post("/command", json={"command": "GetToys"})
        assert resp.status == 200
        data = await resp.json()
        assert "data" in data
        assert "toys" in data.get("data", {})
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_function_vibrate_returns_ok(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post(
            "/command",
            json={"command": "Function", "action": "Vibrate:10", "timeSec": 5, "apiVer": 1},
        )
        assert resp.status == 200
        data = await resp.json()
        assert data.get("code") == 200
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_function_stop_returns_ok(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post(
            "/command",
            json={"command": "Function", "action": "Stop", "timeSec": 0, "apiVer": 1},
        )
        assert resp.status == 200
        data = await resp.json()
        assert data.get("code") == 200
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_unknown_command_returns_400(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post("/command", json={"command": "NoSuchCommand"})
        assert resp.status == 400
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_invalid_json_returns_400(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post(
            "/command",
            data="not-json",
            headers={"Content-Type": "application/json"},
        )
        assert resp.status == 400
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_root_endpoint(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.get("/")
        assert resp.status == 200
        data = await resp.json()
        assert "Lovense Equinox Bridge" in data.get("status", "")
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_pattern_with_stop(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp1 = await client.post(
            "/command",
            json={"command": "Pattern", "rule": "1", "strength": "10", "timeSec": 60},
        )
        assert resp1.status == 200
        assert lovense_server._pattern_task is not None

        resp2 = await client.post(
            "/command",
            json={"command": "Function", "action": "Stop", "timeSec": 0},
        )
        assert resp2.status == 200
        assert lovense_server._pattern_task is None
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_double_pattern_cancels_previous(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp1 = await client.post(
            "/command",
            json={"command": "Pattern", "rule": "1,2,3", "strength": "5;10;15", "timeSec": 30},
        )
        assert resp1.status == 200
        first_task = lovense_server._pattern_task

        resp2 = await client.post(
            "/command",
            json={"command": "Pattern", "rule": "4,5,6", "strength": "20;25", "timeSec": 30},
        )
        assert resp2.status == 200
        second_task = lovense_server._pattern_task

        assert first_task is not None
        assert second_task is not None
        assert first_task is not second_task
        assert first_task.cancelled()
    finally:
        await client.close()


@pytest.mark.asyncio
async def test_preset_creates_task(lovense_server):
    from aiohttp.test_utils import TestClient, TestServer

    server = TestServer(lovense_server.app)
    client = TestClient(server)
    await client.start_server()
    try:
        resp = await client.post(
            "/command",
            json={"command": "Preset", "name": "pulse", "timeSec": 5},
        )
        assert resp.status == 200
        assert lovense_server._pattern_task is not None
    finally:
        await client.close()
