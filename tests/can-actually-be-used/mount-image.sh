#!/usr/bin/bash

set -e

PUSB_FS_TYPE=${PUSB_FS_TYPE:-vfat}

# Partition tables offline — sfdisk works on raw image files for all FS types
echo "Info: preparing partition tables..."
echo 'type=83' | sfdisk virtual_usb.img
echo 'type=83' | sfdisk virtual_usb_alt.img

# Format primary image online
echo "Info: formatting virtual_usb.img as $PUSB_FS_TYPE..."
DEVS_BEFORE=$(lsblk -dno NAME | sort)
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
sudo udevadm settle
PRIMARY_DEV=""
for i in $(seq 1 10); do
    PRIMARY_DEV=$(comm -13 <(echo "$DEVS_BEFORE") <(lsblk -dno NAME | sort) | grep -v '^loop' | head -1)
    [ -n "$PRIMARY_DEV" ] && break
    sleep 1
done
[ -z "$PRIMARY_DEV" ] && { echo "Error: no new block device appeared after loading g_mass_storage" >&2; exit 1; }
sudo mkfs.$PUSB_FS_TYPE "/dev/${PRIMARY_DEV}1"
sync
sudo modprobe -r g_mass_storage
sudo udevadm settle

# Format alt image online
echo "Info: formatting virtual_usb_alt.img as $PUSB_FS_TYPE..."
DEVS_BEFORE=$(lsblk -dno NAME | sort)
sudo modprobe g_mass_storage file=./virtual_usb_alt.img stall=0 removable=y iSerialNumber=1234567891 iProduct=SecondStick
sudo udevadm settle
ALT_DEV=""
for i in $(seq 1 10); do
    ALT_DEV=$(comm -13 <(echo "$DEVS_BEFORE") <(lsblk -dno NAME | sort) | grep -v '^loop' | head -1)
    [ -n "$ALT_DEV" ] && break
    sleep 1
done
[ -z "$ALT_DEV" ] && { echo "Error: no new block device appeared after loading g_mass_storage (alt)" >&2; exit 1; }
sudo mkfs.$PUSB_FS_TYPE "/dev/${ALT_DEV}1"
sync
sudo modprobe -r g_mass_storage
sudo udevadm settle

# Load primary image and mount it
DEVS_BEFORE=$(lsblk -dno NAME | sort)
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
sudo udevadm settle
CREATED_DEVICE=""
for i in $(seq 1 10); do
    CREATED_DEVICE=$(comm -13 <(echo "$DEVS_BEFORE") <(lsblk -dno NAME | sort) | grep -v '^loop' | head -1)
    [ -n "$CREATED_DEVICE" ] && break
    sleep 1
done
[ -z "$CREATED_DEVICE" ] && { echo "Error: no new block device appeared after loading g_mass_storage" >&2; exit 1; }
echo "Info: fake device registered as /dev/$CREATED_DEVICE"

mkdir -p /tmp/fakestick
./do-mount.sh "$PUSB_FS_TYPE" "/dev/${CREATED_DEVICE}1"
