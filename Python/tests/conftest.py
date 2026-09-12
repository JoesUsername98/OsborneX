import time

import pytest

from osbornex.orchestration import BinaryNotFoundError, _find_binary


@pytest.fixture
def server_binary():
    try:
        return _find_binary("Server", "ServerMain")
    except BinaryNotFoundError as exc:
        pytest.skip(str(exc))


@pytest.fixture
def bot_binary():
    try:
        return _find_binary("Bot", "BotMain")
    except BinaryNotFoundError as exc:
        pytest.skip(str(exc))


def wait_until(predicate, timeout: float = 2.0, interval: float = 0.05) -> bool:
    """Polls predicate() until it returns truthy or timeout elapses. Mirrors
    OsborneX::TestSupport::wait_for used throughout the C++ test suites."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return bool(predicate())
