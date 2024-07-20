#!/usr/bin/bash
echo -e "Test:\t\t\tpamusb-conf properly add device(s)"
echo -en "pamusb-conf output:\t" # to fake the unhideable python output as expected output :P
sudo pamusb-conf --add-device=test --device=0 --volume=0 --yes | grep "Done" && cat /etc/security/pam_usb.conf | grep "1234567890" > /dev/null && echo -e "Result:\t\t\tPASSED!" || exit 1