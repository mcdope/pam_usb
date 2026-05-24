#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-conf doesn't add user(s) twice, but it adds a second device for an existing user"

PUSB_FS_TYPE=${PUSB_FS_TYPE:-vfat}

# "unplug" virtual usb
sync && sync && sync
sudo umount /tmp/fakestick
sudo modprobe -r g_mass_storage || exit 1
sudo udevadm settle

# "plug" another virtual usb (pre-formatted by mount-image.sh)
DEVS_BEFORE=$(lsblk -dno NAME | sort)
sudo modprobe g_mass_storage file=./virtual_usb_alt.img stall=0 removable=y iSerialNumber=1234567891 iProduct=SecondStick || exit 1
sudo udevadm settle
DEVS_AFTER=$(lsblk -dno NAME | sort)
CREATED_DEVICE=$(comm -13 <(echo "$DEVS_BEFORE") <(echo "$DEVS_AFTER") | grep -v '^loop' | head -1)
./do-mount.sh "$PUSB_FS_TYPE" "/dev/${CREATED_DEVICE}1"

echo -en "pamusb-conf --add-device output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-device=test2 --device=0 --volume=0 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "1234567891" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1

echo -en "pamusb-conf --add-user output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-user=`whoami` --device=1 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "<device>test2</device>" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1
