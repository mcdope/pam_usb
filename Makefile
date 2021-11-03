# Set to 'yes' to include debugging information, e.g. DEBUG=yes make -e
DEBUG		:= no

PREFIX		?= /usr
ARCH := $(shell uname -m)
UID := $(shell id -u)
GID := $(shell id -g)

ifeq ($(ARCH), x86_64)
	LIBDIR ?= lib/x86_64-linux-gnu
endif
ifeq ($(ARCH), i686)
	LIBDIR ?= lib
endif

# compiler/linker options
CC		:= gcc
CFLAGS		:= $(CFLAGS) -Wall -fPIC `pkg-config --cflags libxml-2.0` \
	`pkg-config --cflags udisks2`
LIBS		:= `pkg-config --libs libxml-2.0` \
	`pkg-config --libs udisks2`

# common source files
SRCS		:= src/conf.c \
		   src/mem.c \
		   src/log.c \
		   src/xpath.c \
		   src/pad.c \
		   src/volume.c \
		   src/process.c \
		   src/tmux.c \
		   src/local.c \
		   src/device.c
OBJS		:= $(SRCS:.c=.o)

# pam_usb
PAM_USB_SRCS	:= src/pam.c
PAM_USB_OBJS	:= $(PAM_USB_SRCS:.c=.o)
PAM_USB			:= pam_usb.so
PAM_USB_LDFLAGS	:= -shared
PAM_USB_DEST	:= $(DESTDIR)/$(LIBDIR)/security

# pamusb-check
PAMUSB_CHECK_SRCS	:= src/pamusb-check.c
PAMUSB_CHECK_OBJS	:= $(PAMUSB_CHECK_SRCS:.c=.o)
PAMUSB_CHECK		:= pamusb-check

# Tools
PAMUSB_CONF		:= pamusb-conf
PAMUSB_AGENT		:= pamusb-agent
TOOLS_DEST		:= $(DESTDIR)$(PREFIX)/bin
TOOLS_SRC		:= tools

# Conf
CONFS			:= doc/pam_usb.conf
CONFS_DEST		:= $(DESTDIR)/etc/security

# Doc
DOCS		:= doc/CONFIGURATION doc/QUICKSTART
DOCS_DEST	:= $(DESTDIR)$(PREFIX)/share/doc/pam_usb

# Man
MANS		:= doc/pamusb-conf.1.gz doc/pamusb-agent.1.gz doc/pamusb-check.1.gz
MANS_DEST	:= $(DESTDIR)$(PREFIX)/share/man/man1

# PAM config
PAM_CONF		:= debian/pam-auth-update/usb
PAM_CONF_DEST 	:= $(DESTDIR)$(PREFIX)/share/pam-configs
PAM_AUTH_UPDATE := pam-auth-update

# Binaries
RM		:= rm
INSTALL		:= install
MKDIR		:= mkdir
DEBUILD := debuild -b -uc -us --lintian-opts --profile debian
MANCOMPILE := gzip -kf
DOCKER := docker

ifeq (yes, ${DEBUG})
	CFLAGS := ${CFLAGS} -ggdb
endif

all		: manpages $(PAM_USB) $(PAMUSB_CHECK)

$(PAM_USB)	: $(OBJS) $(PAM_USB_OBJS)
		$(CC) -o $(PAM_USB) $(PAM_USB_LDFLAGS) $(LDFLAGS) $(OBJS) $(PAM_USB_OBJS) $(LIBS)

$(PAMUSB_CHECK)	: $(OBJS) $(PAMUSB_CHECK_OBJS)
		$(CC) -o $(PAMUSB_CHECK) $(LDFLAGS) $(OBJS) $(PAMUSB_CHECK_OBJS) $(LIBS)

%.o		: %.c
		${CC} -c ${CFLAGS} $< -o $@

clean		:
		$(RM) -f $(MANS) $(PAM_USB) $(PAMUSB_CHECK) $(OBJS) $(PAMUSB_CHECK_OBJS) $(PAM_USB_OBJS)

manpages:
		$(MANCOMPILE) ./doc/*.1

install		: all
		$(MKDIR) -p $(CONFS_DEST) $(DOCS_DEST) $(MANS_DEST) $(TOOLS_DEST) $(PAM_USB_DEST) $(PAM_CONF_DEST)
		$(INSTALL) -m755 $(PAM_USB) $(PAM_USB_DEST)
		$(INSTALL) -m755 $(PAMUSB_CHECK) $(TOOLS_SRC)/$(PAMUSB_CONF) $(TOOLS_SRC)/$(PAMUSB_AGENT) $(TOOLS_DEST)
		$(INSTALL) -b -m644 $(CONFS) $(CONFS_DEST)
		$(INSTALL) -m644 $(DOCS) $(DOCS_DEST)
		$(INSTALL) -m644 $(MANS) $(MANS_DEST)
		$(INSTALL) -m644 $(PAM_CONF) $(PAM_CONF_DEST)/libpam-usb

deinstall	:
		$(RM) -f $(PAM_USB_DEST)/$(PAM_USB)
		$(RM) -f $(TOOLS_DEST)/$(PAMUSB_CHECK) $(TOOLS_DEST)/$(PAMUSB_CONF) $(TOOLS_DEST)/$(PAMUSB_AGENT) $(PAM_CONF_DEST)/$(PAM_CONF)
		$(RM) -rf $(DOCS_DEST)
		$(RM) -f $(MANS_DEST)/pamusb-*\.1\.gz
		$(RM) -f $(PAM_CONF_DEST)/$(PAM_CONF)
		
changelog : 
		git log --pretty=format:"%h %ad%x09%an%x09%s" --date=short 40b17fa..HEAD > changelog-from-v0.5.0

debchangelog : 
		git log --pretty=format:"  * %s (%an <%ae>)" --date=short 40b17fa..HEAD > changelog-for-deb

deb : clean all
	$(DEBUILD)

deb-sign : build
	debsign -S -k$(APT_SIGNING_KEY) `ls -t .build/*.changes | head -1`

buildenv :
	$(DOCKER) build -t mcdope/pam_usb-ubuntu-build .

build : buildenv
	mkdir -p .build
	$(DOCKER) run -it \
		-v`pwd`/.build:/usr/local/src \
		-v`pwd`:/usr/local/src/pam_usb \
		--rm mcdope/pam_usb-ubuntu-build \
		sh -c "make deb && chown $(UID):$(GID) ../libpam-usb*"
