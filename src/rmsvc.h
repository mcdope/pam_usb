/*
 * Copyright (c) 2026 Tobias Baeumer <tobiasbaeumer@gmail.com>
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

#ifndef PUSB_RMSVC_H_
#define PUSB_RMSVC_H_

#include <stdint.h>
#include "conf.h"

int pusb_has_active_remote_service(t_pusb_options *opts);

#ifdef UNIT_TESTING
int pusb_proc_tcp_is_loopback_v4(uint32_t raw_addr);
int pusb_proc_tcp6_is_loopback(const char *hex32);
int pusb_proc_tcp4_has_established(const char *path, uint16_t port);
int pusb_proc_tcp4_has_established_to(const char *path, uint16_t port);
int pusb_proc_tcp6_has_established(const char *path, uint16_t port);
int pusb_proc_tcp6_has_established_to(const char *path, uint16_t port);
int pusb_process_name_exists(const char *procfs_root, const char *name);
#endif

#endif /* !PUSB_RMSVC_H_ */
