#!/usr/bin/bash
#
# Regression test for local.c: an sshd ancestor process must cause denial.
# This exercises the parent-process-chain detection in pusb_local_login()
# (src/local.c), which fires before any device or TTY inspection, so no
# USB device is required for this test.
#
# Unlike XRDP_SESSION, there is no env var to fake an sshd ancestor, so this
# test only makes sense when actually invoked over SSH (as CI does). We skip
# gracefully when run locally.
#
# We check syslog for the message content because log_error() only writes
# to stderr when stdin is a TTY (see log.c:pusb_log_output); in CI (non-interactive
# SSH) stdin is not a TTY, so the message only appears in syslog.

if [ -z "$SSH_CONNECTION" ] && [ -z "$SSH_CLIENT" ]; then
    echo "Skipping: not running in an SSH session, no sshd ancestor to detect"
    exit 0
fi

WHOAMI=$(whoami)

echo -e "Test:\t\t\tpamusb-check denies access when an sshd ancestor is detected"
if pamusb-check "$WHOAMI" 2>/dev/null; then
    echo "FAILED: pamusb-check should deny access when invoked from an SSH session"
    exit 1
fi
echo -e "Result:\t\t\tPASSED!"

echo -e "Test:\t\t\tpamusb-check log contains remote access daemon message"
pamusb-check "$WHOAMI" 2>/dev/null || true
journalctl -t pam_usb --no-pager -n 20 2>/dev/null | grep -q "One of the parent processes found to be a remote access daemon, denying." && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: expected 'One of the parent processes found to be a remote access daemon, denying.' in syslog"; exit 1; }
