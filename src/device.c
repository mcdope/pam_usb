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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <udisks/udisks.h>

#include "conf.h"
#include "log.h"
#include "pad.h"
#include "device.h"

static int pusb_device_connected(t_pusb_options *opts, UDisksClient *udisks)
{
	GDBusObjectManager *manager = udisks_client_get_object_manager(udisks);
	GList *objects = g_dbus_object_manager_get_objects(manager);
	int retval = 0;
	int i;
	UDisksObject *object = NULL;
	UDisksDrive *drive = NULL;

	for (int currentDevice = 0; currentDevice < 10; currentDevice++)
	{
		if (strcmp(opts->device_list[currentDevice].name, "") == 0)
		{
			continue;
		}

		log_error("Searching for \"%s\" in the hardware database...\n", opts->device_list[currentDevice].name);

		for (i = 0; i < g_list_length(objects); ++i)
		{
			object = UDISKS_OBJECT(g_list_nth(objects, i)->data);
			if (udisks_object_peek_drive(object))
			{
				drive = udisks_object_get_drive(object);
				retval = strcmp(udisks_drive_get_serial(drive), opts->device_list[currentDevice].serial) == 0;

				if (strcmp(opts->device_list[currentDevice].vendor, "Generic") != 0)
				{
					retval = retval && strcmp(udisks_drive_get_vendor(drive), opts->device_list[currentDevice].vendor) == 0;
				}

				if (strcmp(opts->device_list[currentDevice].model, "Generic") != 0)
				{
					retval = retval && strcmp(udisks_drive_get_model(drive), opts->device_list[currentDevice].model) == 0;
				}

				g_object_unref(drive);
				if (retval) {
					strncpy(opts->device.name, opts->device_list[currentDevice].name, sizeof(opts->device.name) - 1);
					strncpy(opts->device.vendor, opts->device_list[currentDevice].vendor, sizeof(opts->device.vendor) - 1);
					strncpy(opts->device.model, opts->device_list[currentDevice].model, sizeof(opts->device.model) - 1);
					strncpy(opts->device.serial, opts->device_list[currentDevice].serial, sizeof(opts->device.serial) - 1);
					strncpy(opts->device.volume_uuid, opts->device_list[currentDevice].volume_uuid, sizeof(opts->device.volume_uuid) - 1);
					currentDevice = 10;
					break;
				}
			}
		}
	}

	if (retval)
	{
		log_info("Authentication device \"%s\" is connected.\n", opts->device.name);
	}
	else
	{
		log_error("None of the configured authentication devices is connected.\n");
	}

	for (i = 0; i < g_list_length(objects); ++i)
	{
		g_object_unref(g_list_nth(objects, i)->data);
	}
	g_list_free(objects);

	return (retval);
}

int pusb_device_check(t_pusb_options *opts, const char *user)
{
	UDisksClient *udisks = NULL;
	GError *udisks_client_error = NULL;
	int retval = 0;

	udisks = udisks_client_new_sync(NULL, &udisks_client_error);
	if (udisks_client_error != NULL)
	{
		log_error("Unable to check for device, could not get UDisksClient! Error was: %s\n", udisks_client_error->message);
		g_error_free(udisks_client_error);
		return (0);
	}

	if (!pusb_device_connected(opts, udisks))
	{
		g_object_unref(udisks);
		return (0);
	}

	if (opts->one_time_pad)
	{
		log_info("Performing one time pad verification...\n");
		retval = pusb_pad_check(opts, udisks, user);
	}
	else
	{
		log_debug("One time pad is disabled, no more verifications to do.\n");
		retval = 1;
	}

	g_object_unref(udisks);
	return (retval);
}
