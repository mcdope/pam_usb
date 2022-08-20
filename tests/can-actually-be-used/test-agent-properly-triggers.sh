#!/usr/bin/bash
echo -e "Test:\t\t\tpamusb-agent properly detects device (un)plug and triggers"

# Write agent config with dummy cmds to trigger log entries
sudo sed -i -r 's/<user id="([0-9a-zA-Z]+)">/<user id="\1"><agent event="lock"><cmd>echo $PWD<\/cmd><\/agent><agent event="unlock"><cmd>echo $PWD<\/cmd><\/agent>/g' /etc/security/pam_usb.conf || exit 1

# Enable & start agent
sudo systemctl enable pamusb-agent > /dev/null || exit 1
sudo systemctl start pamusb-agent > /dev/null || exit 1
sleep 5 # make sure agent is up

# "unplug" virtual usb
sync && sync && sync
sudo umount /tmp/fakestick
sudo modprobe -r g_mass_storage || exit 1
sleep 10
sudo tail -n 200 /var/log/auth.log | grep "pamusb-agent\[" | grep "has been removed, locking down user" > /dev/null && echo -e "\t\t\t\tLock event found" || { echo -e "\t\t\t\tNo lock event found!"; exit 1; }

# "plug" virtual usb
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 || exit 1
sleep 10
sudo tail -n 200 /var/log/auth.log | grep "pamusb-agent\[" | grep "Authentication succeeded. Unlocking user"  > /dev/null && echo -e "\t\t\t\tUnlock event found" || { echo -e "\t\t\t\tNo unlock event found!"; exit 1; }

# Disable agent again
sudo systemctl stop pamusb-agent > /dev/null || exit 1
sudo systemctl disable pamusb-agent > /dev/null || exit 1

# Would have failed already if no-pass
echo -e "Result:\t\t\tPASSED!"