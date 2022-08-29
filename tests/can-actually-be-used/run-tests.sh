#!/usr/bin/bash

set -e

# Make sure no old pads are around
rm -rf /home/`whoami`/.pamusb

# Run tests
./test-conf-detects-device.sh && \
./test-conf-adds-devices.sh && \
./test-conf-adds-user.sh && \
./test-conf-doesnt-add-user-twice-but-adds-a-second-device.sh && \
./test-check-verify-created-config.sh && \
./test-agent-properly-triggers.sh
