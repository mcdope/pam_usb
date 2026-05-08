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

#ifndef PUSB_EVDEV_H_
#define PUSB_EVDEV_H_

/*
 * Scans input_dir (typically "/dev/input") for virtual input devices.
 * A device is considered virtual if:
 *   - bus type is BUS_VIRTUAL (0x06)
 *   - physical path is NULL or empty
 *   - device has keyboard (EV_KEY) or pointer (EV_REL/EV_ABS) capabilities
 *
 * Returns 1 if such a device is found, 0 otherwise.
 * Production callers should pass "/dev/input".
 */
int pusb_has_virtual_input_device(const char *input_dir);

#endif /* !PUSB_EVDEV_H_ */
