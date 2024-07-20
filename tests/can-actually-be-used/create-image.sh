#!/usr/bin/bash

set -e

fallocate -l 16M virtual_usb.img
fallocate -l 16M virtual_usb_alt.img
