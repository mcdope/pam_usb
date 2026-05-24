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
DEVS_AFTER=$(lsblk -dno NAME | sort)
PRIMARY_DEV=$(comm -13 <(echo "$DEVS_BEFORE") <(echo "$DEVS_AFTER") | grep -v '^loop' | head -1)
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
DEVS_AFTER=$(lsblk -dno NAME | sort)
ALT_DEV=$(comm -13 <(echo "$DEVS_BEFORE") <(echo "$DEVS_AFTER") | grep -v '^loop' | head -1)
[ -z "$ALT_DEV" ] && { echo "Error: no new block device appeared after loading g_mass_storage (alt)" >&2; exit 1; }
sudo mkfs.$PUSB_FS_TYPE "/dev/${ALT_DEV}1"
sync
sudo modprobe -r g_mass_storage
sudo udevadm settle

# Load primary image and mount it
DEVS_BEFORE=$(lsblk -dno NAME | sort)
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
sudo udevadm settle
DEVS_AFTER=$(lsblk -dno NAME | sort)
CREATED_DEVICE=$(comm -13 <(echo "$DEVS_BEFORE") <(echo "$DEVS_AFTER") | grep -v '^loop' | head -1)
[ -z "$CREATED_DEVICE" ] && { echo "Error: no new block device appeared after loading g_mass_storage" >&2; exit 1; }
echo "Info: fake device registered as /dev/$CREATED_DEVICE"

mkdir -p /tmp/fakestick
./do-mount.sh "$PUSB_FS_TYPE" "/dev/${CREATED_DEVICE}1"
