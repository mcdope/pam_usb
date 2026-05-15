"""
Unit tests for tools/pamusb-keyring-unlock-gnome.

Security regression coverage:
  K-1: authentication check invokes /usr/bin/pamusb-check, not PATH lookup
  K-1: username lookup invokes /usr/bin/id, not PATH lookup
  K-2: stored password is read as data without sourcing shell code
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


def test_password_file_is_not_sourced_as_shell():
    text = SCRIPT.read_text()

    assert "KEYFILE=" in text
    assert "source " not in text
    assert ". \"$KEYFILE\"" not in text
    assert "UNLOCK_PASSWORD=$(grep" not in text
    assert 'UNLOCK_PASSWORD=$("$SED" -n' in text
    assert 'printf \'UNLOCK_PASSWORD=%s\\n\'' in text


def test_keyring_commands_use_absolute_paths():
    text = SCRIPT.read_text()

    assert "PIDOF=/usr/bin/pidof" in text
    assert "KILL=/usr/bin/kill" in text
    assert "GNOME_KEYRING_DAEMON=/usr/bin/gnome-keyring-daemon" in text
    assert "`pidof gnome-keyring-daemon`" not in text
    assert '"$GNOME_KEYRING_DAEMON" --daemonize --login' in text
