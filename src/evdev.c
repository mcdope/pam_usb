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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include "evdev.h"
#include "log.h"

int pusb_has_virtual_input_device(const char *input_dir)
{
	log_debug("	Checking for virtual input devices...\n");

	DIR *d = opendir(input_dir);
	if (!d) {
		int saved_errno = errno;
		log_debug("	Could not open %s for evdev scan\n", input_dir);
		return (saved_errno == EACCES || saved_errno == EPERM) ? -1 : 0;
	}

	char dev_path[PATH_MAX];
	struct dirent *ent;
	int permission_denied = 0;

	while ((ent = readdir(d)) != NULL) {
		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;

		snprintf(dev_path, sizeof(dev_path), "%s/%s", input_dir, ent->d_name);

		int fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
		if (fd < 0) {
			if (errno == EACCES || errno == EPERM)
				permission_denied = 1;
			continue;
		}

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
	return permission_denied ? -1 : 0;
}

int pusb_has_virtual_input_device_safe(const char *input_dir)
{
	struct stat st;
	if (stat(PAMUSB_EVDEV_HELPER_PATH, &st) != 0 || !S_ISREG(st.st_mode) ||
	    st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
		log_debug("	evdev helper at %s absent, not owned by root, or group/world-writable; using direct scan\n",
		          PAMUSB_EVDEV_HELPER_PATH);
		return pusb_has_virtual_input_device(input_dir);
	}

	pid_t pid = fork();
	if (pid < 0) {
		int saved_errno = errno;
		log_debug("	fork() failed (%s), using direct evdev scan\n", strerror(saved_errno));
		return pusb_has_virtual_input_device(input_dir);
	}

	if (pid == 0) {
		char *const argv[] = {PAMUSB_EVDEV_HELPER_PATH, NULL};
		char *const envp[] = {NULL};
		execve(PAMUSB_EVDEV_HELPER_PATH, argv, envp);
		_exit(2);
	}

	/* parent: poll up to 200 ms for the helper to finish */
	struct timespec deadline;
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_nsec += 200000000L;
	if (deadline.tv_nsec >= 1000000000L) {
		deadline.tv_sec  += 1;
		deadline.tv_nsec -= 1000000000L;
	}

	int status;
	for (;;) {
		pid_t r = waitpid(pid, &status, WNOHANG);
		if (r == pid)
			break;
		if (r < 0) {
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			return pusb_has_virtual_input_device(input_dir);
		}
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec > deadline.tv_sec ||
		    (now.tv_sec == deadline.tv_sec &&
		     now.tv_nsec >= deadline.tv_nsec)) {
			log_debug("	evdev helper timed out, falling back to direct scan\n");
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			return pusb_has_virtual_input_device(input_dir);
		}
		struct timespec slp = {0, 5000000L};  /* 5 ms */
		nanosleep(&slp, NULL);
	}

	if (!WIFEXITED(status)) {
		log_debug("	evdev helper exited abnormally, falling back\n");
		return pusb_has_virtual_input_device(input_dir);
	}

	switch (WEXITSTATUS(status)) {
	case 0: return 0;
	case 1: return 1;
	default:
		log_debug("	evdev helper returned unexpected code %d, falling back\n",
		          WEXITSTATUS(status));
		return pusb_has_virtual_input_device(input_dir);
	}
}
