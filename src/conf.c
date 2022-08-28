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

#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include "mem.h"
#include "conf.h"
#include "xpath.h"
#include "log.h"

static void pusb_conf_options_get_from(
	t_pusb_options *opts,
	const char *from,
	xmlDoc *doc
)
{
	pusb_xpath_get_string_from(doc, from, "option[@name='hostname']", opts->hostname, sizeof(opts->hostname));
	pusb_xpath_get_string_from(doc, from, "option[@name='system_pad_directory']", opts->system_pad_directory, sizeof(opts->system_pad_directory));
	pusb_xpath_get_string_from(doc, from, "option[@name='device_pad_directory']", opts->device_pad_directory, sizeof(opts->device_pad_directory));
	pusb_xpath_get_bool_from(doc, from, "option[@name='debug']", &(opts->debug));
	pusb_xpath_get_bool_from(doc, from, "option[@name='quiet']", &(opts->quiet));
	pusb_xpath_get_bool_from(doc, from, "option[@name='color_log']", &(opts->color_log));
	pusb_xpath_get_bool_from(doc, from, "option[@name='enable']", &(opts->enable));
	pusb_xpath_get_bool_from(doc, from, "option[@name='one_time_pad']", &(opts->one_time_pad));
	pusb_xpath_get_time_from(doc, from, "option[@name='pad_expiration']", &(opts->pad_expiration));
	pusb_xpath_get_time_from(doc, from, "option[@name='probe_timeout']", &(opts->probe_timeout));
	pusb_xpath_get_bool_from(doc, from, "option[@name='deny_remote']", &(opts->deny_remote));
}

static int pusb_conf_parse_options(
	t_pusb_options *opts,
	xmlDoc *doc,
	const char *user,
	const char *service
)
{
	char *xpath = NULL;
	size_t xpath_size;
	int i;
	
	// these can come from argv, so make sure nothing messes up snprintf later
	char xpath_user[32] = { };
	char xpath_service[32] = { };
	snprintf(xpath_user, 32, "%s", user);
	snprintf(xpath_service, 32, "%s", service);

	struct s_opt_list	opt_list[] = {
		{ CONF_DEVICE_XPATH, opts->device.name },
		{ CONF_USER_XPATH, xpath_user },
		{ CONF_SERVICE_XPATH, xpath_service },
		{ NULL, NULL }
	};

	pusb_conf_options_get_from(opts, "//configuration/defaults/", doc);
	for (i = 0; opt_list[i].name != NULL; ++i)
	{
		xpath_size = strlen(opt_list[i].name) + strlen(opt_list[i].value) + 1;
		xpath = xmalloc(xpath_size);
		memset(xpath, 0x00, xpath_size);
		snprintf(xpath, xpath_size, opt_list[i].name, opt_list[i].value, "");
		pusb_conf_options_get_from(opts, xpath, doc);
		xfree(xpath);
	}
	return (1);
}

static int pusb_conf_device_get_property(
	t_pusb_options *opts,
	xmlDoc *doc,
	const char *property,
	char *store,
	size_t size,
	char *deviceId
)
{
	char *xpath = NULL;
	size_t xpath_len;
	int retval;

	xpath_len = strlen(CONF_DEVICE_XPATH) + strlen(deviceId) + strlen(property) + 1;
	xpath = xmalloc(xpath_len);
	memset(xpath, 0x00, xpath_len);
	snprintf(xpath, xpath_len, CONF_DEVICE_XPATH, deviceId, property);
	retval = pusb_xpath_get_string(doc, xpath, store, size);
	xfree(xpath);
	return (retval);
}

