#!/usr/bin/bash

set -e

# Make sure no old pads are around
rm -rf /home/`whoami`/.pamusb

# Run tests
./test-conf-detects-device.sh && \
./test-conf-adds-device.sh && \
./test-conf-adds-user.sh && \
./test-check-verify-created-config.sh
