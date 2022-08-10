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

int pusb_local_login(t_pusb_options *opts, const char *user, const char *service);

int pusb_is_tty_local(char *tty);

char *pusb_get_tty_from_display_server(const char *display);

char *pusb_get_tty_by_xorg_display(const char *display, const char *user);

char *pusb_get_tty_by_loginctl();

#endif /* !PUSB_LOCAL_H_ */
