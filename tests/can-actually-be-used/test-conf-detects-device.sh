#!/usr/bin/bash
echo -e "Test:\t\t\tpamusb-conf properly detects device(s)"
# Poll until UDisks2 has enumerated the device (slow on QEMU TCG armhf with 2 vCPUs)
DEVICE_LINE=""
for i in $(seq 1 30); do
    DEVICE_LINE=$(pamusb-conf --list-devices 2>/dev/null | grep "Linux File-Stor Gadget (1234567890)")
    [ -n "$DEVICE_LINE" ] && break
    sleep 2
done
echo -en "pamusb-conf output:\t" # to fake the unhideable python output as expected output :P
[ -n "$DEVICE_LINE" ] && echo -e "$DEVICE_LINE\nResult:\t\t\tPASSED!" || exit 1