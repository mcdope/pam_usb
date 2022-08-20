#!/usr/bin/bash
echo -e "Test:\t\t\tpamusb-agent properly detects device (un)plug and triggers"

# Write agent config with dummy cmds to trigger log entries
sed -i -r 's/<user id="([0-9a-zA-Z]+)">/<user id="\1"><agent event="lock"><cmd>echo $PWD<\/cmd><\/agent><agent event="unlock"><cmd>echo $PWD<\/cmd><\/agent>/g' /etc/security/pam_usb.conf || exit 1

# Enable & start agent
sudo systemctl enable pamusb-agent || exit 1
sudo systemctl start pamusb-agent || exit 1

# "unplug" virtual usb
sudo modprobe -r g_mass_storage || exit 1
sleep 5
tail -n 20 /var/log/auth.log | grep "has been removed, locking down user" || exit 1

# "plug" virtual usb
sudo modprobe g_mass_storage file=./virtual_usb.img stall=0 removable=y iSerialNumber=1234567890 || exit 1
sleep 5
tail -n 20 /var/log/auth.log | grep "Authentication succeeded. Unlocking user" || exit 1

# Disable agent again
sudo systemctl stop pamusb-agent || exit 1
sudo systemctl disable pamusb-agent || exit 1

# Would have failed already if no-pass
echo -e "Result:\t\t\tPASSED!"