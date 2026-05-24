#!/usr/bin/bash

set -e

PUSB_FS_TYPE=${PUSB_FS_TYPE:-vfat}

# Partition tables offline — sfdisk works on raw image files for all FS types
echo "Info: preparing partition tables..."
echo 'type=83' | sfdisk virtual_usb.img
echo 'type=83' | sfdisk virtual_usb_alt.img

# Format primary image online
echo "Info: formatting virtual_usb.img as $PUSB_FS_TYPE..."
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
sudo udevadm settle
PRIMARY_DEV=$(lsblk | grep disk | grep 16M | awk '{ print $1 }')
sudo mkfs.$PUSB_FS_TYPE "/dev/${PRIMARY_DEV}1"
sudo modprobe -r g_mass_storage
sudo udevadm settle

# Format alt image online
echo "Info: formatting virtual_usb_alt.img as $PUSB_FS_TYPE..."
sudo modprobe g_mass_storage file=./virtual_usb_alt.img stall=0 removable=y iSerialNumber=1234567891 iProduct=SecondStick
sudo udevadm settle
ALT_DEV=$(lsblk | grep disk | grep 16M | awk '{ print $1 }')
sudo mkfs.$PUSB_FS_TYPE "/dev/${ALT_DEV}1"
sudo modprobe -r g_mass_storage
sudo udevadm settle

# Load primary image and mount it
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
sudo udevadm settle

CREATED_DEVICE=$(lsblk | grep disk | grep 16M | awk '{ print $1 }')
echo "Info: fake device registered as /dev/$CREATED_DEVICE"

mkdir -p /tmp/fakestick
case "$PUSB_FS_TYPE" in
    vfat|exfat)
        sudo mount -t "$PUSB_FS_TYPE" "/dev/${CREATED_DEVICE}1" /tmp/fakestick -o rw,umask=0000 ;;
    ext4)
        sudo mount -t ext4 "/dev/${CREATED_DEVICE}1" /tmp/fakestick -o rw
        sudo chmod 777 /tmp/fakestick ;;
esac
