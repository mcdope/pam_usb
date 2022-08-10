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

#ifndef PUSB_LOG_H_
#define PUSB_LOG_H_
#define log_debug(s, ...) __log_debug(__FILE__, __LINE__, s, ##__VA_ARGS__)
#include "conf.h"

void __log_debug(const char *file, int line, const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_info(const char *fmt, ...);
void pusb_log_init(t_pusb_options *opts);

#endif /* !PUSB_LOG_H_ */
