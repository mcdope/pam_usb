FROM ubuntu:20.04
ENV DEBIAN_FRONTEND noninteractive
ENV TERM=xterm
WORKDIR /usr/local/src/pam_usb

RUN apt-get -qqy update
RUN apt-get -qqy install debhelper devscripts dh-systemd wget build-essential git libreadline-dev libxml2-dev libpam0g-dev libudisks2-dev libglib2.0-dev gir1.2-udisks-2.0 python3 python3-gi