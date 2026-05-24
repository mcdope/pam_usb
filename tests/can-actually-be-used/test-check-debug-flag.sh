#!/usr/bin/bash
#
# Regression test for pamusb-check.c: debug output must appear when --debug is
# passed or when debug=true is set in the config.
#
# Before the fix (issue #403) pusb_log_init() was called once before
# pusb_conf_init/pusb_conf_parse and never again, so the thread-local
# pusb_log_debug flag stayed 0 regardless of CLI flags or config values.
#
# Both sub-tests use a temp config with enable=false so the "Not enabled,
# exiting..." log_debug() fires before any device lookup — no USB device needed.
# The config still requires a device entry for the user so that pusb_conf_parse()
# succeeds (it returns 0 / error if a user has no configured devices).
#
# __log_debug() writes directly to stderr (no isatty guard), so stderr capture
# works in both interactive and CI (non-TTY) environments.

WHOAMI=$(whoami)
TMP_CONF_A=$(mktemp /tmp/pam_usb_debug_test_A_XXXXXX.conf)
TMP_CONF_B=$(mktemp /tmp/pam_usb_debug_test_B_XXXXXX.conf)
trap 'rm -f "$TMP_CONF_A" "$TMP_CONF_B"' EXIT

# Sub-test A config: enable=false, debug NOT set in config (only via --debug flag)
cat > "$TMP_CONF_A" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<configuration>
  <defaults>
    <option name="enable">false</option>
  </defaults>
  <devices>
    <device id="debugtestdev">
      <vendor>TestVendor</vendor>
      <model>TestModel</model>
      <serial>SERIAL-DEBUG-REGRESSION-001</serial>
      <volume_uuid>DEAD-BEEF</volume_uuid>
    </device>
  </devices>
  <users>
    <user id="$WHOAMI">
      <device>debugtestdev</device>
    </user>
  </users>
  <services/>
</configuration>
EOF

# Sub-test B config: enable=false, debug=true set in config (no --debug flag)
cat > "$TMP_CONF_B" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<configuration>
  <defaults>
    <option name="enable">false</option>
    <option name="debug">true</option>
  </defaults>
  <devices>
    <device id="debugtestdev">
      <vendor>TestVendor</vendor>
      <model>TestModel</model>
      <serial>SERIAL-DEBUG-REGRESSION-001</serial>
      <volume_uuid>DEAD-BEEF</volume_uuid>
    </device>
  </devices>
  <users>
    <user id="$WHOAMI">
      <device>debugtestdev</device>
    </user>
  </users>
  <services/>
</configuration>
EOF

echo -e "Test:\t\t\tpamusb-check --debug produces debug output on stderr"
debug_output=$(pamusb-check --debug --config="$TMP_CONF_A" "$WHOAMI" 2>&1 || true)
echo "$debug_output" | grep -q "Not enabled, exiting" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: expected 'Not enabled, exiting' in stderr with --debug flag"; exit 1; }

echo -e "Test:\t\t\tpamusb-check produces debug output when debug=true in config"
debug_output=$(pamusb-check --config="$TMP_CONF_B" "$WHOAMI" 2>&1 || true)
echo "$debug_output" | grep -q "Not enabled, exiting" && \
    echo -e "Result:\t\t\tPASSED!" || \
    { echo "FAILED: expected 'Not enabled, exiting' in stderr with debug=true in config"; exit 1; }
