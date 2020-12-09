#!/usr/bin/bash

set -e

# Run tests
./test-conf-detects-device.sh && \
./test-conf-adds-device.sh && \
./test-conf-adds-user.sh && \
./test-check-verify-created-config.sh
