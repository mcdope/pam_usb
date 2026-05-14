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

static int pusb_conf_xpath_id_is_safe(const char *name, const char *value)
{
	const unsigned char *cursor;

	if (value == NULL || value[0] == '\0')
	{
		log_error("%s is empty.\n", name);
		return 0;
	}

	for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
	{
		if (*cursor == '\'' || *cursor < 0x20)
		{
			log_error("%s contains an unsafe character for XPath lookup.\n", name);
			return 0;
		}
	}

	return 1;
}

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
	pusb_xpath_get_bool_from(doc, from, "option[@name='remote_desktop_check']", &(opts->remote_desktop_check));
	pusb_xpath_get_bool_from(doc, from, "option[@name='superuser']", &(opts->superuser));
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

	struct s_opt_list opt_list[] = {
		{ CONF_DEVICE_XPATH, opts->device.name },
		{ CONF_USER_XPATH, user },
		{ CONF_SERVICE_XPATH, service },
		{ NULL, NULL }
	};

	pusb_conf_options_get_from(opts, "//configuration/defaults/", doc);
	for (i = 0; opt_list[i].name != NULL; ++i)
	{
		xpath_size = strlen(opt_list[i].name) + strlen(opt_list[i].value) + 1;
		xpath = xmalloc(xpath_size);
		if (xpath == NULL) {
			log_error("Memory allocation failed\n");
			return 0;
		}
		memset(xpath, 0x00, xpath_size);
		snprintf(xpath, xpath_size, opt_list[i].name, opt_list[i].value, "");
		pusb_conf_options_get_from(opts, xpath, doc);
		xfree(xpath);
	}
	return 1;
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
	if (xpath == NULL) {
		log_error("Memory allocation failed\n");
		return 0;
	}
	memset(xpath, 0x00, xpath_len);
	snprintf(xpath, xpath_len, CONF_DEVICE_XPATH, deviceId, property);
	retval = pusb_xpath_get_string(doc, xpath, store, size);
	xfree(xpath);
	return retval;
}

static int pusb_conf_parse_device(
	t_pusb_options *opts,
	xmlDoc *doc,
	int deviceIndex,
	char *deviceId
)
{
	if (!pusb_conf_xpath_id_is_safe("Device id", deviceId))
	{
		return 0;
	}

	pusb_conf_device_get_property(opts, doc, "vendor", opts->device_list[deviceIndex].vendor, sizeof(opts->device_list[deviceIndex].vendor), deviceId);
	pusb_conf_device_get_property(opts, doc, "model", opts->device_list[deviceIndex].model, sizeof(opts->device_list[deviceIndex].model), deviceId);

	if (!pusb_conf_device_get_property(opts, doc, "serial", opts->device_list[deviceIndex].serial, sizeof(opts->device_list[deviceIndex].serial), deviceId))
	{
		return 0;
	}

	pusb_conf_device_get_property(opts, doc, "volume_uuid", opts->device_list[deviceIndex].volume_uuid, sizeof(opts->device_list[deviceIndex].volume_uuid), deviceId);
	return 1;
}

int pusb_conf_init(t_pusb_options *opts)
{
	struct utsname	u;

	memset(opts, 0x00, sizeof(*opts));
	if (uname(&u) == -1)
	{
		log_error("uname: %s\n", strerror(errno));
		return 0;
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
	opts->remote_desktop_check = 1;
	return 1;
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
	if (!pusb_conf_xpath_id_is_safe("Username", user) ||
	    !pusb_conf_xpath_id_is_safe("Service", service))
	{
		return 0;
	}
	if (strlen(user) > CONF_USER_MAXLEN)
	{
		log_error("Username \"%s\" is too long (max: %d).\n", user, CONF_USER_MAXLEN);
		return 0;
	}
	if (!(doc = xmlReadFile(file, NULL, 0)))
	{
		log_error("Unable to parse \"%s\".\n", file);
		return 0;
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
		sizeof(opts->device.name),
		10
	);
	if (!retval)
	{
		log_error("No authentication device(s) configured for user \"%s\".\n", user);
		xmlFreeDoc(doc);
		xmlCleanupParser();

		for (int currentDevice = 0; currentDevice < 10; currentDevice++)
		{
			xfree(device_list[currentDevice]);
		}
		return 0;
	}

	for (int currentDevice = 0; currentDevice < 10; currentDevice++)
	{
		if (device_list[currentDevice] == NULL || strlen(device_list[currentDevice]) == 0)
		{
			continue;
		}

		snprintf(opts->device_list[currentDevice].name, sizeof(opts->device_list[currentDevice].name), "%s", device_list[currentDevice]);
		if (!pusb_conf_parse_device(opts, doc, currentDevice, device_list[currentDevice]))
		{
			xmlFreeDoc(doc);
			xmlCleanupParser();

			for (int i = 0; i < 10; i++)
			{
				xfree(device_list[i]);
			}
			return 0;
		}
	}

	/* Mark devices that carry superuser="true" in the user's <device> elements. */
	{
		size_t su_len = sizeof(CONF_USER_XPATH) + CONF_USER_MAXLEN
		                + sizeof("device[@superuser='true']");
		char *su_xpath = xmalloc(su_len);
		if (!su_xpath)
		{
			log_error("Memory allocation failed\n");
			xmlFreeDoc(doc);
			xmlCleanupParser();
			for (int i = 0; i < 10; i++) xfree(device_list[i]);
			return 0;
		}
		snprintf(su_xpath, su_len, CONF_USER_XPATH, user, "device[@superuser='true']");

		char *su_names[10];
		for (int i = 0; i < 10; i++)
		{
			su_names[i] = xmalloc(128);
			memset(su_names[i], 0, 128);
		}

		if (pusb_xpath_get_string_list(doc, su_xpath, su_names, sizeof(opts->device.name), 10))
		{
			for (int i = 0; i < 10; i++)
			{
				if (opts->device_list[i].name[0] == '\0') continue;
				for (int j = 0; j < 10; j++)
				{
					if (su_names[j][0] == '\0') continue;
					if (strcmp(opts->device_list[i].name, su_names[j]) == 0)
					{
						opts->device_list[i].superuser = 1;
						break;
					}
				}
			}
		}

		for (int i = 0; i < 10; i++) xfree(su_names[i]);
		xfree(su_xpath);
	}

	if (!pusb_conf_parse_options(opts, doc, user, service))
	{
		xmlFreeDoc(doc);
		xmlCleanupParser();

		for (int currentDevice = 0; currentDevice < 10; currentDevice++)
		{
			xfree(device_list[currentDevice]);
		}
		return (0);
	}

	/* If the service requires a superuser device, remove non-superuser devices. */
	if (opts->superuser)
	{
		log_debug("Service \"%s\" requires superuser device. Filtering device list.\n", service);
		for (int i = 0; i < 10; i++)
		{
			if (opts->device_list[i].name[0] == '\0') continue;
			if (!opts->device_list[i].superuser)
			{
				log_debug("Device \"%s\" excluded for service \"%s\" (no superuser attribute).\n",
				          opts->device_list[i].name, service);
				memset(&opts->device_list[i], 0, sizeof(t_pusb_device));
			}
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();

	for (int currentDevice = 0; currentDevice < 10; currentDevice++)
	{
		xfree(device_list[currentDevice]);
	}
	return (1);
}
