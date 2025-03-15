#!/usr/bin/bash

set -e

# Load module with image
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 iProduct=FirstStick
echo "Info: sleeping 5s to ensure kernel picks up our new device..."
sleep 5

# Determine device id, create partition and format it
CREATED_DEVICE=$(lsblk | grep disk | grep 16M | awk '{ print $1 }')
echo "Info: fake device registered as /dev/$CREATED_DEVICE"
echo "Info: creating partition..."
echo 'type=83' | sudo sfdisk /dev/$CREATED_DEVICE
echo "Info: formatting partition as vfat..."
sudo mkfs.vfat "/dev/"$CREATED_DEVICE"1"

# Create mountpoint and mount fake stick
mkdir -p /tmp/fakestick
sudo mount -t vfat "/dev/"$CREATED_DEVICE"1" /tmp/fakestick -o rw,umask=0000
