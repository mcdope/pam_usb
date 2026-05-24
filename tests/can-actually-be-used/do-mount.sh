#!/usr/bin/bash

# Usage: do-mount.sh <fs_type> <device>
# Mounts <device> to /tmp/fakestick with options appropriate for <fs_type>.

set -e

FS_TYPE=$1
DEVICE=$2

case "$FS_TYPE" in
    vfat|exfat)
        sudo mount -t "$FS_TYPE" "$DEVICE" /tmp/fakestick -o rw,umask=0000 ;;
    ext4)
        sudo mount -t ext4 "$DEVICE" /tmp/fakestick -o rw
        sudo chmod 777 /tmp/fakestick ;;
    *)
        echo "Error: unsupported filesystem type '$FS_TYPE'" >&2
        exit 1 ;;
esac
