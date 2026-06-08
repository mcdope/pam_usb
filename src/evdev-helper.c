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

/*
 * Minimal setgid helper for evdev virtual-device detection.
 *
 * Installed as root:input with the setgid bit so that pam_usb can check
 * /dev/input without the authenticating user being a member of the input
 * group.
 *
 * Exit codes:
 *   0 — no virtual input device found
 *   1 — virtual input device detected (possible remote desktop)
 *   2 — scan inconclusive (permission denied even as setgid input)
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "evdev.h"
#include "log.h"

int main(void)
{
	/*
	 * Ensure stdin/stdout/stderr are open before anything else.  As a
	 * setgid binary we may be invoked with some standard fds closed; if a
	 * subsequent open() or opendir() receives fd 0, 1, or 2 as its
	 * descriptor, later I/O through those fds could misbehave.
	 */
	for (int i = 0; i < 3; i++) {
		struct stat st;
		if (fstat(i, &st) < 0 && errno == EBADF) {
			int fd = open("/dev/null", O_RDWR);
			if (fd < 0)
				return 2;
			if (fd != i) {
				dup2(fd, i);
				close(fd);
			}
		}
	}

	int r = pusb_has_virtual_input_device("/dev/input");
	if (r == 1)  return 1;
	if (r == -1) return 2;
	return 0;
}
