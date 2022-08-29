#!/usr/bin/bash
echo -e "Test:\t\t\tpamusb-conf properly detects device(s)"
echo -en "pamusb-conf output (first device):\t" # to fake the unhideable python output as expected output :P
pamusb-conf --list-devices 2>/dev/null | grep "Linux FirstStick (1234567890)" && echo -e "Result:\t\t\tPASSED!" || exit 1
echo -en "pamusb-conf output (first device):\t" # to fake the unhideable python output as expected output :P
pamusb-conf --list-devices 2>/dev/null | grep "Linux SecondStick (1234567891)" && echo -e "Result:\t\t\tPASSED!" || exit 1