# Set to 'yes' to include debugging information, e.g. DEBUG=yes make -e
DEBUG := no

PREFIX ?= /usr
CROSS_ARCH ?=
ifeq ($(CROSS_ARCH),)
    ifeq ($(DEB_HOST_ARCH), m68k)
        CROSS_ARCH := m68k-linux-gnu
    endif
endif
ARCH := $(if $(CROSS_ARCH),$(CROSS_ARCH),$(shell uname -m))
UID := $(shell id -u)
GID := $(shell id -g)
USE_FEDORA_LIBDIR := $(shell test -d /lib64/security && echo 1 || echo 0)
VERSION := $(shell git for-each-ref --sort=creatordate --format '%(tag)' refs/tags | tail -n 1).$(shell git rev-list --count $(shell git for-each-ref --sort=creatordate --format '%(tag)' refs/tags | tail -n 1)..HEAD)

ifeq ($(ARCH), x86_64)
	ifeq ($(USE_FEDORA_LIBDIR), 1)
		LIBDIR ?= lib64
	else
		LIBDIR ?= lib/x86_64-linux-gnu
	endif
endif
ifeq ($(ARCH), i686)
	LIBDIR ?= lib
endif
ifeq ($(ARCH), aarch64) # ARM64, i.e Apple silicon and other up2date CPUs/SoCs
	ifeq ($(USE_FEDORA_LIBDIR), 1)
		LIBDIR ?= lib64
	else
		LIBDIR ?= lib/aarch64-linux-gnu
	endif
endif
ifeq ($(ARCH), armv7l) # ARM32, i.e Raspberries
	LIBDIR ?= lib/arm-linux-gnueabihf
endif
ifeq ($(ARCH), m68k-linux-gnu) # Motorola 68k - Amiga forever
	LIBDIR ?= lib/m68k-linux-gnu
endif

# compiler/linker options
CC ?= gcc
PKG_CONFIG ?= pkg-config
CFLAGS := $(CFLAGS) -Wall -O2 -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fstack-clash-protection -fno-plt -Wformat=2 `$(PKG_CONFIG) --cflags libxml-2.0` `$(PKG_CONFIG) --cflags udisks2` `$(PKG_CONFIG) --cflags libevdev` #cflags libxml?
# -fzero-call-used-regs requires GCC >= 11; probe at configure time
ZERO_REGS_SUPPORTED := $(shell $(CC) -fzero-call-used-regs=used-gpr -E - < /dev/null 2>/dev/null && echo yes)
ifeq ($(ZERO_REGS_SUPPORTED), yes)
	CFLAGS := $(CFLAGS) -fzero-call-used-regs=used-gpr
endif
ifeq ($(ARCH), x86_64)
	CFLAGS := $(CFLAGS) -fcf-protection=full
endif
ifeq (yes, ${DEBUG})
	CFLAGS := ${CFLAGS} -O0 -U_FORTIFY_SOURCE -ggdb
endif

HARDENING_LDFLAGS := -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

LIBS := `$(PKG_CONFIG) --libs libxml-2.0` `$(PKG_CONFIG) --libs udisks2` `$(PKG_CONFIG) --libs libevdev`
ifeq ($(ARCH), m68k-linux-gnu)
    LIBS += -lm
    LDFLAGS += -Wl,--allow-shlib-undefined
endif

# common source files
SRCS := src/conf.c \
	src/mem.c \
	src/log.c \
	src/xpath.c \
	src/pad.c \
	src/volume.c \
	src/process.c \
	src/tmux.c \
	src/local.c \
	src/device.c \
	src/rmsvc.c \
	src/evdev.c
OBJS := $(SRCS:.c=.o)

# pam_usb
PAM_USB_SRCS := src/pam.c
PAM_USB_OBJS := $(PAM_USB_SRCS:.c=.o)
PAM_USB	:= pam_usb.so
PAM_USB_LDFLAGS := -shared $(HARDENING_LDFLAGS)
PAM_USB_DEST := $(DESTDIR)/$(LIBDIR)/security

# pamusb-check
PAMUSB_CHECK_SRCS := src/pamusb-check.c
PAMUSB_CHECK_OBJS := $(PAMUSB_CHECK_SRCS:.c=.o)
PAMUSB_CHECK := pamusb-check

