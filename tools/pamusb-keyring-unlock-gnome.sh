#!/bin/bash

# Check if password file exists and has correct permissions
if [ ! -f "~/.keyring_unlock_password" ]; then
    exit 0
fi

PERMISSIONS=`stat -c "%a %n" ~/.keyring_unlock_password | awk '{print $1}'`
if [[ $PERMISSIONS -lt 600 || $PERMISSIONS -gt 600 ]]
then
    echo "Error: bad permissions on ~/.keyring_unlock_password. Please change them to 0600."
    exit 1
fi

# Read UNLOCK_PASSWORD from ~/.keyring_unlock_password
source ~/.keyring_unlock_password

# Kill maybe autostarted daemon
killall -q gnome-keyring-daemon

# Perform unlock and start daemon
echo -n $UNLOCK_PASSWORD | gnome-keyring-daemon --daemonize --login && gnome-keyring-daemon --start

exit 0