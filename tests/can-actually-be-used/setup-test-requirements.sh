#!/usr/bin/bash

set -e

# Prepare testing
./setup-dummyhcd.sh && \
./prepare-mounting.sh && \
./create-image.sh && \
./mount-image.sh
