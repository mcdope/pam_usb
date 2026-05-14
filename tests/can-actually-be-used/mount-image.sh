#!/usr/bin/bash

set -e

# Format both images offline — sfdisk and mkfs.vfat work directly on raw files
echo "Info: preparing virtual_usb.img..."
echo 'type=83' | sfdisk virtual_usb.img
mkfs.vfat --offset 2048 virtual_usb.img

echo "Info: preparing virtual_usb_alt.img for second device..."
echo 'type=83' | sfdisk virtual_usb_alt.img
mkfs.vfat --offset 2048 virtual_usb_alt.img

# Load module with primary image
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
echo "Info: sleeping 5s to ensure kernel picks up our new device..."
sleep 5

# Determine device id and mount it
CREATED_DEVICE=$(lsblk | grep disk | grep 16M | awk '{ print $1 }')
echo "Info: fake device registered as /dev/$CREATED_DEVICE"

# Create mountpoint and mount fake stick
mkdir -p /tmp/fakestick
sudo mount -t vfat "/dev/"$CREATED_DEVICE"1" /tmp/fakestick -o rw,umask=0000