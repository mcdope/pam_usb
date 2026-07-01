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

#ifndef PUSB_PAD_H_
#define PUSB_PAD_H_
#include <stdio.h>
#include <stdint.h>
#include <udisks/udisks.h>
#include "conf.h"

#define PUSB_PAD_PATH_MAX (1024 * 5)
#define PUSB_PAD_SIZE     1024

int pusb_pad_check(t_pusb_options *opts, UDisksClient *udisks, const char *user);

#ifdef UNIT_TESTING
int   pusb_pad_build_device_path(t_pusb_options *opts, const char *mnt_point, const char *user, char *path_out, size_t path_size);
int   pusb_pad_build_system_path(t_pusb_options *opts, const char *user, char *path_out, size_t path_size);
FILE *pusb_pad_open_device(t_pusb_options *opts, const char *mnt_point, const char *user, const char *mode);
int   pusb_pad_should_update(t_pusb_options *opts, const char *user);
int   pusb_pad_compare(t_pusb_options *opts, const char *volume, const char *user);
int   pusb_pad_update(t_pusb_options *opts, const char *volume, const char *user);
int   open_pad_file_in_dir(const char *fullpath, int flags);
int   generate_random_bytes(uint8_t *buf, size_t len);
int   timingsafe_memcmp(const void *a, const void *b, size_t n);
#endif

#endif /* !PUSB_PAD_H_ */
