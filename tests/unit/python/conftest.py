"""
Shared fixtures and module-level mocks for pamusb Python tool tests.

gi, dbus, and UDisks2 modules are stubbed before any tool is imported,
so the native libraries don't need to be importable in the test environment.
"""

import sys
import importlib.util
import importlib.machinery
from unittest.mock import MagicMock
import pytest

# Stub all native modules that the tools try to import at module level
for _mod in [
    "gi",
    "gi.repository",
    "gi.repository.UDisks",
    "gi.repository.GLib",
    "dbus",
    "dbus.mainloop",
    "dbus.mainloop.glib",
    "dotenv",
]:
    sys.modules.setdefault(_mod, MagicMock())


def load_tool(name: str, path: str):
    """Import a Python tool file that has no .py extension."""
    loader = importlib.machinery.SourceFileLoader(name, path)
    spec = importlib.util.spec_from_loader(name, loader)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    loader.exec_module(mod)
    return mod


@pytest.fixture(scope="session")
def tools_dir(pytestconfig):
    return str(pytestconfig.rootdir / "tools")


@pytest.fixture
def tmp_conf(tmp_path):
    """Minimal valid pam_usb.conf written to a temp directory."""
    conf = tmp_path / "pam_usb.conf"
    conf.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<configuration>\n"
        "  <devices/>\n"
        "  <users/>\n"
        "</configuration>\n"
    )
    return conf
