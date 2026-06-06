#!/usr/bin/bash

# set -e

# Skip build if the module is already installed (e.g. pre-built in the golden image)
if modinfo dummy_hcd &>/dev/null; then
    echo "dummy_hcd already installed, skipping build"
    exit 0
fi

mkdir -p /tmp/src/dummy-hcd
git clone -b main https://github.com/0prichnik/dummy-hcd.git /tmp/src/dummy-hcd/master/
cd /tmp/src/dummy-hcd/master/
make update
make dkms || cat /var/lib/dkms/dummy-hcd/0.1/build/make.log
