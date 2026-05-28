#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-conf --unwhitelist-service removes service from whitelist"
echo -en "pamusb-conf output:\t"
sudo pamusb-conf --whitelist-service=test-unwhitelist-svc --yes > /dev/null 2>&1
sudo pamusb-conf --unwhitelist-service=test-unwhitelist-svc --yes | grep "Done" && ! grep -q "test-unwhitelist-svc" /etc/security/pam_usb.conf && echo -e "Result:\t\t\tPASSED!" || exit 1