# Tools
PAMUSB_CONF := pamusb-conf
PAMUSB_AGENT := pamusb-agent
PAMUSB_KEYRING_GNOME := pamusb-keyring-unlock-gnome
PAMUSB_PINENTRY := pinentry-pamusb
TOOLS_DEST := $(DESTDIR)$(PREFIX)/bin
TOOLS_SRC := tools

# Conf
CONFS := doc/pam_usb.conf
CONFS_DEST := $(DESTDIR)/etc/security

# Doc
DOCS := doc/CONFIGURATION doc/QUICKSTART doc/SECURITY doc/TROUBLESHOOTING
DOCS_DEST := $(DESTDIR)$(PREFIX)/share/doc/pam_usb

# Man
MANS := doc/pamusb-conf.1.gz doc/pamusb-agent.1.gz doc/pamusb-check.1.gz doc/pamusb-keyring-unlock-gnome.1.gz doc/pinentry-pamusb.1.gz
MANS_DEST := $(DESTDIR)$(PREFIX)/share/man/man1

# PAM config
PAM_CONF := debian/pam-auth-update/usb
PAM_CONF_DEST := $(DESTDIR)$(PREFIX)/share/pam-configs

# polkit config
POLKIT_CONF := doc/systemd-polkit-agent-helper-pamusb.conf
POLKIT_CONF_DEST := $(DESTDIR)$(PREFIX)/lib/systemd/system/polkit-agent-helper@.service.d/

# Binaries
RM  := rm
INSTALL	:= install
MKDIR := mkdir
DEBARCH ?=
DEBUILD	:= debuild $(if $(DEBARCH),-a $(DEBARCH),) -b -uc -us --lintian-opts --profile debian
RPMBUILD := rpmbuild -v -bb --clean fedora/SPECS/pam_usb.spec
ZSTBUILD := cd arch_linux && makepkg -p PKGBUILD_git && cd ..
MANCOMPILE := gzip -kf
DOCKER := docker

.DEFAULT_GOAL := all

# ── Unit test targets ──────────────────────────────────────────────────────────
TEST_CFLAGS              := $(CFLAGS)
XPATH_LDFLAGS            := `$(PKG_CONFIG) --libs libxml-2.0` -lcmocka
CONF_LDFLAGS             := `$(PKG_CONFIG) --libs libxml-2.0` -lcmocka
TMUX_LDFLAGS             := -lcmocka
PAD_LDFLAGS              := `$(PKG_CONFIG) --libs libxml-2.0` -lcmocka
PROC_LDFLAGS             := -lcmocka
RMSVC_LDFLAGS            := -lcmocka
EVDEV_LDFLAGS            := -lcmocka
LOCAL_LDFLAGS            := `$(PKG_CONFIG) --libs libevdev` -lcmocka
PAM_TEST_LDFLAGS         := -lcmocka
LOG_TEST_LDFLAGS         := -lcmocka
MEM_LDFLAGS              := -lcmocka

test-c-xpath: src/xpath.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/xpath_test.c $^ $(XPATH_LDFLAGS) -o tests/unit/c/xpath_test
	./tests/unit/c/xpath_test

test-c-conf: src/conf.o src/xpath.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/conf_test.c $^ $(CONF_LDFLAGS) -o tests/unit/c/conf_test
	./tests/unit/c/conf_test

test-c-tmux: src/process.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/tmux_test.c $^ -Wl,--wrap=popen,--wrap=pclose $(TMUX_LDFLAGS) -o tests/unit/c/tmux_test
	./tests/unit/c/tmux_test

test-c-pad: src/conf.o src/xpath.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/pad_test.c $^ $(PAD_LDFLAGS) -o tests/unit/c/pad_test
	./tests/unit/c/pad_test

test-c-process: src/process.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/process_test.c $^ $(PROC_LDFLAGS) -o tests/unit/c/process_test
	./tests/unit/c/process_test

test-c-rmsvc: src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/rmsvc_test.c $^ \
		$(RMSVC_LDFLAGS) -o tests/unit/c/rmsvc_test
	./tests/unit/c/rmsvc_test

test-c-local: src/process.o src/tmux.o src/rmsvc.o src/evdev.o src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/local_test.c $^ \
		$(LOCAL_LDFLAGS) -o tests/unit/c/local_test
	./tests/unit/c/local_test

test-c-evdev: src/mem.o src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/evdev_test.c \
		tests/unit/c/fake_libevdev.c $^ \
		-Wl,--wrap=opendir,--wrap=readdir,--wrap=closedir,--wrap=open,--wrap=close \
		$(EVDEV_LDFLAGS) -o tests/unit/c/evdev_test
	./tests/unit/c/evdev_test

