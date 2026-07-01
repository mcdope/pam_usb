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

#ifndef PUSB_TESTING_H_
#define PUSB_TESTING_H_

/*
 * PUSB_STATIC controls linkage of internal helper functions that unit tests
 * need to call directly.  In production builds the functions remain static
 * (internal linkage, full optimizer freedom, no symbol pollution in the .so).
 * When compiled with -DUNIT_TESTING the keyword is removed so the linker can
 * resolve the symbols from the linked .o file.
 *
 * This header is the single source of truth for the macro — include it in
 * every .c file that uses PUSB_STATIC, never define it inline.
 */
#ifdef UNIT_TESTING
# define PUSB_STATIC
#else
# define PUSB_STATIC static
#endif

#endif /* PUSB_TESTING_H_ */
