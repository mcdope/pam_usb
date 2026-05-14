#!/usr/bin/bash

set -e

# Make sure no old pads are around
rm -rf /home/`whoami`/.pamusb

# Run tests
./test-keyring-unlock-gnome-installer.sh && \
./test-conf-detects-device.sh && \
./test-conf-adds-device.sh && \
./test-conf-adds-user.sh && \
./test-conf-doesnt-add-user-twice-but-adds-a-second-device.sh && \
./test-check-verify-created-config.sh && \
./test-conf-reset-pads.sh && \
./test-check-superuser-filtering.sh && \
rm -rf /tmp/fakestick/.pamusb && \
./test-agent-properly-triggers.sh
