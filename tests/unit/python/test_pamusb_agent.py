"""
Unit tests for tools/pamusb-agent

Security regression coverage:
  H-1: dangerous env vars (LD_PRELOAD, LD_LIBRARY_PATH, PYTHONPATH, IFS …) are stripped
  H-1: legitimate env vars (HOME, DISPLAY …) are preserved
  H-1: run_auth_check passes subprocess.run a list, not a shell string
"""

import sys
import os
import subprocess
import pytest
from unittest.mock import MagicMock, patch
from xml.etree import ElementTree as ET
import importlib.util
import importlib.machinery

_TOOL_PATH = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "tools", "pamusb-agent"
)


def _load_agent():
    """Import pamusb-agent without triggering __main__ execution."""
    loader = importlib.machinery.SourceFileLoader("pamusb_agent", _TOOL_PATH)
    spec = importlib.util.spec_from_loader("pamusb_agent", loader)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["pamusb_agent"] = mod
    loader.exec_module(mod)
    return mod


_mod = _load_agent()


# ── sanitize_agent_env ────────────────────────────────────────────────────────

def _make_hotplug_xml(env_pairs):
    """Build a minimal <agent> element with <env>KEY=VALUE</env> children."""
    agent = ET.fromstring("<agent event='unlock'></agent>")
    for k, v in env_pairs:
        e = ET.SubElement(agent, "env")
        e.text = "%s=%s" % (k, v)
    return agent


def _make_logger():
    logger = MagicMock()
    logger.error = MagicMock()
    return logger


def test_ld_preload_rejected():
    """H-1: LD_PRELOAD must not be passed to agent subprocesses."""
    hotplug = _make_hotplug_xml([("LD_PRELOAD", "/evil.so"), ("HOME", "/home/user")])
    logger = _make_logger()
    result = _mod.sanitize_agent_env(hotplug, "testuser", logger)
    assert "LD_PRELOAD" not in result
    logger.error.assert_called()


def test_ld_library_path_rejected():
    """H-1: LD_LIBRARY_PATH must not be passed to agent subprocesses."""
    hotplug = _make_hotplug_xml([("LD_LIBRARY_PATH", "/evil/lib")])
    logger = _make_logger()
    result = _mod.sanitize_agent_env(hotplug, "testuser", logger)
    assert "LD_LIBRARY_PATH" not in result


def test_pythonpath_rejected():
    """H-1: PYTHONPATH must not be passed to agent subprocesses."""
    hotplug = _make_hotplug_xml([("PYTHONPATH", "/evil/python")])
    logger = _make_logger()
    result = _mod.sanitize_agent_env(hotplug, "testuser", logger)
    assert "PYTHONPATH" not in result


def test_ifs_rejected():
    """H-1: IFS must not be passed to agent subprocesses."""
    hotplug = _make_hotplug_xml([("IFS", "x")])
    logger = _make_logger()
    result = _mod.sanitize_agent_env(hotplug, "testuser", logger)
    assert "IFS" not in result


def test_legitimate_env_preserved():
    """H-1: safe env vars like HOME and DISPLAY must be passed through."""
    hotplug = _make_hotplug_xml([
        ("HOME", "/home/user"),
        ("DISPLAY", ":0"),
        ("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus"),
    ])
    logger = _make_logger()
    result = _mod.sanitize_agent_env(hotplug, "testuser", logger)
    assert result.get("HOME") == "/home/user"
    assert result.get("DISPLAY") == ":0"
    assert result.get("DBUS_SESSION_BUS_ADDRESS") == "unix:path=/run/user/1000/bus"


# ── run_auth_check ─────────────────────────────────────────────────────────────

def test_run_auth_check_uses_list():
    """H-1: subprocess.run must be called with a list, not a shell string."""
    captured = []

    def mock_run(args, **kwargs):
        captured.append(args)
        return subprocess.CompletedProcess(args, 0, b"", b"")

    options = {"check": "/usr/bin/pamusb-check", "configFile": "/etc/pam_usb.conf"}

    with patch("subprocess.run", side_effect=mock_run):
        _mod.run_auth_check(options, "testuser")

    assert len(captured) == 1
    assert isinstance(captured[0], list), (
        "subprocess.run must be called with a list to avoid shell injection"
    )


def test_run_auth_check_includes_username():
    """run_auth_check must include the username as a positional argument."""
    captured = []

    def mock_run(args, **kwargs):
        captured.append(args)
        return subprocess.CompletedProcess(args, 0, b"", b"")

    options = {"check": "/usr/bin/pamusb-check", "configFile": "/tmp/test.conf"}

    with patch("subprocess.run", side_effect=mock_run):
        _mod.run_auth_check(options, "alice")

    assert "alice" in captured[0]


def test_dangerous_env_vars_set_is_at_module_level():
    """_DANGEROUS_ENV_VARS must be a module-level set so tests and code share it."""
    assert hasattr(_mod, "_DANGEROUS_ENV_VARS")
    assert isinstance(_mod._DANGEROUS_ENV_VARS, (set, frozenset))
    assert "LD_PRELOAD" in _mod._DANGEROUS_ENV_VARS
    assert "PYTHONPATH" in _mod._DANGEROUS_ENV_VARS
    assert "IFS" in _mod._DANGEROUS_ENV_VARS
