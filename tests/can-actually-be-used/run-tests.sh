#!/usr/bin/bash

set -e

# Prepare testing
./setup-dummyhcd.sh && \
./prepare-mounting.sh && \
./create-image.sh && \
./mount-image.sh

# Run tests
./test-conf-detects-device.sh && \
./test-conf-adds-device.sh && \
./test-conf-adds-user.sh && \
./test-check-verify-created-config.sh

# Unmount image
./umount-image.sh
