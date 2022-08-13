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

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include "conf.h"
#include "log.h"

static t_pusb_options *pusb_opts = NULL;

static void pusb_log_syslog(int level, const char *format, va_list ap)
{
	openlog("pam_usb", LOG_PID, LOG_AUTH);
	vsyslog(level, format, ap);
	closelog();
}

static void pusb_log_output(int level, const char *format, va_list ap)
{
	if (!isatty(fileno(stdin))) 
	{
		return;
	}
	
	if (pusb_opts && !pusb_opts->quiet)
	{
		if (pusb_opts && pusb_opts->color_log)
		{
			if (level == LOG_ERR)
			{
				fprintf(stderr, "\033[01;31m*\033[00m ");
			}
			else if (level == LOG_NOTICE)
			{
				fprintf(stderr, "\033[01;32m*\033[00m ");
			}
		}
		else
		{
			fprintf(stderr, "* ");
		}

		vfprintf(stderr, format, ap);
	}
}

void __log_debug(const char *file, int line, const char *fmt, ...)
{
	va_list	ap;

	if (!pusb_opts || !pusb_opts->debug)
	{
		return;
	}

	fprintf(stderr, "[%s:%03d] ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	pusb_log_syslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

void log_error(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	pusb_log_syslog(LOG_ERR, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	pusb_log_output(LOG_ERR, fmt, ap);
	va_end(ap);
}

void log_info(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	pusb_log_syslog(LOG_NOTICE, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	pusb_log_output(LOG_NOTICE, fmt, ap);
	va_end(ap);
}

void pusb_log_init(t_pusb_options *opts)
{
	pusb_opts = opts;
}