test-c-pam: src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/pam_test.c src/pam.c $^ \
		-Wl,--wrap=pam_get_item,--wrap=pam_get_user,--wrap=pusb_conf_init,--wrap=pusb_conf_parse,--wrap=pusb_conf_free,--wrap=pusb_local_login,--wrap=pusb_device_check \
		$(PAM_TEST_LDFLAGS) -o tests/unit/c/pam_test
	./tests/unit/c/pam_test

test-c-log: tests/unit/c/log_test.c src/log.c
	$(CC) $(TEST_CFLAGS) tests/unit/c/log_test.c \
		$(LOG_TEST_LDFLAGS) -o tests/unit/c/log_test
	./tests/unit/c/log_test

test-c-mem: tests/unit/c/mem_test.c src/mem.c src/log.o
	$(CC) $(TEST_CFLAGS) tests/unit/c/mem_test.c src/log.o \
		-Wl,--wrap=explicit_bzero,--wrap=__explicit_bzero_chk \
		$(MEM_LDFLAGS) -o tests/unit/c/mem_test
	./tests/unit/c/mem_test

test-c: test-c-xpath test-c-conf test-c-tmux test-c-pad test-c-process test-c-rmsvc test-c-local test-c-evdev test-c-pam test-c-log test-c-mem

test-python:
	python3 -m pytest tests/unit/python/ -v

test-shell:
	bash tests/unit/shell/loginctl_session_id_test.sh
	bash tests/unit/shell/w_remote_detection_test.sh

test: test-c test-python test-shell

all: manpages $(PAM_USB) $(PAMUSB_CHECK)

$(PAM_USB): $(OBJS) $(PAM_USB_OBJS)
	$(CC) -o $(PAM_USB) $(PAM_USB_LDFLAGS) $(LDFLAGS) $(OBJS) $(PAM_USB_OBJS) $(LIBS)

$(PAMUSB_CHECK): $(OBJS) $(PAMUSB_CHECK_OBJS)
	$(CC) -o $(PAMUSB_CHECK) $(LDFLAGS) $(HARDENING_LDFLAGS) -pie $(OBJS) $(PAMUSB_CHECK_OBJS) $(LIBS)

%.o: %.c
	${CC} -c ${CFLAGS} $< -o $@

clean:
	$(RM) -f \
		$(MANS) \
		doc/pamusb-pinentry.1.gz \
		$(PAM_USB) \
		$(PAMUSB_CHECK) \
		$(OBJS) \
		$(PAMUSB_CHECK_OBJS) \
		$(PAM_USB_OBJS)

