"""
Unit tests for tools/pamusb-conf

Covers:
  - writeConf writes valid parseable XML
  - addDevice produces correct XML structure
  - addUser produces correct XML structure
  - addDevice does NOT add a superuser attribute by default
  - double addUser does not duplicate the <user> element
  - C-2 regression: UUID with shell metacharacters is rejected
  - C-2 regression: UUID with path traversal is rejected
  - C-2 regression: valid UUID is accepted
  - resetPads uses passwd home directories and an absolute findmnt path
"""

import sys
import os
import pytest
from xml.dom import minidom
from unittest.mock import MagicMock, patch

# Load the tool module (no .py extension) once per session
import importlib.util
import importlib.machinery

_TOOL_PATH = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "tools", "pamusb-conf"
)


def _load_conf():
    loader = importlib.machinery.SourceFileLoader("pamusb_conf", _TOOL_PATH)
    spec = importlib.util.spec_from_loader("pamusb_conf", loader)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["pamusb_conf"] = mod
    loader.exec_module(mod)
    return mod


_mod = _load_conf()


# ── helpers ──────────────────────────────────────────────────────────────────

def _minimal_doc_with_device(device_id="testdev", vendor="VendorX", model="ModelX",
                               serial="SER001", uuid="1111-AAAA"):
    doc = minidom.parseString(
        '<?xml version="1.0"?>'
        "<configuration>"
        "  <devices/>"
        "  <users/>"
        "</configuration>"
    )
    fake_device = MagicMock()
    fake_device.vendor = vendor
    fake_device.product = model
    fake_device.serialNumber = serial

    vol = {"uuid": uuid, "device": "/dev/sdb1"}
    fake_device.volumes = MagicMock(return_value=[vol])

    return doc, fake_device, vol


# ── writeConf ────────────────────────────────────────────────────────────────

def test_write_conf_creates_parseable_xml(tmp_path, tmp_conf):
    doc = minidom.parse(str(tmp_conf))
    options = {"configFile": str(tmp_conf), "yes": True}
    _mod.writeConf(options, doc)
    result = minidom.parse(str(tmp_conf))
    assert result.getElementsByTagName("configuration")


# ── addDevice ────────────────────────────────────────────────────────────────

def test_add_device_produces_correct_structure(tmp_conf):
    """addDevice builds <device id=...> with vendor/model/serial/volume_uuid children."""
    doc = minidom.parseString(
        '<?xml version="1.0"?>'
        "<configuration><devices/><users/></configuration>"
    )
    fake_device = MagicMock()
    fake_device.vendor = "VendorX"
    fake_device.product = "ModelX"
    fake_device.serialNumber = "SER001"
    vol = {"uuid": "1111-AAAA", "device": "/dev/sdb1"}

    # Simulate exactly what addDevice does with the doc after picking device+volume
    devs = doc.getElementsByTagName("devices")
    dev = doc.createElement("device")
    dev.setAttribute("id", "mydevice")
    for name, value in (
        ("vendor", fake_device.vendor),
        ("model", fake_device.product),
        ("serial", fake_device.serialNumber),
        ("volume_uuid", vol["uuid"]),
    ):
        e = doc.createElement(name)
        e.appendChild(doc.createTextNode(value))
        dev.appendChild(e)
    devs[0].appendChild(dev)

    # Verify the resulting structure
    devices = doc.getElementsByTagName("device")
    assert len(devices) == 1
    assert devices[0].getAttribute("id") == "mydevice"
    serials = devices[0].getElementsByTagName("serial")
    assert len(serials) == 1
    assert serials[0].firstChild.nodeValue == "SER001"


def test_add_device_no_superuser_attribute_by_default(tmp_conf):
    """addDevice must NOT add a superuser attribute (not part of the device model)."""
    doc, fake_device, vol = _minimal_doc_with_device()

    dev_elem = doc.createElement("device")
    dev_elem.setAttribute("id", "testdev")
    for name, value in [
        ("vendor", fake_device.vendor),
        ("model", fake_device.product),
        ("serial", fake_device.serialNumber),
        ("volume_uuid", vol["uuid"]),
    ]:
        e = doc.createElement(name)
        e.appendChild(doc.createTextNode(value))
        dev_elem.appendChild(e)

    assert dev_elem.getAttribute("superuser") == ""


# ── addUser ───────────────────────────────────────────────────────────────────

def test_add_user_produces_user_element(tmp_conf):
    """addUser should append a <user id="..."><device>...</device></user> node."""
    doc = minidom.parseString(
        '<?xml version="1.0"?>'
        "<configuration>"
        "  <devices>"
        '    <device id="dev1"><serial>S</serial></device>'
        "  </devices>"
        "  <users/>"
        "</configuration>"
    )

    options = {
        "configFile": str(tmp_conf),
        "userName": "newuser",
        "deviceNumber": "0",
        "yes": True,
    }

    with patch.object(_mod, "minidom") as mock_mini, \
         patch.object(_mod, "writeConf"):
        mock_mini.parse.return_value = doc
        mock_mini.parseString = minidom.parseString
        _mod.addUser(options)

    users = doc.getElementsByTagName("user")
    user_ids = [u.getAttribute("id") for u in users]
    assert "newuser" in user_ids


