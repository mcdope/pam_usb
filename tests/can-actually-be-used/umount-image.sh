#!/usr/bin/bash
sync && sync && sync
sudo umount /tmp/fakestick
sudo modprobe -r g_mass_storage

rm virtual_usb.img
rm -rf /tmp/fakestick
rm -rf /home/`whoami`/.pamusb
