%define _topdir         /usr/local/src/pam_usb/fedora
%define name            pam_usb 
%define release         1
%define version         0.9.2
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
BuildRequires: libudisks2-devel libxml2-devel libevdev-devel
Requires:  pam python3-gobject gawk libevdev

%description
Adds auth over usb-stick to pam
 Provides a new pam module, pam_usb.so, that can be used in pam.d configurations

%prep
cd %{_topdir}/BUILD
rm -rf %{name}-%{version}
mkdir %{name}-%{version}
shopt -s extglob
cp -a %{_topdir}/../!(fedora|arch_linux|debian|.build|.github|.idea|.vscode|tests|.claude|CLAUDE.md|AGENTS.md|.cursorrules|.pytest_cache|.gitignore|.git-blame-ignore-revs) %{name}-%{version}
cd %{name}-%{version}
chmod -Rf a+rX,u+w,g-w,o-w .

%build
cd %{_topdir}/BUILD/%{name}-%{version}
%make_build all

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
%attr(0755,root,root) /usr/bin/pinentry-pamusb

%config(noreplace) %attr(0644,root,root) /etc/security/pam_usb.conf
%config(noreplace) %attr(0644,root,root) /usr/lib/systemd/system/polkit-agent-helper@.service.d/systemd-polkit-agent-helper-pamusb.conf

%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-agent.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-check.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-conf.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pamusb-keyring-unlock-gnome.1.gz
%doc %attr(0644,root,root) /usr/share/man/man1/pinentry-pamusb.1.gz
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/CONFIGURATION
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/QUICKSTART
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/SECURITY
%doc %attr(0644,root,root) /usr/share/doc/pam_usb/TROUBLESHOOTING

%changelog

