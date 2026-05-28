#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-conf --whitelist-service adds service to whitelist"
echo -en "pamusb-conf output:\t"
sudo pamusb-conf --whitelist-service=test-whitelist-svc --yes | grep "Done" && grep -q '<service id="test-whitelist-svc"><option name="deny_remote">false</option></service>' /etc/security/pam_usb.conf && echo -e "Result:\t\t\tPASSED!" || exit 1
