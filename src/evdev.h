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

#ifndef PUSB_EVDEV_H_
#define PUSB_EVDEV_H_

/*
 * Compile-time path for the setgid helper binary that performs the evdev scan
 * with input group privileges so the authenticating user does not need to be a
 * member of the input group.  Override via -DPAMUSB_EVDEV_HELPER_PATH=... at
 * build time when installing to a non-standard prefix.
 */
#ifndef PAMUSB_EVDEV_HELPER_PATH
#define PAMUSB_EVDEV_HELPER_PATH "/usr/lib/pam_usb/pamusb-evdev-helper"
#endif

/*
 * Scans input_dir (typically "/dev/input") for virtual input devices.
 * A device is considered virtual if:
 *   - bus type is BUS_VIRTUAL (0x06)
 *   - physical path is NULL or empty
 *   - device has keyboard (EV_KEY) or pointer (EV_REL/EV_ABS) capabilities
 *
 * Returns:
 *   1  — virtual input device found
 *   0  — no virtual input device found
 *  -1  — scan inconclusive: at least one device could not be opened due to
 *         insufficient permissions (EACCES or EPERM, the latter returned by
 *         some LSMs such as AppArmor/SELinux); result should be treated as a
 *         warning, not a definitive negative
 * Production callers should pass "/dev/input".
 */
int pusb_has_virtual_input_device(const char *input_dir);

/*
 * Like pusb_has_virtual_input_device() but first tries to exec the setgid
 * helper at PAMUSB_EVDEV_HELPER_PATH with a cleared environment.  Falls back
 * to the direct scan when the helper is absent, non-executable, or times out.
 *
 * Callers that run without root should use this variant so that the scan works
 * even when the user is not a member of the input group.
 */
int pusb_has_virtual_input_device_safe(const char *input_dir);

#endif /* !PUSB_EVDEV_H_ */