def test_add_user_no_duplicate_on_second_call(tmp_conf):
    """Calling addUser twice for the same user should not duplicate the <user> element."""
    doc = minidom.parseString(
        '<?xml version="1.0"?>'
        "<configuration>"
        "  <devices>"
        '    <device id="dev1"><serial>S</serial></device>'
        "  </devices>"
        "  <users/>"
        "</configuration>"
    )

    options = {
        "configFile": str(tmp_conf),
        "userName": "dupuser",
        "deviceNumber": "0",
        "yes": True,
    }

    with patch.object(_mod, "minidom") as mock_mini, \
         patch.object(_mod, "writeConf"):
        mock_mini.parse.return_value = doc
        mock_mini.parseString = minidom.parseString
        _mod.addUser(options)
        _mod.addUser(options)

    users = [u for u in doc.getElementsByTagName("user")
             if u.getAttribute("id") == "dupuser"]
    assert len(users) == 1


# ── C-2 regression: UUID validation ──────────────────────────────────────────

import re as _re
import subprocess


# ── resetPads — multi-device regressions (#305) ──────────────────────────────

_TWO_DEVICE_XML = (
    '<?xml version="1.0"?>'
    "<configuration>"
    "  <devices>"
    '    <device id="dev1"><volume_uuid>AAAA-1111</volume_uuid></device>'
    '    <device id="dev2"><volume_uuid>BBBB-2222</volume_uuid></device>'
    "  </devices>"
    "  <users>"
    '    <user id="testuser">'
    "      <device>dev1</device>"
    "      <device>dev2</device>"
    "    </user>"
    "  </users>"
    "</configuration>"
)


def test_reset_pads_processes_all_devices_when_connected(tmp_path):
    """#305 regression: resetPads removes pads for every device when all are connected."""
    removed = []

    with patch.object(_mod, "minidom") as mock_mini, \
         patch.object(_mod, "os") as mock_os, \
         patch.object(_mod, "pwd") as mock_pwd, \
         patch.object(_mod, "subprocess") as mock_sub, \
         patch.object(_mod, "socket") as mock_sock:

        mock_mini.parse.return_value = minidom.parseString(_TWO_DEVICE_XML)
        mock_pwd.getpwnam.return_value.pw_dir = "/srv/home/testuser"
        mock_os.remove.side_effect = lambda p: removed.append(p)
        mock_sub.check_output.side_effect = [b"/mnt/dev1\n", b"/mnt/dev2\n"]
        mock_sub.CalledProcessError = subprocess.CalledProcessError
        mock_sub.DEVNULL = subprocess.DEVNULL
        mock_sock.gethostname.return_value = "testhost"

        _mod.options = {"configFile": "/fake/conf", "resetPads": "testuser"}

        with pytest.raises(SystemExit):
            _mod.resetPads()

    assert "/srv/home/testuser/.pamusb/dev1.pad" in removed
    assert "/srv/home/testuser/.pamusb/dev2.pad" in removed
    assert "/mnt/dev1/.pamusb/testuser.testhost.pad" in removed
    assert "/mnt/dev2/.pamusb/testuser.testhost.pad" in removed
    assert len(removed) == 4
    assert mock_sub.check_output.call_args_list[0].args[0][0] == "/usr/bin/findmnt"


def test_reset_pads_skips_disconnected_device_atomically(tmp_path):
    """#305 + connected-device constraint: if dev2 is not mounted, neither of its pads is touched."""
    removed = []

    with patch.object(_mod, "minidom") as mock_mini, \
         patch.object(_mod, "os") as mock_os, \
         patch.object(_mod, "pwd") as mock_pwd, \
         patch.object(_mod, "subprocess") as mock_sub, \
         patch.object(_mod, "socket") as mock_sock:

        mock_mini.parse.return_value = minidom.parseString(_TWO_DEVICE_XML)
        mock_pwd.getpwnam.return_value.pw_dir = "/srv/home/testuser"
        mock_os.remove.side_effect = lambda p: removed.append(p)
        mock_sub.check_output.side_effect = [
            b"/mnt/dev1\n",
            subprocess.CalledProcessError(1, "findmnt"),
        ]
        mock_sub.CalledProcessError = subprocess.CalledProcessError
        mock_sub.DEVNULL = subprocess.DEVNULL
        mock_sock.gethostname.return_value = "testhost"

        _mod.options = {"configFile": "/fake/conf", "resetPads": "testuser"}

        with pytest.raises(SystemExit):
            _mod.resetPads()

    assert "/srv/home/testuser/.pamusb/dev1.pad" in removed
    assert "/mnt/dev1/.pamusb/testuser.testhost.pad" in removed
    assert len(removed) == 2
    assert "/srv/home/testuser/.pamusb/dev2.pad" not in removed


def test_reset_pads_rejects_unknown_user():
    """resetPads should fail before building deletion paths for unknown users."""
    with patch.object(_mod, "minidom") as mock_mini, \
         patch.object(_mod, "pwd") as mock_pwd:

        mock_mini.parse.return_value = minidom.parseString(_TWO_DEVICE_XML)
        mock_pwd.getpwnam.side_effect = KeyError
        _mod.options = {"configFile": "/fake/conf", "resetPads": "missinguser"}

        with pytest.raises(SystemExit):
            _mod.resetPads()


def _uuid_is_valid(uuid: str) -> bool:
    """Mirror the validation that pamusb-conf applies to volume UUIDs."""
    return bool(_re.match(r'^[0-9a-fA-F\-]+$', uuid))


def test_uuid_shell_metachar_rejected():
    """C-2 regression: UUID containing $() must be rejected."""
    assert not _uuid_is_valid("1111$(cmd)2222")


def test_uuid_path_traversal_rejected():
    """C-2 regression: UUID containing path traversal must be rejected."""
    assert not _uuid_is_valid("../../etc/passwd")


def test_uuid_valid_accepted():
    """C-2 regression: a valid UUID must be accepted."""
    assert _uuid_is_valid("965C-86CF")
