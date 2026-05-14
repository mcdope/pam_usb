#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-conf --reset-pads resets pads for all connected devices"

# Pre-condition: pads for test2 must exist from the previous pamusb-check run
[ -f "/home/$USERNAME/.pamusb/test2.pad" ] || { echo -e "Result:\t\t\tFAILED (pre-condition: system pad for test2 not found)"; exit 1; }
[ -f "/tmp/fakestick/.pamusb/$USERNAME.$HOSTNAME.pad" ] || { echo -e "Result:\t\t\tFAILED (pre-condition: device pad not found)"; exit 1; }

# Run --reset-pads and capture output
echo -en "pamusb-conf --reset-pads output:\t"
RESET_OUTPUT=$(sudo pamusb-conf --reset-pads=$USERNAME 2>&1)
echo "$RESET_OUTPUT" | head -1

# test2 pads must be gone
[ ! -f "/home/$USERNAME/.pamusb/test2.pad" ] || { echo -e "Result:\t\t\tFAILED (system pad for test2 still exists)"; exit 1; }
[ ! -f "/tmp/fakestick/.pamusb/$USERNAME.$HOSTNAME.pad" ] || { echo -e "Result:\t\t\tFAILED (device pad still exists)"; exit 1; }

# test device (not connected) must have been reported as skipped
echo "$RESET_OUTPUT" | grep -q "test.*not connected" || { echo -e "Result:\t\t\tFAILED (expected skip warning for disconnected device 'test')"; exit 1; }

# pamusb-check must still succeed and regenerate pads
pamusb-check --debug $USERNAME > /dev/null 2>&1 || { echo -e "Result:\t\t\tFAILED (pamusb-check failed after pad reset)"; exit 1; }
[ -f "/home/$USERNAME/.pamusb/test2.pad" ] || { echo -e "Result:\t\t\tFAILED (system pad for test2 not regenerated)"; exit 1; }
[ -f "/tmp/fakestick/.pamusb/$USERNAME.$HOSTNAME.pad" ] || { echo -e "Result:\t\t\tFAILED (device pad not regenerated)"; exit 1; }

echo -e "Result:\t\t\tPASSED!"
