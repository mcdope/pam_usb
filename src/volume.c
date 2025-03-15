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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#ifndef __GNU__
#include <sys/mount.h>
#endif

#include "mem.h"
#include "conf.h"
#include "log.h"
#include "volume.h"

static int pusb_volume_mount(t_pusb_volume *volume)
{
	GError *error = NULL;
	GVariant *options = NULL;
	GVariantBuilder builder;
	int retval = 0;
	const gchar *const *mount_points = NULL;

	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	options = g_variant_builder_end(&builder);

	log_debug("Attempting to mount device %s.\n", volume->device);

	udisks_filesystem_call_mount_sync(
		volume->filesystem,
		options,
		&volume->mount_point,
		NULL,
		&error
	);

	if (!error)
	{
		volume->unmount = 1;
		retval = 1;
		log_debug("Mounted device %s to %s.\n", volume->device, volume->mount_point);
	}
	else if (error->code == UDISKS_ERROR_ALREADY_MOUNTED)
	{
		g_main_context_iteration(NULL, FALSE);
		mount_points = udisks_filesystem_get_mount_points(volume->filesystem);
		if (mount_points && *mount_points)
		{
			volume->mount_point = xstrdup(*mount_points);
			retval = 1;
			log_debug("Device %s mounted in between our probe and mount.\n", volume->device);
		} 
		else
		{
			log_error("Device %s mounted in between our probe and mount, but could not get the list mount points.\n", volume->device);
		}
	}
	else
	{
		log_error("Failed to mount device %s: %s\n", volume->device, error->message);
	}

	if (error)
	{
		g_error_free(error);
	}

	return (retval);
}

static t_pusb_volume *pusb_volume_probe(t_pusb_options *opts, UDisksClient *udisks)
{
	t_pusb_volume *volume = NULL;
	int maxtries = (opts->probe_timeout * 1000000) / 100000;
	int i;
	int j;
	GList *blocks = NULL;
	UDisksBlock *block = NULL;
	UDisksObject *object = NULL;
	const gchar *const *mount_points = NULL;

	if (!*(opts->device.volume_uuid))
	{
		log_debug("No UUID configured for device.\n");
		return (NULL);
	}

	log_debug("Searching for volume with uuid %s.\n", opts->device.volume_uuid);

	for (i = 0; i < maxtries; ++i)
	{
		blocks = udisks_client_get_block_for_uuid(udisks, opts->device.volume_uuid);

		if (i == 1)
		{
			log_info("Probing volume (this could take a while)...\n");
		}

		for (j = 0; j < g_list_length(blocks); ++j)
		{
			block = UDISKS_BLOCK(g_list_nth(blocks, j)->data);
			object = UDISKS_OBJECT(g_dbus_interface_get_object(G_DBUS_INTERFACE(block)));

			if (udisks_object_peek_filesystem(object))
			{
				volume = xmalloc(sizeof(t_pusb_volume));
				volume->filesystem = udisks_object_get_filesystem(object);
				volume->unmount = 0;
				volume->device = xstrdup(udisks_block_get_device(block));
				volume->mount_point = NULL;

				mount_points = udisks_filesystem_get_mount_points(volume->filesystem);
				log_debug("Found mount points: %s\n", *mount_points);
				if (mount_points && *mount_points)
				{
					volume->mount_point = xstrdup(*mount_points);
				}

				break;
			}
		}

		g_list_foreach(blocks, (GFunc) g_object_unref, NULL);
		g_list_free(blocks);

		if (volume)
		{
			log_debug("Found volume %s.\n", opts->device.volume_uuid);
			break;
		}

		usleep(100000);
		g_main_context_iteration(NULL, FALSE);
	}

	if (!volume)
	{
		log_debug("Could not find volume %s.\n", opts->device.volume_uuid);
	}

	return (volume);
}

t_pusb_volume *pusb_volume_get(t_pusb_options *opts, UDisksClient *udisks)
{
	t_pusb_volume *volume = pusb_volume_probe(opts, udisks);

	if (!volume)
	{
		return (NULL);
	}

	if (volume->mount_point)
	{
		log_debug("Volume %s is already mounted.\n", opts->device.volume_uuid);
		return (volume);
	}

	if (!pusb_volume_mount(volume))
	{
		pusb_volume_destroy(volume);
		return (NULL);
	}

	return (volume);
}

void pusb_volume_destroy(t_pusb_volume *volume)
{
	GVariantBuilder builder;
	GVariant *options;
	int ret;

	if (volume->unmount)
	{
		g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
		options = g_variant_builder_end(&builder);

		log_debug("Attempting to unmount %s from %s.\n", volume->device, volume->mount_point);

		ret = udisks_filesystem_call_unmount_sync(
			volume->filesystem,
			options,
			NULL,
			NULL
		);
		if (!ret)
		{
			log_error("Unable to unmount %s from %s\n", volume->device, volume->mount_point);
		}

		log_debug("Unmount succeeded.\n");
	}

	g_object_unref(volume->filesystem);
	xfree(volume->device);
	xfree(volume->mount_point);
	xfree(volume);
}

