#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-check recovers from orphaned .tmp pad files (issue #438)"

SYS_PAD="$HOME/.pamusb/test2.pad"
DEV_PAD="/tmp/fakestick/.pamusb/$USER.$HOSTNAME.pad"
SYS_TMP="${SYS_PAD}.tmp"
DEV_TMP="${DEV_PAD}.tmp"

# Ensure the simulated orphans never outlive the test, even on failure or interruption
cleanup() {
    rm -f "$SYS_TMP" "$DEV_TMP"
}
trap cleanup EXIT

# Pre-condition: valid pads must exist from the previous pamusb-check run
[ -f "$SYS_PAD" ] || { echo -e "Result:\t\t\tFAILED (pre-condition: system pad not found)"; exit 1; }
[ -f "$DEV_PAD" ] || { echo -e "Result:\t\t\tFAILED (pre-condition: device pad not found)"; exit 1; }

# Force the next check to regenerate: age the system pad past the default 1h expiration
touch -d "2 hours ago" "$SYS_PAD" || { echo -e "Result:\t\t\tFAILED (could not age system pad mtime)"; exit 1; }

# Simulate leftovers from an update that was killed before cleanup (e.g. SIGKILL).
# Without the fix these block every future update with "File exists" and deny auth.
echo "orphan" > "$SYS_TMP" || { echo -e "Result:\t\t\tFAILED (could not create stale system .tmp)"; exit 1; }
echo "orphan" > "$DEV_TMP" || { echo -e "Result:\t\t\tFAILED (could not create stale device .tmp)"; exit 1; }

# pamusb-check must succeed: the orphans are cleaned up under the update lock and pads rotate
pamusb-check --debug "$USER" > /dev/null 2>&1 || { echo -e "Result:\t\t\tFAILED (pamusb-check denied auth despite stale .tmp)"; exit 1; }

# The orphaned temp files must be gone afterwards
[ ! -f "$SYS_TMP" ] || { echo -e "Result:\t\t\tFAILED (stale system .tmp not cleaned up)"; exit 1; }
[ ! -f "$DEV_TMP" ] || { echo -e "Result:\t\t\tFAILED (stale device .tmp not cleaned up)"; exit 1; }

# Pads must still be present (regenerated)
[ -f "$SYS_PAD" ] || { echo -e "Result:\t\t\tFAILED (system pad missing after recovery)"; exit 1; }
[ -f "$DEV_PAD" ] || { echo -e "Result:\t\t\tFAILED (device pad missing after recovery)"; exit 1; }

echo -e "Result:\t\t\tPASSED!"