manpages:
	$(MANCOMPILE) ./doc/*.1

update-other-docs:
	wget https://raw.githubusercontent.com/wiki/mcdope/pam_usb/Configuration.md -O doc/CONFIGURATION > /dev/null 2>&1
	wget https://raw.githubusercontent.com/wiki/mcdope/pam_usb/Getting-Started.md -O doc/QUICKSTART > /dev/null 2>&1
	wget https://raw.githubusercontent.com/wiki/mcdope/pam_usb/Security.md -O doc/SECURITY > /dev/null 2>&1
	wget https://raw.githubusercontent.com/wiki/mcdope/pam_usb/Troubleshooting.md -O doc/TROUBLESHOOTING > /dev/null 2>&1
	git status --porcelain=v1 2>/dev/null && echo "Committing docs..." || { echo "Git staging area needs to be clean!"; exit 1; }
	git add \
		doc/CONFIGURATION \
		doc/QUICKSTART \
		doc/SECURITY \
		doc/TROUBLESHOOTING \
		 > /dev/null 2>&1
	git commit \
		--author="make update-other-docs <noemail@example.com>" \
		--signoff \
		-m "[Docs] Update non-manpage \"doc/\" files" \
		 > /dev/null 2>&1 || { git reset doc/CONFIGURATION doc/QUICKSTART doc/SECURITY doc/TROUBLESHOOTING; echo "No changes to commit."; }

install: all
	$(MKDIR) -p \
		$(CONFS_DEST) \
		$(DOCS_DEST) \
		$(MANS_DEST) \
		$(TOOLS_DEST) \
		$(PAM_USB_DEST)

	$(INSTALL) -m755 $(PAM_USB) $(PAM_USB_DEST)
	$(INSTALL) -m755 $(PAMUSB_CHECK) $(TOOLS_SRC)/$(PAMUSB_CONF) $(TOOLS_SRC)/$(PAMUSB_AGENT) $(TOOLS_SRC)/$(PAMUSB_KEYRING_GNOME) $(TOOLS_SRC)/$(PAMUSB_PINENTRY) $(TOOLS_DEST)
	$(INSTALL) -m644 $(DOCS) $(DOCS_DEST)
	$(INSTALL) -m644 $(MANS) $(MANS_DEST)

	if test -d $(PAM_CONF_DEST); then $(INSTALL) -m644 $(PAM_CONF) $(PAM_CONF_DEST)/libpam-usb; fi
	if test -f $(CONFS_DEST)/pam_usb.conf; then $(INSTALL) -b -m644 $(CONFS) $(CONFS_DEST)/pam_usb.conf.dist; fi
	if test ! -f $(CONFS_DEST)/pam_usb.conf; then $(INSTALL) -b -m644 $(CONFS) $(CONFS_DEST); fi

	install -d -m 0755 $(POLKIT_CONF_DEST)
	$(INSTALL) -m644 $(POLKIT_CONF) $(POLKIT_CONF_DEST)/systemd-polkit-agent-helper-pamusb.conf

# force pam-auth-update config install if building a deb
	if test $(DEB_TARGET_ARCH) != "" > /dev/null 2>&1; then mkdir -p $(PAM_CONF_DEST) && $(INSTALL) -m644 $(PAM_CONF) $(PAM_CONF_DEST)/libpam-usb; fi

deinstall:
	$(RM) -f $(PAM_USB_DEST)/$(PAM_USB)
	$(RM) -f \
		$(TOOLS_DEST)/$(PAMUSB_CHECK) \
		$(TOOLS_DEST)/$(PAMUSB_CONF) \
		$(TOOLS_DEST)/$(PAMUSB_AGENT) \
		$(TOOLS_DEST)/$(PAMUSB_KEYRING_GNOME) \
		$(TOOLS_DEST)/$(PAMUSB_PINENTRY) \
		$(PAM_CONF_DEST)/$(PAM_CONF) \
		$(POLKIT_CONF_DEST)/systemd-polkit-agent-helper-pamusb.conf

	$(RM) -rf $(DOCS_DEST)
	$(RM) -f $(addprefix $(MANS_DEST)/,$(notdir $(MANS)))
	$(RM) -f $(MANS_DEST)/pamusb-*\.1\.gz
	$(RM) -f $(PAM_CONF_DEST)/$(PAM_CONF)

uninstall: deinstall

changelog:
	git log --pretty=format:"%h %ad%x09%an%x09%s" --date=short 40b17fa..HEAD > changelog-from-v0.5.0

debchangelog:
	git log --pretty=format:"  * %s (%an <%ae>)" --date=short 40b17fa..HEAD > changelog-for-deb

builddir:
	mkdir -p .build > /dev/null 2>&1 || echo 0

deb: clean builddir
	$(DEBUILD)

deb-sign: build-debian
	debsign -S -k$(APT_SIGNING_KEY) `ls -t .build/*.changes | head -1`

rpm: clean builddir
	$(RPMBUILD)
	yes | cp -rf fedora/RPMS/$(ARCH)/*.rpm .build

rpm-sign: build-fedora
	rpm --addsign `ls -t .build/*.rpm | head -1`

zst-sign: build-arch
	@pkg=$$(ls -t .build/*.pkg.tar.zst 2>/dev/null | head -1); \
	if [ -n "$$pkg" ]; then gpg --detach-sign "$$pkg"; else echo "No package found to sign"; exit 1; fi

rpm-lint: build-fedora
	rpmlint `ls -t .build/*.rpm | head -1`

zst: clean builddir
	rm -f arch_linux/*.zst
	$(ZSTBUILD)
	yes | cp -rf arch_linux/*.zst .build
	rm -rf arch_linux/pam_usb arch_linux/src arch_linux/pkg arch_linux/*.tar.gz arch_linux/*.zst

sourcegz: clean builddir
	tar --exclude="./debian" \
		--exclude="./fedora" \
		--exclude="./arch_linux" \
		--exclude="./tests" \
		--exclude="./.build" \
		--exclude="./.idea" \
		--exclude="./.vscode" \
		--exclude="./.github" \
		--exclude="./.git" \
		--exclude="./.gitignore" \
		--exclude="./.git-blame-ignore-revs" \
		--exclude="./.claude" \
		--exclude="./CLAUDE.md" \
		--exclude="./AGENTS.md" \
		--exclude="./.cursorrules" \
		--exclude="./.pytest_cache" \
		-zcvf .build/pam_usb-$(VERSION).tar.gz .

buildenv-debian:
	$(DOCKER) build -f Dockerfile.debian -t mcdope/pam_usb-ubuntu-build .

buildenv-fedora:
	$(DOCKER) build -f Dockerfile.fedora -t mcdope/pam_usb-fedora-build .

buildenv-arch:
	$(DOCKER) build -f Dockerfile.arch -t mcdope/pam_usb-arch-build .

build-debian: buildenv-debian
	$(DOCKER) run -i \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-ubuntu-build \
		sh -c "make deb && chown -R $(UID):$(GID) .build debian"

build-fedora: buildenv-fedora
	$(DOCKER) run -i \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-fedora-build \
		sh -c "make rpm && chown -R $(UID):$(GID) .build fedora"

build-arch: buildenv-arch
	$(DOCKER) run -i \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-arch-build \
		sh -c "chown -R builduser:builduser . && sudo -u builduser make zst && chown -R $(UID):$(GID) ."

build-all: sourcegz buildenv-arch buildenv-debian buildenv-fedora build-arch build-debian build-fedora build-debian-arm64 build-debian-armhf build-debian-i386 build-debian-m68k build-fedora-arm64 build-arch-arm64

setup-qemu:
	$(DOCKER) run --rm --privileged multiarch/qemu-user-static -p yes || true

buildenv-debian-arm64: setup-qemu
	DOCKER_BUILDKIT=1 $(DOCKER) build --platform linux/arm64 -f Dockerfile.debian -t mcdope/pam_usb-ubuntu-arm64-build .

build-debian-arm64: buildenv-debian-arm64
	$(DOCKER) run -i --platform linux/arm64 \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-ubuntu-arm64-build \
		sh -c "git config --global --add safe.directory /usr/local/src/pam_usb && make deb && chown -R $(UID):$(GID) .build debian"

buildenv-debian-armhf: setup-qemu
	DOCKER_BUILDKIT=1 $(DOCKER) build --platform linux/arm/v7 -f Dockerfile.debian -t mcdope/pam_usb-ubuntu-armhf-build .

build-debian-armhf: buildenv-debian-armhf
	$(DOCKER) run -i --platform linux/arm/v7 \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-ubuntu-armhf-build \
		sh -c "git config --global --add safe.directory /usr/local/src/pam_usb && make deb && chown -R $(UID):$(GID) .build debian"

buildenv-debian-i386: setup-qemu
	DOCKER_BUILDKIT=1 $(DOCKER) build --platform linux/386 -f Dockerfile.debian-i386 -t mcdope/pam_usb-ubuntu-i386-build .

build-debian-i386: buildenv-debian-i386
	$(DOCKER) run -i --platform linux/386 \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-ubuntu-i386-build \
		sh -c "make deb && chown -R $(UID):$(GID) .build debian"

buildenv-debian-m68k:
	$(DOCKER) build -f Dockerfile.debian-m68k -t mcdope/pam_usb-debian-m68k-build .

build-debian-m68k: buildenv-debian-m68k
	$(DOCKER) run -i \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-debian-m68k-build \
		sh -c "git config --global --add safe.directory /usr/local/src/pam_usb && DEBARCH=m68k make deb && chown -R $(UID):$(GID) .build debian"

buildenv-fedora-arm64: setup-qemu
	DOCKER_BUILDKIT=1 $(DOCKER) build --platform linux/arm64 -f Dockerfile.fedora-arm64 -t mcdope/pam_usb-fedora-arm64-build .

build-fedora-arm64: buildenv-fedora-arm64
	$(DOCKER) run -i --platform linux/arm64 \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-fedora-arm64-build \
		sh -c "make rpm && chown -R $(UID):$(GID) .build fedora"

buildenv-arch-arm64: setup-qemu
	DOCKER_BUILDKIT=1 $(DOCKER) build --platform linux/arm64 -f Dockerfile.arch-arm64 -t mcdope/pam_usb-arch-arm64-build .

build-arch-arm64: buildenv-arch-arm64
	$(DOCKER) run -i --platform linux/arm64 \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-arch-arm64-build \
		sh -c "chown -R builduser:builduser . && sudo -u builduser make zst && chown -R $(UID):$(GID) ."
