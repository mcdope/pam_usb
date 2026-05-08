#!/usr/bin/bash

WHOAMI=$(whoami)
CONF=/etc/security/pam_usb.conf
TMP_SU=/tmp/pam_usb_test_su_enabled.conf
TMP_NO_SU=/tmp/pam_usb_test_su_disabled.conf
SERVICE_ENTRY='<service id="test-sudo"><option name="superuser">true</option></service>'

# variant-A: test2 marked superuser + service requires superuser
sed 's|<device>test2</device>|<device superuser="true">test2</device>|' "$CONF" | \
    sed "s|</configuration>|${SERVICE_ENTRY}</configuration>|" > "$TMP_SU"

# variant-B: test2 NOT marked superuser, but service still requires superuser
sed "s|</configuration>|${SERVICE_ENTRY}</configuration>|" "$CONF" > "$TMP_NO_SU"

echo -e "Test:\t\t\tsuperuser device accepted for superuser-required service"
pamusb-check --debug --config="$TMP_SU" --service=test-sudo "$WHOAMI" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: superuser device should be accepted for superuser-required service"; rm -f "$TMP_SU" "$TMP_NO_SU"; exit 1; }

echo -e "Test:\t\t\tnon-superuser device rejected for superuser-required service"
if pamusb-check --debug --config="$TMP_NO_SU" --service=test-sudo "$WHOAMI"; then
    echo "FAILED: non-superuser device should be rejected for superuser-required service"
    rm -f "$TMP_SU" "$TMP_NO_SU"
    exit 1
fi
echo -e "Result:\t\t\tPASSED!"

echo -e "Test:\t\t\tnon-superuser device accepted for non-superuser service (backward compat)"
pamusb-check --debug --config="$TMP_NO_SU" "$WHOAMI" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: non-superuser device should work for non-superuser service"; rm -f "$TMP_SU" "$TMP_NO_SU"; exit 1; }

rm -f "$TMP_SU" "$TMP_NO_SU"
