#!/usr/bin/bash

# @todo: this will fail if other devices are present. its missing a step to actually determine the number used internally
# @todo: check if the numbering used in debconf is correct after all

echo -e "Test:\t\t\tpamusb-conf properly add user(s)"
echo -en "pamusb-conf output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-user=`whoami` --device=0 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "<device>test</device>" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1