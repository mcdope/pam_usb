"""
Unit tests for tools/pamusb-keyring-unlock-gnome.

Security regression coverage:
  K-1: authentication check invokes /usr/bin/pamusb-check, not PATH lookup
  K-1: username lookup invokes /usr/bin/id, not PATH lookup
"""

from pathlib import Path
import subprocess


SCRIPT = Path(__file__).resolve().parents[3] / "tools" / "pamusb-keyring-unlock-gnome"


def test_script_syntax_is_valid():
    subprocess.run(["sh", "-n", str(SCRIPT)], check=True)


def test_auth_check_uses_absolute_pamusb_check():
    text = SCRIPT.read_text()

    assert "PAMUSB_CHECK=/usr/bin/pamusb-check" in text
    assert '"$PAMUSB_CHECK" "$USER_NAME"' in text
    assert "pamusb-check `whoami`" not in text
    assert "pamusb-check $(whoami)" not in text


def test_username_lookup_uses_absolute_id():
    text = SCRIPT.read_text()

    assert "ID=/usr/bin/id" in text
    assert 'USER_NAME=$("$ID" -un)' in text
    assert "`whoami`" not in text
    assert "$(whoami)" not in text
