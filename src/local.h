/*
 * Copyright (c) 2003-2007 Andrea Luzzardi <scox@sig11.org>
 *
 * This file is part of the pam_usb project. pam_usb is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * pam_usb is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PUSB_LOCAL_H_
#define PUSB_LOCAL_H_

#include <sys/types.h>
#include "conf.h"

#define CMDLINE_BUF_SIZE 4096
#define LOGINCTL_SHOW_SESSION_CMD \
	"export LC_ALL=C; /usr/bin/loginctl show-session auto -p %s 2>/dev/null | " \
	"/usr/bin/awk -F= '{print $2}'"

int pusb_local_login(t_pusb_options *opts, const char *user, const char *service);

int pusb_is_tty_local(char *tty);

char *pusb_get_tty_from_display_server(const char *display);

char *pusb_get_tty_by_xorg_display(const char *display, const char *user);

char *pusb_get_tty_by_loginctl();

#ifdef UNIT_TESTING
int         pusb_utmpx_field_equals(const char *field, size_t field_len, const char *value);
int         pusb_utmpx_field_starts_with(const char *field, size_t field_len, const char *prefix);
const char *pusb_loginctl_parse_output(char *buf);
ssize_t     pusb_read_cmdline(int fd, char *buf, size_t bufsize);
#endif

#endif /* !PUSB_LOCAL_H_ */