* Sat May 23 2026 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.9.2
- [Enhancement] Emit log_error when superuser device filtering removes all devices (#399)
- [Security] Fixed GHSA-rmp6-wfrq-wrrc (xfree() heap memory not cleared before free) (#374)
- [Security] Fixed GHSA-64g7-3jrm-4jc3 (O_NOFOLLOW missing in evdev open, symlink following) (#376)
- [Security] Fixed GHSA-hxh6-9574-5vp6 (O_EXCL missing on pad temp file creation) (#380)
- [Security] Fixed GHSA-4j8q-67fq-3xc3 (TOCTOU race in pad directory creation) (#381)
- [Security] Harden pusb_conf_xpath_id_is_safe: extend blocklist to reject double-quote and DEL (#383)
- [Security] Fixed GHSA-qr83-mf3h-fvqr (getenv() used in PAM context, env variable injection) (#384)
- [Security] Fixed GHSA-96vv-r4wc-28c2 (XXE in xmlReadFile via default parser flags) (#385)
- [Security] Add compiler and linker hardening flags (#386)
- [Security] Fixed GHSA-7j6h-wfc2-mg5q (NULL dereference crash in pusb_is_loginctl_local) (#393)
- [Security] Fixed GHSA-h28h-9hc3-v595 (infinite loop DoS in process-tree walk) (#394)
- [Security] Fixed GHSA-rfcg-6wgv-77hw (integer overflow UB in xpath time/int parsing) (#395)
- [Reliability] Fixed GHSA-vw5x-vxgv-fgxm (BUFSIZ truncation in pusb_get_process_envvar) (#396)
- [Security] Fixed GHSA-xgfj-3wm2-gvx8 (fopen without O_CLOEXEC in process.c) (#397)
- [Security] Fixed GHSA-chgp-j28w-mx9q (per-device config options silently never applied) (#398)

* Wed May 20 2026 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.9.1
- [Bugfix] Restore debug output suppressed before config parsing; fix XRDP detection log message (#355)
- [Security] Fixed GHSA-qg76-57wq-mpv6 (thread-unsafe static pointer in log.c) (#355)
- [Security] Fixed GHSA-24mw-m2vf-36vp (integer overflow before heap allocation in conf.c) (#359)
- [Security] Fixed GHSA-pvrg-chgw-x42c (evdev scan silently discards EACCES under non-root) (#360)
- [Security] Fixed GHSA-w38v-cw9r-x9p6 (PAM_RHOST check skipped when deny_remote=false) (#361)
- [CI/Tests] Add regression tests for thread-safe log.c (#357)
- [Refactor] Deduplicate pam_sm_authenticate / pam_sm_acct_mgmt in pam.c (#356)

* Tue May 19 2026 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.9.0
- [Feature] Remove hardcoded 10-device limit per user (#236)
- [Feature] Add additional remote connection check for VNC/RDP (optional, default on) (#202)
- [Enhancement] Add --superuser flag to pamusb-conf --add-user (#344)
- [Enhancement] Rename pamusb-pinentry to pinentry-pamusb, add syslog logging (#343)
- [Enhancement] Improve pinentry robustness (thx @DhruvaSambrani) (#341)
- [Enhancement] Add --install/--uninstall options to pinentry-pamusb (#290)
- [Enhancement] Add --install/--uninstall options to pamusb-keyring-unlock-gnome (#121)
- [Bugfix] Fix --reset-pads only applying to the primary/first device (#305)
- [Bugfix] Use absolute path for pamusb-check invocation
- [Bugfix] Use absolute shell path for agent commands (#325)
- [Security] Harden OTP pad mechanism: uninitialized magic, partial-read denial, timing-safe compare, sensitive buffer zeroing, O_CLOEXEC (#303)
- [Security] Harden tmux local login checks (#318)
- [Security] Harden process stat parent parsing (#319)
- [Security] Reject unsafe config XPath IDs
- [Security] Harden tmux command lookup
- [Security] Harden utmp display session matching (#306)
- [Security] Harden XPath string copying
- [Security] Harden reset-pads path handling
- [Security] Harden keyring auth check path (#323)
- [Security] Fixed GHSA-vx6f-rrqr-j87c (OTP pad authentication bypass) (#303)
- [Security] Fixed GHSA-vfj3-5h5v-6g93 (XPath injection via PAM-supplied identifiers) (#311)
- [Security] Fixed GHSA-pp29-w28g-r9h9 (uncontrolled search path in PAM tools)
- [Security] Fixed GHSA-7cgr-4c38-59h2 (local check bypass via process/session parsing)
- [Security] Fixed GHSA-jmmj-qhrq-w45g (IPv6-mapped address bypass in deny_remote) (#336)
- [Security] Fixed GHSA-j3xw-vc43-x7jg (strtok thread-safety race in deny_remote) (#336)
- [Security] Fixed GHSA-7rvx-jcc6-7hqq (OOM guards removable via -DNDEBUG) (#336)
- [CI/Tests] Add unit test suite (#296)
- [CI/Tests] Add integration test for >10 devices per user (#236)
- [CI/Tests] Add integration test for superuser device filtering (#223)
- [CI/Tests] Add integration test for pinentry --install/--uninstall (#304)
- [Docs] Update manpages to reflect current feature set (#345)
- [Docs] Expand CONTRIBUTING.md with bounty conditions and AI contribution rules
- [Misc] Update AUTHORS

* Tue May 05 2026 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.7
- [Enhancement] Specify a dedicated device for superuser services
- [Enhancement] Restore PolicyKit support (also for 127)
- [Enhancement] Remove default installation of pamusb-pinentry
- [Security] Fixed GHSA-822m-whrh-vrj8
- [Security] Fixed GHSA-jgv5-w6rm-7wxg
- [Security] Fixed GHSA-fjpm-p9pj-mp34
- [Security] Fixed GHSA-j8cq-2gv6-gfwf
- [Security] Fixed GHSA-jxrj-q67x-wr4c

* Fri May 03 2026 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.6
- [Enhancement] Documentation updates
- [Enhancement] Remote VSCode tunnels are now detected in deny_remote check (thx @jaoppb)
- [Bugfix] Fixed multiple memory issues
- [Bugfix] Possible pad corruption fixed (#278)
- [Tools] Add optional pinentry application (see documentation)
- [Misc] We are the official upstream (since 2024, shortly after 0.8.5)
- IMPORTANT: Exclude pam_usb from polkit authentication flow (see QUICKSTART/TROUBLESHOOTING)

* Fri Jul 26 2024 Tobias Bäumer <tobiasbaeumer@gmail.com> - 0.8.5
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
