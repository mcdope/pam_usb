#!/usr/bin/bash
#
# Regression test for local.c: XRDP_SESSION env var must cause denial.
# The check fires in pusb_local_login() before any device or TTY inspection,
# so no USB device is required for this test.

WHOAMI=$(whoami)

echo -e "Test:\t\t\tpamusb-check denies access when XRDP_SESSION is set"
if XRDP_SESSION=1 pamusb-check "$WHOAMI" 2>/dev/null; then
    echo "FAILED: pamusb-check should deny access when XRDP_SESSION is set (deny_remote defaults to true)"
    exit 1
fi
echo -e "Result:\t\t\tPASSED!"

echo -e "Test:\t\t\tpamusb-check log contains XRDP session message"
XRDP_SESSION=testvalue pamusb-check "$WHOAMI" 2>&1 | grep -q "XRDP session detected (testvalue)" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: expected 'XRDP session detected (testvalue)' in output"; exit 1; }
