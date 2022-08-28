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

#ifndef PUSB_XPATH_H_
# define PUSB_XPATH_H_
# include <libxml/parser.h>

int pusb_xpath_get_string(xmlDocPtr doc, const char *path, char *value, size_t size);
int pusb_xpath_get_string_list(xmlDocPtr doc, const char *path, char *value[], size_t size);
int pusb_xpath_get_string_from(xmlDocPtr doc, const char *base, const char *path, char *value, size_t size);
int pusb_xpath_get_bool(xmlDocPtr doc, const char *path, int *value);
int pusb_xpath_get_bool_from(xmlDocPtr doc, const char *base, const char *path, int *value);
int pusb_xpath_get_time(xmlDocPtr doc, const char *path, time_t *value);
int pusb_xpath_get_time_from(xmlDocPtr doc, const char *base, const char *path, time_t *value);
int pusb_xpath_get_int(xmlDocPtr doc, const char *path, int *value);
int pusb_xpath_get_int_from(xmlDocPtr doc, const char *base, const char *path, int *value);

#endif /* !PUSB_XPATH_H_ */
