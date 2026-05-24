#!/usr/bin/bash

set -e

# Prepare kernel modules for USB emulation; image creation and mounting is
# handled by run-tests.sh per filesystem-type iteration.
./setup-dummyhcd.sh && \
./prepare-mounting.sh
