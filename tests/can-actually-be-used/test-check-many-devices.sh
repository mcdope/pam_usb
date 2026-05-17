#!/usr/bin/bash

WHOAMI=$(whoami)
CONF=/etc/security/pam_usb.conf
TMP_MANY=/tmp/pam_usb_test_many_devices.conf

# Build 9 fake device definitions and 9 user device refs.
# Combined with the 2 real devices already in the config, the user will
# have 11 configured devices — above the old hardcoded limit of 10.
FAKE_DEVICE_BLOCK=""
FAKE_REF_BLOCK=""
for n in $(seq 1 9); do
    FAKE_DEVICE_BLOCK="${FAKE_DEVICE_BLOCK}<device id=\"fake236_${n}\"><vendor>Fake</vendor><model>Fake</model><serial>FAKE236SERIAL${n}</serial><volume_uuid>FAKE-${n}</volume_uuid></device>"
    FAKE_REF_BLOCK="${FAKE_REF_BLOCK}<device>fake236_${n}</device>"
done

sed "s|</devices>|${FAKE_DEVICE_BLOCK}</devices>|" "$CONF" | \
    sed "s|</user>|${FAKE_REF_BLOCK}</user>|" > "$TMP_MANY"

[ $? -eq 0 ] || { echo "FAILED: could not build temp config"; exit 1; }

# Pre-condition: user must now have more than 10 device refs in the temp config.
# <device> (closed tag, no attributes) matches only user refs, not device definitions.
REF_COUNT=$(grep -c "<device>" "$TMP_MANY" || true)
[ "$REF_COUNT" -gt 10 ] || { echo "FAILED: pre-condition: expected >10 device refs, got ${REF_COUNT}"; rm -f "$TMP_MANY"; exit 1; }

echo -e "Test:\t\t\tpamusb-check grants access when user has more than 10 devices configured"
# test2 (iSerialNumber=1234567891) is connected; it is entry 2 of 11 in the temp config.
# This verifies the dynamic device list introduced for issue #236.
pamusb-check --debug --config="$TMP_MANY" "$WHOAMI" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: pamusb-check denied access with >10 devices in config"; rm -f "$TMP_MANY"; exit 1; }

rm -f "$TMP_MANY"
