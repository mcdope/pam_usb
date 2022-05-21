%define _topdir         $PWD/..
%define name            pam_usb 
%define release         1
%define version         0.8.2
%define buildroot       %{_topdir}/%{name}‑%{version}‑root

BuildRoot:    %{buildroot}
Summary:         pam_usb
License:         GPLv2
Name:             %{name}
Version:         %{version}
Release:         %{release}
Source:         %{name}‑%{version}.tar.gz
Prefix:         /usr
Group:             Development/Tools

%description
Adds auth over usb-stick to pam
 Provides a new pam module, pam_usb.so, that can be used in pam.d/common-auth

%prep
%setup ‑q

%build
./configure
make

%install
make install prefix=$RPM_BUILD_ROOT/usr

%files
%defattr(‑,root,root)
/usr/lib64/pam_usb.so
/usr/bin/pamusb-check
/usr/bin/pamusb-agent
/usr/bin/pamusb-keyring-unlock-gnome

%conf /etc/security/pam_usb.conf

%doc %attr(0444,root,root) /usr/local/share/man/man1/pamusb-agent.1
%doc %attr(0444,root,root) /usr/local/share/man/man1/pamusb-check.1
%doc %attr(0444,root,root) /usr/local/share/man/man1/pamusb-conf.1
%doc %attr(0444,root,root) /usr/local/share/man/man1/pamusb-keyring-unlock-gnome.1
%doc %attr(0444,root,root) /usr/share/doc/pam_usb/CONFIGURATION
%doc %attr(0444,root,root) /usr/share/doc/pam_usb/QUICKSTART