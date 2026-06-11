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
#include <libxml/xpath.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "xpath.h"
#include "log.h"

static xmlXPathObject *pusb_xpath_match(xmlDocPtr doc, const char *path)
{
	xmlXPathContext *context = NULL;
	xmlXPathObject *result = NULL;

	context = xmlXPathNewContext(doc);
	if (context == NULL)
	{
		log_error("Unable to create XML context\n");
		return NULL;
	}
	result = xmlXPathEvalExpression((xmlChar *)path, context);
	xmlXPathFreeContext(context);
	if (result == NULL)
	{
		log_error("Error in xmlXPathEvalExpression\n");
		return NULL;
	}
	if (xmlXPathNodeSetIsEmpty(result->nodesetval))
	{
		xmlXPathFreeObject(result);
		return NULL;
	}
	return result;
}

static int pusb_xpath_strip_string(char *dest, const char *src, size_t size)
{
	int first_char = -1;
	int last_char = -1;
	int i;
	size_t len;

	for (i = 0; src[i]; ++i)
	{
		if (isspace((unsigned char)src[i]))
		{
			continue;
		}

		if (first_char == -1)
		{
			first_char = i;
		}

		last_char = i;
	}

	if (first_char == -1 || last_char == -1)
	{
		return 0;
	}

	len = (size_t)(last_char - first_char + 1);
	if (size == 0 || len >= size)
	{
		log_error("Device name is too long: %s", src);
		return 0;
	}

	memset(dest, 0x0, size);
	memcpy(dest, &(src[first_char]), len);
	dest[len] = '\0';
	return 1;
}

int pusb_xpath_get_string(
	xmlDocPtr doc, 
	const char *path,
	char *value, 
	size_t size
)
{
	xmlXPathObject *result = NULL;
	xmlNode *node = NULL;
	xmlChar *result_string = NULL;

	if (!(result = pusb_xpath_match(doc, path)))
	{
		return 0;
	}

	if (result->nodesetval->nodeNr > 1)
	{
		xmlXPathFreeObject(result);
		log_debug("Syntax error: %s: more than one record found\n", path);
		return 0;
	}

	node = result->nodesetval->nodeTab[0]->xmlChildrenNode;
	result_string = xmlNodeListGetString(doc, node, 1);
	if (!result_string)
	{
		xmlXPathFreeObject(result);
		log_debug("Empty value for %s\n", path);
		return 0;
	}
	if (!pusb_xpath_strip_string(value, (const char *)result_string, size))
	{
		log_debug("Result for %s (%s) is too long (max: %zu)\n", path, (const char *)result_string, size);
		xmlFree(result_string);
		xmlXPathFreeObject(result);
		return 0;
	}
	xmlFree(result_string);
	xmlXPathFreeObject(result);
	return 1;
}

int pusb_xpath_count_nodes(xmlDocPtr doc, const char *path)
{
	xmlXPathObject *result = pusb_xpath_match(doc, path);
	if (!result)
		return 0;
	int count = result->nodesetval->nodeNr;
	xmlXPathFreeObject(result);
	return count;
}

int pusb_xpath_get_string_list(
	xmlDocPtr doc,
	const char *path,
	char *values[],
	size_t size,
	int max_count
)
{
	xmlXPathObject *result = NULL;
	xmlNode *node = NULL;
	xmlChar *result_string = NULL;

	if (!(result = pusb_xpath_match(doc, path)))
	{
		return 0;
	}

	for (int currentResult = 0; currentResult < result->nodesetval->nodeNr && currentResult < max_count; currentResult++)
	{
		node = result->nodesetval->nodeTab[currentResult]->xmlChildrenNode;
		result_string = xmlNodeListGetString(doc, node, 1);
		if (!result_string || strcmp("", (char *)result_string) == 0)
		{
			log_debug("Empty value for %s\n", path);
			xmlFree(result_string);
			result_string = NULL;
			continue;
		}
		if (!pusb_xpath_strip_string(values[currentResult], (char *)result_string, size))
		{
			log_debug("Result for %s (%s) is too long (max: %zu)\n", path, (const char *)result_string, size);
			xmlFree(result_string);
			result_string = NULL;
			continue;
		}
		xmlFree(result_string);
		result_string = NULL;
	}

	xmlFree(result_string);
	xmlXPathFreeObject(result);
	return 1;
}

int pusb_xpath_get_string_from(
	xmlDocPtr doc,
	const char *base,
	const char *path,
	char *value, 
	size_t size
)
{
	char *xpath = NULL;
	size_t xpath_size;
	int retval;

	xpath_size = strlen(base) + strlen(path) + 1;
	xpath = xmalloc(xpath_size);
	if (xpath == NULL)
	{
		log_error("Memory allocation failed\n");
		return 0;
	}
	memset(xpath, 0x00, xpath_size);
	snprintf(xpath, xpath_size, "%s%s", base, path);
	retval = pusb_xpath_get_string(doc, xpath, value, size);
	if (retval)
	{
		log_debug("%s%s -> %s\n", base, path, value);
	}

	xfree(xpath);
	return retval;
}

