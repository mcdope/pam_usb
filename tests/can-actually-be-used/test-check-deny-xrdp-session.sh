#!/usr/bin/bash
#
# Regression test for local.c: XRDP_SESSION env var must cause denial.
# The check fires in pusb_local_login() before any device or TTY inspection,
# so no USB device is required for this test.
#
# Note: CI disables deny_remote globally so SSH-based pamusb-check calls work.
# We create a temp config with deny_remote=true to test this path in isolation.
# We also check syslog for the message content because log_error() only writes
# to stderr when stdin is a TTY (see log.c:pusb_log_output); in CI (non-interactive
# SSH) stdin is not a TTY, so the message only appears in syslog.

WHOAMI=$(whoami)
CONF=/etc/security/pam_usb.conf
TMP_CONF=$(mktemp /tmp/pam_usb_xrdp_test_XXXXXX.conf)
trap 'rm -f "$TMP_CONF"' EXIT

# Flip deny_remote to true (CI sets it to false globally to allow SSH-based checks)
sed 's|<option name="deny_remote">false</option>|<option name="deny_remote">true</option>|' "$CONF" > "$TMP_CONF"

echo -e "Test:\t\t\tpamusb-check denies access when XRDP_SESSION is set"
if XRDP_SESSION=1 pamusb-check --config="$TMP_CONF" "$WHOAMI" 2>/dev/null; then
    echo "FAILED: pamusb-check should deny access when XRDP_SESSION is set"
    exit 1
fi
echo -e "Result:\t\t\tPASSED!"

echo -e "Test:\t\t\tpamusb-check log contains XRDP session message"
XRDP_SESSION=testvalue pamusb-check --config="$TMP_CONF" "$WHOAMI" 2>/dev/null || true
journalctl -t pam_usb --no-pager -n 20 2>/dev/null | grep -q "XRDP session detected (testvalue)" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: expected 'XRDP session detected (testvalue)' in syslog"; exit 1; }
