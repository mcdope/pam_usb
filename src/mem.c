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
#include <malloc.h>
#include "mem.h"
#include "log.h"

void *xmalloc(size_t size)
{
	void *data = malloc(size);
	if (data == NULL) {
		log_error("xmalloc: out of memory\n");
		abort();
	}
	return (data);
}

void *xrealloc(void *ptr, size_t size)
{
	size_t oldsize = ptr ? malloc_usable_size(ptr) : 0;
	void *data = xmalloc(size);
	if (ptr) {
		memcpy(data, ptr, oldsize < size ? oldsize : size);
		xfree(ptr);
	}
	return (data);
}

/* noinline: prevents FORTIFY_SOURCE false positive. When inlined, the compiler
 * sees the allocation size at the call site and flags malloc_usable_size() as
 * overflowing that size (alignment padding). As a non-inline function, ptr has
 * unknown object size so FORTIFY_SOURCE skips the check. */
__attribute__((__noinline__))
void xfree(void *ptr)
{
	if (ptr)
		explicit_bzero(ptr, malloc_usable_size(ptr));
	free(ptr);
}

char *xstrdup(const char *s)
{
	char *data = strdup(s);
	if (data == NULL) {
		log_error("xstrdup: out of memory\n");
		abort();
	}
	return (data);
}
