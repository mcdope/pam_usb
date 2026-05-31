#!/usr/bin/bash

set -e

CONF_BACKUP=$(mktemp)
cp /etc/security/pam_usb.conf "$CONF_BACKUP"

_restore_conf() {
    sudo sed -i "1{r ${CONF_BACKUP}
d}; 1,\$d" /etc/security/pam_usb.conf 2>/dev/null || true
}

cleanup() {
    local exit_code=$?
    [ $exit_code -eq 0 ] || echo "Error: test suite failed (exit $exit_code), cleaning up..."
    sync 2>/dev/null || true
    sudo umount /tmp/fakestick 2>/dev/null || true
    sudo modprobe -r g_mass_storage 2>/dev/null || true
    sudo udevadm settle 2>/dev/null || true
    _restore_conf
    rm -f "$CONF_BACKUP"
}
trap cleanup EXIT

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
    # Use || true: test-agent-properly-triggers.sh unplugs the device at the end,
    # leaving fakestick unmounted and g_mass_storage already unloaded.
    if [ -n "$PUSB_FS_TYPE_PREV" ]; then
        sync && sync && sync
        sudo umount /tmp/fakestick 2>/dev/null || true
        sudo modprobe -r g_mass_storage 2>/dev/null || true
        sudo udevadm settle
        _restore_conf
    fi

    ./create-image.sh
    ./mount-image.sh

    rm -rf "/home/$(whoami)/.pamusb"

    ./test-keyring-unlock-gnome-installer.sh
    ./test-pinentry-installer.sh
    ./test-conf-detects-device.sh
    ./test-conf-adds-device.sh
    ./test-conf-adds-user.sh
    ./test-conf-doesnt-add-user-twice-but-adds-a-second-device.sh
    ./test-check-verify-created-config.sh
    ./test-conf-reset-pads.sh
    ./test-check-many-devices.sh
    ./test-check-superuser-filtering.sh
    ./test-conf-adds-user-with-superuser.sh
    ./test-check-deny-xrdp-session.sh
    ./test-check-debug-flag.sh
    rm -rf /tmp/fakestick/.pamusb
    ./test-agent-properly-triggers.sh

    PUSB_FS_TYPE_PREV=$PUSB_FS_TYPE
done
