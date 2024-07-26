%define _topdir         /usr/local/src/pam_usb/fedora
%define name            pam_usb 
%define release         1
%define version         0.8.5
%define buildroot       %{_topdir}/%{name}‑%{version}‑root

BuildRoot: %{buildroot}
Summary:   pam_usb
License:   GPLv2
URL:       https://github.com/mcdope/pam_usb/
Packager:  Tobias Bäumer <tobiasbaeumer@gmail.com>
Name:      %{name}
Version:   %{version}
Release:   %{release}
Prefix:    /usr
Group:     System Environment/Base
Requires:  pam python3-gobject gawk

%description
Adds auth over usb-stick to pam
 Provides a new pam module, pam_usb.so, that can be used in pam.d configurations

%prep
cd %{_topdir}/BUILD
rm -rf %{name}-%{version}
mkdir %{name}-%{version}
rsync -a %{_topdir}/../ %{name}-%{version} --exclude fedora --exclude arch_linux --exclude .build --exclude .github  --exclude .idea  --exclude .vscode
cd %{name}-%{version}
chmod -Rf a+rX,u+w,g-w,o-w .

%build
cd %{_topdir}/BUILD/%{name}-%{version}
make all

%install
cd %{_topdir}/BUILD/%{name}-%{version}
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}/usr/share/pam-configs

%files
%attr(0755,root,root) /lib64/security/pam_usb.so
%attr(0755,root,root) /usr/bin/pamusb-agent
%attr(0755,root,root) /usr/bin/pamusb-check
%attr(0755,root,root) /usr/bin/pamusb-conf
%attr(0755,root,root) /usr/bin/pamusb-keyring-unlock-gnome

%config(noreplace) %attr(0644,root,root) /etc/security/pam_usb.conf

%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-agent.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-check.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-conf.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-keyring-unlock-gnome.1.gz
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/CONFIGURATION
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/QUICKSTART
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/SECURITY
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/TROUBLESHOOTING

%changelog
* Thu Jul 26 2024 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.5
- [Feature] Support multiple devices per user
- [Enhancement] Misc. memory and string handling stuff
- [Enhancement] Deny if pads can't be updated
- [Enhancement] SELinux! There is now a profile for Fedora 40 (not installed automatically!) and a doc on how to create your own (see Wiki)
- [Bugfix] LC_ALL usage

* Thu Jan 04 2024 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.4
- [Bugfix] loginctl usage was not sh compatible
- [Bugfix] Misc. fixes related to memory handling
- [Enhancement] Don't check every element of ut_addr_v6
- [Enhancement] Service whitelist is now user configurable
- [Enhancement] Whitelist additions: lxdm, xscreensaver, klockscreen

* Tue Aug 30 2022 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.3-1
- [Enhancement] Install pam-auth-update config only on systems having it
- [Feature] pamusb-conf now has a --reset-pads=username option
- [Bugfix] Fix RHOST check triggering on empty value
- [Bugfix] Whitelist pamusb-agent for remoteness-check
- [Bugfix] Fix "tty from displayserver" remoteness-check method
- [Docs] Update manpages and text files
- [Bugfix] Fix some usages of tmux being able to circumvent localcheck

* Sun May 22 2022 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.2-1
- First version being packaged for RPM
- [Tools/Docs] Add pamusb-keyring-unlock-gnome, to allow unlocking the GNOME keyring (#11)
- [Bugfix] Whitelist "login" service name to prevent insta-logout on TTY shells (#115)
- [Bugfix] Check PAM_RHOST if deny_remote is enable to fix vsftpd auth breaking down (#100)
- [Bugfix] Fix type for argument to stat (community contribution)
- [Docs] Added code of conduct (#106) and updated AUTHORS
- [Makefile] Fix LIBDIR on non-debian systems