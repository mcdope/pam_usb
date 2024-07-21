![Build](https://github.com/mcdope/pam_usb/workflows/Can%20be%20built%20successfully/badge.svg) ![Packaging](https://github.com/mcdope/pam_usb/workflows/Packaging%20(deb,%20rpm,%20zst)/badge.svg) ![Functional testing](https://github.com/mcdope/pam_usb/workflows/Build%20&%20test/badge.svg) ![CodeQL](https://github.com/mcdope/pam_usb/workflows/CodeQL/badge.svg) ![DevSkim](https://github.com/mcdope/pam_usb/workflows/DevSkim/badge.svg) ![Nightly builds (tar.gz, deb, rpm, zst)](https://github.com/mcdope/pam_usb/actions/workflows/nightly-build-on-own-server.yml/badge.svg)


pam\_usb
========

pam\_usb provides hardware authentication for Linux using ordinary removable media. Tested are flash sticks and storage cards, but it should work with harddrives, SSDs and even floppies (at least USB based) too.

It works with any application supporting PAM, such as _su_ and login managers (_GDM_, _KDM_).

Features
--------

* `Password-less authentication.` Use your removable media for authentication, don't type passwords anymore (or add a second factor).
* `Device auto probing.` You don't need to mount the device, or even to configure the device location (_sda1_, _sdb1_, etc). pam\_usb.so will automatically locate the device using `UDisks` and access its data by itself.
* `Two-factor authentication.` Archive greater security by requiring both the removable media and the password to authenticate the user.
* `Non-intrusive.` pam\_usb doesn't require any modifications of the USB storage device to work (no additional partitions required).
* USB Serial number, model and vendor verification.
* Support for **One Time Pads** authentication.
* Support for all kind of removable devices (SD, MMC, etc).
* Can optionally unlock your GNOME keyring

Tools
-----
* `pamusb-agent`: trigger actions (such as locking the screen) upon device authentication and removal.
* `pamusb-conf`: configuration helper.
* `pamusb-check`: integrate pam\_usb's authentication engine within your scripts or applications.
* `pamusb-keyring-unlock-gnome`: utility to unlock the gnome-keyring on login with pam_usb

Getting Started
----------------
* [Install](https://github.com/mcdope/pam_usb/wiki/Install) pam_usb on your system
* Read the [Getting Started Guide](https://github.com/mcdope/pam_usb/wiki/Getting-Started)
* Have a look at the [Configuration File Reference](https://github.com/mcdope/pam_usb/wiki/Configuration)
* Problem? See the [Troubleshooting Guide](https://github.com/mcdope/pam_usb/wiki/Troubleshooting)

History of this PAM module
----------------
This software was initially created by Andrea Luzzardi (https://github.com/aluzzardi/pam_usb/) but is
unmaintained for quite some years by now. While the years passed the community continued to work on it
but there was never a centralized place merging all those improvement. The goal of this repo is to provide
exactly that, resulting in an up-to-date version. See [Install](https://github.com/mcdope/pam_usb/wiki/Install)
for installation instructions and download options for prebuilt binaries.

This repo is mainly based on community improvements from 
 * Pekka Helenius (https://github.com/Fincer/pam_usb) - Agent improvements and Arch packaging support
 * Luka Novsak (https://github.com/luka-n/pam_usb) - UDisk2 port
 * "IGP" (https://github.com/IGP/pam_usb)" (many small improvements)
 * "McDope" (https://github.com/mcdope/pam_usb) (this repo), 
 * ... and others. 
 
See the commit history for details. You can find a list of all contributors in the `AUTHORS` file. 

The last official release was 0.5.0 btw, some private packages used 0.6.0 to override the upstream provided 
version but those varied in changes from 0.5.0. This repo started at 0.7.0 and includes all contributions up
to this point I'm aware of (Python3 port, UDisks2 support, other smaller ones) or did myself.