int pusb_xpath_get_bool(xmlDocPtr doc, const char *path, int *value)
{
	char ret[6]; /* strlen("false") + 1 */

	if (!pusb_xpath_get_string(doc, path, ret, sizeof(ret)))
	{
		return 0;
	}

	if (!strcmp(ret, "true"))
	{
		*value = 1;
		return 1;
	}

	if (!strcmp(ret, "false"))
	{
		*value = 0;
		return 1;
	}

	log_debug("Expecting a boolean, got %s\n", ret);
	return 0;
}

int pusb_xpath_get_bool_from(
	xmlDocPtr doc,
	const char *base,
	const char *path,
	int *value
)
{
	char *xpath = NULL;
	size_t xpath_size;
	int retval;

	xpath_size = strlen(base) + strlen(path) + 1;
	xpath = xmalloc(xpath_size);
	if (xpath == NULL)
	{
		log_error("Memory allocation failed\n");
		return 0;
	}
	memset(xpath, 0x00, xpath_size);
	snprintf(xpath, xpath_size, "%s%s", base, path);
	retval = pusb_xpath_get_bool(doc, xpath, value);
	xfree(xpath);
	return retval;
}

int pusb_xpath_get_time(xmlDocPtr doc, const char *path, time_t *value)
{
	char ret[64];
	char *last;
	char *endptr;
	int coef;
	int errsv;
	long numeric;

	if (!pusb_xpath_get_string(doc, path, ret, sizeof(ret)))
	{
		return 0;
	}

	last = &(ret[strlen(ret) - 1]);
	coef = 1;
	if (*last == 's')
	{
		coef = 1;
	}
	else if (*last == 'm')
	{
		coef = 60;
	}
	else if (*last == 'h')
	{
		coef = 3600;
	}
	else if (*last == 'd')
	{
		coef = 3600 * 24;
	}
	else if (!isdigit((unsigned char)*last))
	{
		log_debug("Expecting a time modifier, got %c\n", *last);
		return 0;
	}
	if (!isdigit((unsigned char)*last))
	{
		*last = '\0';
	}

	errno = 0;
	numeric = strtol(ret, &endptr, 10);
	if (endptr == ret || *endptr != '\0' || errno != 0 || numeric < 0)
	{
		errsv = errno;
		log_debug("Invalid or out-of-range time value: %s\n", ret);
		errno = errsv;
		return 0;
	}
	if (coef > 0 && numeric > LONG_MAX / coef)
	{
		log_debug("Time value overflow: %ld * %d\n", numeric, coef);
		return 0;
	}
	*value = (time_t)(numeric * coef);

	return 1;
}

int pusb_xpath_get_time_from(
	xmlDocPtr doc,
	const char *base,
	const char *path,
	time_t *value
)
{
	char *xpath = NULL;
	size_t xpath_size;
	int retval;

	xpath_size = strlen(base) + strlen(path) + 1;
	xpath = xmalloc(xpath_size);
	if (xpath == NULL)
	{
		log_error("Memory allocation failed\n");
		return 0;
	}
	memset(xpath, 0x00, xpath_size);
	snprintf(xpath, xpath_size, "%s%s", base, path);
	retval = pusb_xpath_get_time(doc, xpath, value);
	xfree(xpath);
	return retval;
}

int pusb_xpath_get_int(xmlDocPtr doc, const char *path, int *value)
{
	char ret[64];
	char *endptr;
	int errsv;
	long numeric;

	if (!pusb_xpath_get_string(doc, path, ret, sizeof(ret)))
	{
		return 0;
	}

	errno = 0;
	numeric = strtol(ret, &endptr, 10);
	if (endptr == ret || *endptr != '\0' || errno != 0 || numeric < INT_MIN || numeric > INT_MAX)
	{
		errsv = errno;
		log_debug("Invalid or out-of-range int value: %s\n", ret);
		errno = errsv;
		return 0;
	}
	*value = (int)numeric;
	return 1;
}

int pusb_xpath_get_int_from(
	xmlDocPtr doc,
	const char *base,
	const char *path,
	int *value
)
{
	char *xpath = NULL;
	size_t xpath_size;
	int retval;

	xpath_size = strlen(base) + strlen(path) + 1;
	xpath = xmalloc(xpath_size);
	if (xpath == NULL)
	{
		log_error("Memory allocation failed\n");
		return 0;
	}
	memset(xpath, 0x00, xpath_size);
	snprintf(xpath, xpath_size, "%s%s", base, path);
	retval = pusb_xpath_get_int(doc, xpath, value);
	xfree(xpath);
	return retval;
}
