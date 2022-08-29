#!/usr/bin/bash
sync && sync && sync
sudo umount /tmp/fakestick
sudo umount /tmp/fakestick_alt
sudo modprobe -r g_mass_storage

rm virtual_usb.img
rm virtual_usb_alt.img
rm -rf /tmp/fakestick
rm -rf /tmp/fakestick_alt
rm -rf /home/`whoami`/.pamusb
