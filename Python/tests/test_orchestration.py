import time

import pytest

from osbornex.orchestration import BinaryNotFoundError, _find_binary, _resolve_repo_root


def _touch(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("")


def test_find_binary_picks_newest_by_mtime(tmp_path):
    older = tmp_path / "build" / "preset-a" / "Server" / "Debug" / "ServerMain.exe"
    newer = tmp_path / "build" / "preset-b" / "Server" / "Debug" / "ServerMain.exe"
    _touch(older)
    time.sleep(0.01)
    _touch(newer)

    found = _find_binary("Server", "ServerMain", repo_root=tmp_path)
    assert found == newer


def test_find_binary_respects_preset_filter(tmp_path):
    a = tmp_path / "build" / "preset-a" / "Server" / "Debug" / "ServerMain.exe"
    b = tmp_path / "build" / "preset-b" / "Server" / "Debug" / "ServerMain.exe"
    _touch(a)
    time.sleep(0.01)
    _touch(b)  # newer overall, but excluded by the preset filter below

    found = _find_binary("Server", "ServerMain", preset="preset-a", repo_root=tmp_path)
    assert found == a


def test_find_binary_works_without_msvc_debug_release_subdir(tmp_path):
    # Linux builds put the exe directly under build/<preset>/<component>/, with no
    # Debug/Release subdirectory the way the MSVC generator does.
    linux_style = tmp_path / "build" / "linux-gcc-debug" / "Bot" / "BotMain"
    _touch(linux_style)

    found = _find_binary("Bot", "BotMain", repo_root=tmp_path)
    assert found == linux_style


def test_find_binary_raises_a_helpful_error_when_nothing_matches(tmp_path):
    with pytest.raises(BinaryNotFoundError) as exc_info:
        _find_binary("Server", "ServerMain", repo_root=tmp_path)
    assert "cmake --build --preset" in str(exc_info.value)


def test_resolve_repo_root_finds_cmake_presets_walking_upward(tmp_path, monkeypatch):
    monkeypatch.delenv("OSBORNEX_REPO_ROOT", raising=False)
    (tmp_path / "CMakePresets.json").write_text("{}")
    nested = tmp_path / "some" / "nested" / "dir"
    nested.mkdir(parents=True)
    monkeypatch.chdir(nested)

    assert _resolve_repo_root(None) == tmp_path.resolve()


def test_resolve_repo_root_prefers_explicit_param(tmp_path):
    assert _resolve_repo_root(tmp_path) == tmp_path.resolve()


def test_resolve_repo_root_uses_env_var(tmp_path, monkeypatch):
    monkeypatch.setenv("OSBORNEX_REPO_ROOT", str(tmp_path))
    assert _resolve_repo_root(None) == tmp_path.resolve()
