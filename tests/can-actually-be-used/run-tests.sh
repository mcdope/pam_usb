#!/usr/bin/bash

set -e

for PUSB_FS_TYPE in vfat ext4 exfat; do
    export PUSB_FS_TYPE

    if [ "$PUSB_FS_TYPE" = "exfat" ]; then
        sudo modprobe exfat 2>/dev/null || { echo "Info: exfat kernel module not available, skipping exfat run"; continue; }
    fi

    echo ""
    echo "======================================================"
    echo "Running test suite with filesystem: $PUSB_FS_TYPE"
    echo "======================================================"

    # Teardown previous iteration's mount (skip on first)
    if [ -n "$PUSB_FS_TYPE_PREV" ]; then
        sync && sync && sync
        sudo umount /tmp/fakestick
        sudo modprobe -r g_mass_storage
        sudo udevadm settle
    fi

    ./create-image.sh
    ./mount-image.sh

    rm -rf "/home/$(whoami)/.pamusb"

    ./test-keyring-unlock-gnome-installer.sh && \
    ./test-pinentry-installer.sh && \
    ./test-conf-detects-device.sh && \
    ./test-conf-adds-device.sh && \
    ./test-conf-adds-user.sh && \
    ./test-conf-doesnt-add-user-twice-but-adds-a-second-device.sh && \
    ./test-check-verify-created-config.sh && \
    ./test-conf-reset-pads.sh && \
    ./test-check-many-devices.sh && \
    ./test-check-superuser-filtering.sh && \
    ./test-conf-adds-user-with-superuser.sh && \
    ./test-check-deny-xrdp-session.sh && \
    ./test-check-debug-flag.sh && \
    rm -rf /tmp/fakestick/.pamusb && \
    ./test-agent-properly-triggers.sh

    PUSB_FS_TYPE_PREV=$PUSB_FS_TYPE
done
