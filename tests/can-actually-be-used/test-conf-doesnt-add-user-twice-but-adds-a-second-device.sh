#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-conf doesn't add user(s) twice, but it adds a second device for an existing user"

# "unplug" virtual usb
sync && sync && sync
sudo umount /tmp/fakestick
sudo modprobe -r g_mass_storage || exit 1
sleep 10

# "plug" another virtual usb
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567891 iProduct=SecondStick || exit 1
sleep 10

echo -en "pamusb-conf --add-device output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-device=test2 --device=0 --volume=0 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "1234567891" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1

echo -en "pamusb-conf --add-user output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-user=`whoami` --device=0 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "<device>test2</device>" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1
