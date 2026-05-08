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

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include "log.h"

int pusb_has_virtual_input_device(const char *input_dir)
{
	DIR *d = opendir(input_dir);
	if (!d) {
		log_debug("	Could not open %s for evdev scan\n", input_dir);
		return 0;
	}

	char dev_path[PATH_MAX];
	struct dirent *ent;

	while ((ent = readdir(d)) != NULL) {
		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;

		snprintf(dev_path, sizeof(dev_path), "%s/%s", input_dir, ent->d_name);

		int fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;

		struct libevdev *dev = NULL;
		int rc = libevdev_new_from_fd(fd, &dev);
		if (rc < 0) {
			close(fd);
			continue;
		}

		int bustype  = libevdev_get_id_bustype(dev);
		const char *phys = libevdev_get_phys(dev);
		int has_input = (
			libevdev_has_event_type(dev, EV_KEY) ||
			libevdev_has_event_type(dev, EV_REL) ||
			libevdev_has_event_type(dev, EV_ABS)
		);

		int is_virtual = (
			bustype == BUS_VIRTUAL &&
			(phys == NULL || phys[0] == '\0') &&
			has_input
		);

		log_debug("	evdev: %s bustype=0x%02x phys=%s virtual=%d\n",
				  ent->d_name, bustype, phys ? phys : "(null)", is_virtual);

		libevdev_free(dev);
		close(fd);

		if (is_virtual) {
			closedir(d);
			return 1;
		}
	}

	closedir(d);
	return 0;
}
