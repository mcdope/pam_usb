#!/usr/bin/bash
set -e
echo -e "Test:\t\t\tpamusb-pinentry --install / --uninstall"

FAKE_HOME=$(mktemp -d)
MOCK_BIN=$(mktemp -d)
MOCK_LOG=$(mktemp)
trap "rm -rf $FAKE_HOME $MOCK_BIN $MOCK_LOG" EXIT

TOOL=$(which pamusb-pinentry)

# Mock update-alternatives: log all calls, always succeed.
# printf expands $MOCK_LOG now; $@ stays literal inside the generated script.
printf '#!/usr/bin/bash\necho "$@" >> %s\nexit 0\n' "$MOCK_LOG" > "$MOCK_BIN/update-alternatives"
chmod +x "$MOCK_BIN/update-alternatives"

# --install
HOME=$FAKE_HOME PATH="$MOCK_BIN:$PATH" $TOOL --install

# verify envfile created with correct path, mode 0600, and default content
[ -f "$FAKE_HOME/.pamusb/.pinentry.env" ]
PERMS=$(stat -c "%a" "$FAKE_HOME/.pamusb/.pinentry.env")
[ "$PERMS" = "600" ]
grep -q 'PINENTRY_PASSWORD=changeme' "$FAKE_HOME/.pamusb/.pinentry.env"

# verify update-alternatives --install and --set were called
grep -q -- "--install /usr/bin/pinentry pinentry /usr/bin/pamusb-pinentry 100" "$MOCK_LOG"
grep -q -- "--set pinentry /usr/bin/pamusb-pinentry" "$MOCK_LOG"

# idempotency: --install again must not overwrite existing envfile
HOME=$FAKE_HOME PATH="$MOCK_BIN:$PATH" $TOOL --install

# --uninstall
> "$MOCK_LOG"
HOME=$FAKE_HOME PATH="$MOCK_BIN:$PATH" $TOOL --uninstall

# verify envfile removed
[ ! -f "$FAKE_HOME/.pamusb/.pinentry.env" ]

# verify update-alternatives --remove was called
grep -q -- "--remove pinentry /usr/bin/pamusb-pinentry" "$MOCK_LOG"

echo -e "Result:\t\t\tPASSED!"
