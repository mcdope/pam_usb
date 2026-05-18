#!/usr/bin/bash

WHOAMI=$(whoami)
CONF=/etc/security/pam_usb.conf
SERVICE_ENTRY='<services><service id="test-sudo-via-conf"><option name="superuser">true</option></service></services>'

echo -e "Test:\t\t\tpamusb-conf --add-user --superuser writes superuser attribute to config"
echo -en "pamusb-conf output:\t"
sudo pamusb-conf --add-user="$WHOAMI" --device=1 --superuser --yes | grep "Done" && \
    grep '<device superuser="true">test2</device>' "$CONF" > /dev/null && \
    echo -e "Result:\t\t\tPASSED!" || exit 1

echo -e "Test:\t\t\tsuperuser device written by pamusb-conf is accepted by pamusb-check"
TMP_CONF=$(mktemp /tmp/pam_usb_test_XXXXXX.conf)
sed "s|</configuration>|${SERVICE_ENTRY}</configuration>|" "$CONF" > "$TMP_CONF"
pamusb-check --debug --config="$TMP_CONF" --service=test-sudo-via-conf "$WHOAMI" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: device marked superuser by pamusb-conf should be accepted"; rm -f "$TMP_CONF"; exit 1; }
rm -f "$TMP_CONF"