static int pusb_conf_parse_device(
	t_pusb_options *opts,
	xmlDoc *doc,
	int deviceIndex,
	char *deviceId
)
{
	pusb_conf_device_get_property(opts, doc, "vendor", opts->device_list[deviceIndex].vendor, sizeof(opts->device_list[deviceIndex].vendor), deviceId);
	pusb_conf_device_get_property(opts, doc, "model", opts->device_list[deviceIndex].model, sizeof(opts->device_list[deviceIndex].model), deviceId);

	if (!pusb_conf_device_get_property(opts, doc, "serial", opts->device_list[deviceIndex].serial, sizeof(opts->device_list[deviceIndex].serial), deviceId))
	{
		return (0);
	}

	pusb_conf_device_get_property(opts, doc, "volume_uuid", opts->device_list[deviceIndex].volume_uuid, sizeof(opts->device_list[deviceIndex].volume_uuid), deviceId);
	return (1);
}

int pusb_conf_init(t_pusb_options *opts)
{
	struct utsname	u;

	memset(opts, 0x00, sizeof(*opts));
	if (uname(&u) == -1)
	{
		log_error("uname: %s\n", strerror(errno));
		return (0);
	}
	snprintf(opts->hostname, sizeof(opts->hostname), "%s", u.nodename);
	if (strnlen(u.nodename, sizeof(u.nodename)) > sizeof(opts->hostname)) 
	{
		log_info("Hostname \"%s\" is too long, truncating to \"%s\".\n", u.nodename, opts->hostname);
	}

	snprintf(opts->system_pad_directory, sizeof(opts->system_pad_directory), "%s", ".pamusb");
	snprintf(opts->device_pad_directory, sizeof(opts->device_pad_directory), "%s", ".pamusb");
	opts->probe_timeout = 10;
	opts->enable = 1;
	opts->debug = 0;
	opts->quiet = 0;
	opts->color_log = 1;
	opts->one_time_pad = 1;
	opts->pad_expiration = 3600;
	opts->deny_remote = 1;
	return (1);
}

int pusb_conf_parse(
	const char *file,
	t_pusb_options *opts,
	const char *user,
	const char *service
)
{
	xmlDoc *doc = NULL;
	int retval;
	char device_xpath[sizeof(CONF_USER_XPATH) + CONF_USER_MAXLEN + sizeof("device")];

	log_debug("Parsing settings...\n", user, service);
	if (strnlen(user, sizeof(user)) > CONF_USER_MAXLEN)
	{
		log_error("Username \"%s\" is too long (max: %d).\n", user, CONF_USER_MAXLEN);
		return (0);
	}
	if (!(doc = xmlReadFile(file, NULL, 0)))
	{
		log_error("Unable to parse \"%s\".\n", file);
		return (0);
	}
	snprintf(device_xpath, sizeof(device_xpath), CONF_USER_XPATH, user, "device");

	char *device_list[10] = { 
		xmalloc(128), xmalloc(128), xmalloc(128), xmalloc(128), xmalloc(128), 
		xmalloc(128), xmalloc(128), xmalloc(128), xmalloc(128), xmalloc(128) 
	};
	for (int currentDevice = 0; currentDevice < 10; currentDevice++)
	{
		memset(device_list[currentDevice], 0x0, 128);
	}
	retval = pusb_xpath_get_string_list(
		doc,
		device_xpath,
		device_list,
		sizeof(opts->device.name)
	);

	if (!retval)
	{
		log_error("No authentication device(s) configured for user \"%s\".\n", user);
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return (0);
	}

	for (int currentDevice = 0; currentDevice < 10; currentDevice++)
	{
		log_error("DBG: currentDevice: %d, strnlen: %d, value: %s\n", currentDevice, strnlen(device_list[currentDevice], 128), device_list[currentDevice]);
		if (device_list[currentDevice] == NULL || strnlen(device_list[currentDevice], 128) == 0)
		{
			continue;
		}

		strncpy(opts->device_list[currentDevice].name, device_list[currentDevice], strnlen(device_list[currentDevice], 128));
		pusb_conf_parse_device(opts, doc, currentDevice, device_list[currentDevice]);
		log_error("DBG: found device\n");
		log_error("DBG:     name: %s\n", opts->device_list[currentDevice].name);
		log_error("DBG:     vendor: %s\n", opts->device_list[currentDevice].vendor);
	}

	if (!pusb_conf_parse_options(opts, doc, user, service))
	{
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return (0);
	}
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return (1);
}
