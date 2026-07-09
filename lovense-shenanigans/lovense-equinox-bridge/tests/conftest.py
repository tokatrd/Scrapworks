"""Shared fixtures and mocks for Lovense Equinox Bridge tests."""

import asyncio
import sys
from pathlib import Path
from unittest.mock import AsyncMock, PropertyMock

import pytest

# Ensure the bridge package is importable
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))


@pytest.fixture(scope="session")
def event_loop():
    """Session-scoped event loop for all async fixtures."""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    yield loop
    loop.close()


@pytest.fixture
def mock_device_manager():
    """Create a DeviceManager with a mock backend."""
    from bridge import DeviceManager

    mgr = DeviceManager()
    mgr._backend = AsyncMock()
    type(mgr._backend).is_connected = PropertyMock(return_value=True)
    return mgr


@pytest.fixture
def lovense_server(mock_device_manager):
    """Create a LovenseAPIServer instance with mock device manager."""
    from lovense_api import LovenseAPIServer

    server = LovenseAPIServer(mock_device_manager, host="127.0.0.1", port=20010)
    return server
