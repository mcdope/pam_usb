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

#ifndef PUSB_CONF_H_
#define PUSB_CONF_H_
#define PUSB_CONF_FILE "/etc/security/pam_usb.conf"
#define CONF_DEVICE_XPATH "//configuration/devices/device[@id='%s']/%s"
#define CONF_USER_XPATH "//configuration/users/user[@id='%s']/%s"
#define CONF_SERVICE_XPATH "//configuration/services/service[@id='%s']/%s"
#define CONF_USER_MAXLEN 32

#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <sys/utsname.h>
#include <sys/time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct	pusb_device
{
	char		name[128];
	char		vendor[128];
	char		model[128];
	char		serial[128];
	char		volume_uuid[128];
}				t_pusb_device;

typedef struct		pusb_options
{
	time_t			probe_timeout;
	int				enable;
	int				debug;
	int				quiet;
	int				color_log;
	int				one_time_pad;
	time_t			pad_expiration;
	int				deny_remote;
	char			hostname[_UTSNAME_NODENAME_LENGTH];
	char			system_pad_directory[PATH_MAX];
	char			device_pad_directory[PATH_MAX];
	t_pusb_device	device;
}					t_pusb_options;

struct		s_opt_list
{
	char	*name;
	char	*value;
};

int pusb_conf_init(t_pusb_options *opts);
int pusb_conf_parse(const char *file, t_pusb_options *opts, const char *user, const char *service);

#endif /* !PUSB_CONF_H_ */
