"""Launches and manages the existing ServerMain/BotMain executables as subprocesses.

No changes to the C++ side are needed: both mains already accept the config this
needs via CLI args, and both already read a line from stdin to stop -- so a
graceful stop() here is just writing a newline to the subprocess's stdin pipe.
"""

from __future__ import annotations

import os
import socket
import subprocess
import threading
import time
from collections import deque
from collections.abc import Sequence
from pathlib import Path
from typing import TypeVar

_ManagedProcessT = TypeVar("_ManagedProcessT", bound="_ManagedProcess")


class BinaryNotFoundError(FileNotFoundError):
    pass


def _resolve_repo_root(repo_root: str | Path | None) -> Path:
    if repo_root is not None:
        return Path(repo_root).resolve()
    env = os.environ.get("OSBORNEX_REPO_ROOT")
    if env:
        return Path(env).resolve()
    current = Path.cwd().resolve()
    for candidate in (current, *current.parents):
        if (candidate / "CMakePresets.json").is_file():
            return candidate
    raise FileNotFoundError(
        "could not locate the OsborneX repo root (no CMakePresets.json found walking up "
        f"from {current}); pass repo_root= explicitly or set OSBORNEX_REPO_ROOT"
    )


def _find_binary(
    component: str,
    exe_name: str,
    *,
    preset: str | None = None,
    repo_root: str | Path | None = None,
) -> Path:
    """Finds the newest-built exe_name under build/*/component/**/, e.g.
    build/windows-msvc-debug/Server/Debug/ServerMain.exe or
    build/linux-gcc-debug/Server/ServerMain (Linux has no Debug/Release subdir)."""
    root = _resolve_repo_root(repo_root)
    build_dir = root / "build"
    candidates: list[Path] = []
    for pattern in (f"{exe_name}.exe", exe_name):
        candidates.extend(p for p in build_dir.glob(f"*/{component}/**/{pattern}") if p.is_file())

    if preset is not None:
        candidates = [p for p in candidates if p.relative_to(build_dir).parts[0] == preset]

    if not candidates:
        where = f"{build_dir}/{preset or '*'}/{component}/**/{exe_name}[.exe]"
        raise BinaryNotFoundError(
            f"could not find {exe_name} under {where}. Build it first, "
            f"e.g.: cmake --build --preset windows-msvc-debug"
        )

    return max(candidates, key=lambda p: p.stat().st_mtime)


class _ManagedProcess:
    """Shared subprocess lifecycle for Server/Bot: drains stdout so the child never
    blocks on a full pipe buffer, and stops gracefully via stdin before falling back
    to terminate()/kill()."""

    def __init__(self, process: subprocess.Popen) -> None:
        self._process = process
        self._output: deque[str] = deque(maxlen=200)
        self._drain_thread = threading.Thread(target=self._drain_stdout, daemon=True)
        self._drain_thread.start()

    def _drain_stdout(self) -> None:
        assert self._process.stdout is not None
        for line in self._process.stdout:
            self._output.append(line.rstrip("\n"))

    @property
    def output(self) -> list[str]:
        """The child's captured stdout/stderr, most recent 200 lines."""
        return list(self._output)

    def is_running(self) -> bool:
        return self._process.poll() is None

    def stop(self, timeout: float = 5.0) -> None:
        if self._process.poll() is not None:
            return

        try:
            if self._process.stdin is not None:
                self._process.stdin.write("\n")
                self._process.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

        try:
            self._process.wait(timeout=timeout)
            return
        except subprocess.TimeoutExpired:
            pass

        self._process.terminate()
        try:
            self._process.wait(timeout=2.0)
            return
        except subprocess.TimeoutExpired:
            pass

        self._process.kill()
        self._process.wait(timeout=2.0)

    def __enter__(self: _ManagedProcessT) -> _ManagedProcessT:
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.stop()


class Server(_ManagedProcess):
    """A running ServerMain subprocess."""

    def __init__(
        self,
        process: subprocess.Popen,
        order_entry_port: int,
        market_data_group: str,
        market_data_port: int,
    ) -> None:
        super().__init__(process)
        self._order_entry_port = order_entry_port
        self._market_data_group = market_data_group
        self._market_data_port = market_data_port

    @classmethod
    def start(
        cls,
        *,
        order_entry_port: int = 9001,
        market_data_group: str = "239.1.1.1",
        market_data_port: int = 9002,
        binary: str | Path | None = None,
        preset: str | None = None,
        repo_root: str | Path | None = None,
        ready_timeout: float = 10.0,
    ) -> "Server":
        exe = (
            Path(binary)
            if binary is not None
            else _find_binary("Server", "ServerMain", preset=preset, repo_root=repo_root)
        )
        process = subprocess.Popen(
            [str(exe), str(order_entry_port), market_data_group, str(market_data_port)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        server = cls(process, order_entry_port, market_data_group, market_data_port)
        server._wait_ready(ready_timeout)
        return server

    def _wait_ready(self, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._process.poll() is not None:
                raise RuntimeError(
                    f"ServerMain exited before becoming ready (code {self._process.returncode}); "
                    "output:\n" + "\n".join(self.output)
                )
            try:
                with socket.create_connection((self.host, self._order_entry_port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError(
            f"ServerMain did not become ready on port {self._order_entry_port} within {timeout}s; "
            "output:\n" + "\n".join(self.output)
        )

    @property
    def host(self) -> str:
        return "127.0.0.1"

    @property
    def order_entry_port(self) -> int:
        return self._order_entry_port

    @property
    def market_data_group(self) -> str:
        return self._market_data_group

    @property
    def market_data_port(self) -> int:
        return self._market_data_port


class Bot(_ManagedProcess):
    """A running BotMain subprocess (a fire-and-forget order-flow participant)."""

    @classmethod
    def start(
        cls,
        *,
        symbols: Sequence[int],
        source: int,
        server: "Server | None" = None,
        server_host: str = "127.0.0.1",
        order_entry_port: int = 9001,
        market_data_group: str = "239.1.1.1",
        market_data_port: int = 9002,
        binary: str | Path | None = None,
        preset: str | None = None,
        repo_root: str | Path | None = None,
        startup_grace: float = 0.3,
    ) -> "Bot":
        if server is not None:
            server_host = server.host
            order_entry_port = server.order_entry_port
            market_data_group = server.market_data_group
            market_data_port = server.market_data_port

        exe = Path(binary) if binary is not None else _find_binary("Bot", "BotMain", preset=preset, repo_root=repo_root)
        symbols_arg = ",".join(str(symbol) for symbol in symbols)
        process = subprocess.Popen(
            [
                str(exe),
                server_host,
                str(order_entry_port),
                market_data_group,
                str(market_data_port),
                symbols_arg,
                str(source),
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        bot = cls(process)
        # No listening port to poll for readiness -- BotMain already retries
        # connecting to the server internally for up to 10s on its own
        # (Bot/apps/bot_main.cpp), so this is just a quick "didn't die instantly" check.
        time.sleep(startup_grace)
        if process.poll() is not None:
            raise RuntimeError(
                f"BotMain exited immediately (code {process.returncode}); output:\n" + "\n".join(bot.output)
            )
        return bot
