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

#define _GNU_SOURCE
#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
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
		if (*cursor == '\'' || *cursor == '"' || *cursor < 0x20 || *cursor == 0x7f)
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		snprintf(xpath, xpath_size, opt_list[i].name, opt_list[i].value, "");
#pragma GCC diagnostic pop
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
	char *opt_xpath;
	size_t opt_xpath_len;

	if (!pusb_conf_xpath_id_is_safe("Device id", deviceId))
	{
		return 0;
	}

	opt_xpath_len = strlen(CONF_DEVICE_XPATH) + strlen(deviceId) + strlen("option") + 1; /* DevSkim: ignore DS140021 - all inputs are null-terminated: CONF_DEVICE_XPATH is a literal, deviceId validated above, "option" is a literal */
	opt_xpath = xmalloc(opt_xpath_len);
	{
		int ret = snprintf(opt_xpath, opt_xpath_len, CONF_DEVICE_XPATH, deviceId, "option");
		if (ret > 0 && (size_t)ret < opt_xpath_len && pusb_xpath_count_nodes(doc, opt_xpath) > 0)
		{
			log_error("Device \"%s\": per-device <option> elements are not applied at runtime "
			          "and will be ignored. Use <defaults>, <users>, or <services> instead.\n",
			          deviceId);
		}
		xfree(opt_xpath);
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

void pusb_conf_free(t_pusb_options *opts)
{
	if (opts->device_list)
	{
		xfree(opts->device_list);
		opts->device_list = NULL;
		opts->device_count = 0;
	}
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
	char device_xpath[sizeof(CONF_USER_XPATH) + CONF_USER_MAXLEN + sizeof("device")];

	if (!pusb_conf_xpath_id_is_safe("Username", user) ||
	    !pusb_conf_xpath_id_is_safe("Service", service))
	{
		return 0;
	}
	log_debug("Parsing settings for user \"%s\", service \"%s\"...\n", user, service);
	if (strlen(user) > CONF_USER_MAXLEN)
	{
		log_error("Username \"%s\" is too long (max: %d).\n", user, CONF_USER_MAXLEN);
		return 0;
	}
	// XML_PARSE_NONET blocks all network-URI entity fetches (http://, ftp://). // DevSkim: ignore DS137138 - false positive, this is a comment not a URL
	// XML_PARSE_NOENT forces eager substitution so that a NONET-blocked entity
	// reference is expanded to empty content during parsing (where NONET is active)
	// rather than being left as an unexpanded node that could be fetched later.
	// file:// entities bypass NONET; exploiting them requires write access to the
	// root-owned config file, which implies full system compromise already.
	// Thread-safe per-context entity blocking (xmlCtxtSetExternalEntityLoader)
	// is available only in libxml2 >= 2.13.
	// lgtm[cpp/xxe]
	if (!(doc = xmlReadFile(file, NULL, XML_PARSE_NONET | XML_PARSE_NOENT)))
	{
		log_error("Unable to parse \"%s\".\n", file);
		return 0;
	}
	snprintf(device_xpath, sizeof(device_xpath), CONF_USER_XPATH, user, "device");

	int n_devices = pusb_xpath_count_nodes(doc, device_xpath);
	if (n_devices == 0)
	{
		log_error("No authentication device(s) configured for user \"%s\".\n", user);
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return 0;
	}
	if (n_devices < 0 || (size_t)n_devices > SIZE_MAX / sizeof(t_pusb_device))
	{
		log_error("Device count for user \"%s\" would overflow allocation.\n", user);
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return 0;
	}

	char **device_list = xmalloc(n_devices * sizeof(char *));
	if (!device_list)
	{
		log_error("Memory allocation failed\n");
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return 0;
	}
	for (int i = 0; i < n_devices; i++)
	{
		device_list[i] = xmalloc(128);
		memset(device_list[i], 0x0, 128);
	}

	opts->device_list = xmalloc(n_devices * sizeof(t_pusb_device));
	if (!opts->device_list)
	{
		log_error("Memory allocation failed\n");
		xmlFreeDoc(doc);
		xmlCleanupParser();
		for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
		xfree(device_list);
		return 0;
	}
	memset(opts->device_list, 0x0, n_devices * sizeof(t_pusb_device));
	opts->device_count = n_devices;

	pusb_xpath_get_string_list(doc, device_xpath, device_list, sizeof(opts->device.name), n_devices);

	for (int currentDevice = 0; currentDevice < n_devices; currentDevice++)
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
			for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
			xfree(device_list);
			xfree(opts->device_list);
			opts->device_list = NULL;
			opts->device_count = 0;
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
			for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
			xfree(device_list);
			xfree(opts->device_list);
			opts->device_list = NULL;
			opts->device_count = 0;
			return 0;
		}
		snprintf(su_xpath, su_len, CONF_USER_XPATH, user, "device[@superuser='true']");

		char **su_names = xmalloc(n_devices * sizeof(char *));
		if (!su_names)
		{
			log_error("Memory allocation failed\n");
			xfree(su_xpath);
			xmlFreeDoc(doc);
			xmlCleanupParser();
			for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
			xfree(device_list);
			xfree(opts->device_list);
			opts->device_list = NULL;
			opts->device_count = 0;
			return 0;
		}
		for (int i = 0; i < n_devices; i++)
		{
			su_names[i] = xmalloc(128);
			memset(su_names[i], 0, 128);
		}

		if (pusb_xpath_get_string_list(doc, su_xpath, su_names, sizeof(opts->device.name), n_devices))
		{
			for (int i = 0; i < n_devices; i++)
			{
				if (opts->device_list[i].name[0] == '\0') continue;
				for (int j = 0; j < n_devices; j++)
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

		for (int i = 0; i < n_devices; i++) xfree(su_names[i]);
		xfree(su_names);
		xfree(su_xpath);
	}

	if (!pusb_conf_parse_options(opts, doc, user, service))
	{
		xmlFreeDoc(doc);
		xmlCleanupParser();
		for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
		xfree(device_list);
		xfree(opts->device_list);
		opts->device_list = NULL;
		opts->device_count = 0;
		return (0);
	}

	/* If the service requires a superuser device, remove non-superuser devices. */
	if (opts->superuser)
	{
		int remaining = 0;
		log_debug("Service \"%s\" requires superuser device. Filtering device list.\n", service);
		for (int i = 0; i < n_devices; i++)
		{
			if (opts->device_list[i].name[0] == '\0') continue;
			if (!opts->device_list[i].superuser)
			{
				log_debug("Device \"%s\" excluded for service \"%s\" (no superuser attribute).\n",
				          opts->device_list[i].name, service);
				memset(&opts->device_list[i], 0, sizeof(t_pusb_device));
			}
			else
			{
				remaining++;
			}
		}
		if (remaining == 0)
		{
			log_error("Service \"%s\" for user \"%s\" requires a superuser-capable device "
			          "but none of the registered devices have the superuser attribute.\n",
			          service, user);
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();

	for (int i = 0; i < n_devices; i++) xfree(device_list[i]);
	xfree(device_list);
	return (1);
}
