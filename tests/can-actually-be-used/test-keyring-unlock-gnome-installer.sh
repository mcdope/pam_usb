#!/usr/bin/bash
set -e
echo -e "Test:\t\t\tpamusb-keyring-unlock-gnome --install / --uninstall"

FAKE_HOME=$(mktemp -d)
trap "rm -rf $FAKE_HOME" EXIT

TOOL=$(which pamusb-keyring-unlock-gnome)

# --install: pipe password via stdin (non-interactive path, no stty needed)
printf 'testpassword\n' | HOME=$FAKE_HOME $TOOL --install

# verify password file exists with mode 0600
[ -f "$FAKE_HOME/.pamusb/.keyring_unlock_password" ]
PERMS=$(stat -c "%a" "$FAKE_HOME/.pamusb/.keyring_unlock_password")
[ "$PERMS" = "600" ]
grep -q 'UNLOCK_PASSWORD="testpassword"' "$FAKE_HOME/.pamusb/.keyring_unlock_password"

# verify autostart desktop file created with autostart enabled
[ -f "$FAKE_HOME/.config/autostart/pamusb-keyring-unlock-gnome.desktop" ]
grep -q 'X-GNOME-Autostart-enabled=true' "$FAKE_HOME/.config/autostart/pamusb-keyring-unlock-gnome.desktop"

# verify gnome-keyring-secrets override created to prevent daemon from racing our script
[ -f "$FAKE_HOME/.config/autostart/gnome-keyring-secrets.desktop" ]
grep -q 'Hidden=true' "$FAKE_HOME/.config/autostart/gnome-keyring-secrets.desktop"

# --uninstall
HOME=$FAKE_HOME $TOOL --uninstall

# verify all three files are removed
[ ! -f "$FAKE_HOME/.pamusb/.keyring_unlock_password" ]
[ ! -f "$FAKE_HOME/.config/autostart/pamusb-keyring-unlock-gnome.desktop" ]
[ ! -f "$FAKE_HOME/.config/autostart/gnome-keyring-secrets.desktop" ]

echo -e "Result:\t\t\tPASSED!"
