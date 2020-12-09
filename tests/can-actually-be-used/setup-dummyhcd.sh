#!/usr/bin/bash

# set -e

mkdir -p /tmp/src/dummy-hcd
git clone -b main https://github.com/0prichnik/dummy-hcd.git /tmp/src/dummy-hcd/master/
cd /tmp/src/dummy-hcd/master/
make update
make dkms || cat /var/lib/dkms/dummy-hcd/0.1/build/make.log
cat /var/lib/dkms/dummy-hcd/0.1/build/make.log