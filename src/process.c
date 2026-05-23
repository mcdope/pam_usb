/**
 * Source: https://gist.github.com/fclairamb/a16a4237c46440bdb172
 * Modifications: removed main(), added header file
 *
 * Copyright 2014 Florent Clairambault (@fclairamb)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include "process.h"
#include "mem.h"

/**
 * Get a process name from its PID.
 * @param pid PID of the process
 * @param name Buffer to store the process name
 * 
 * Source: https://stackoverflow.com/questions/15545341/process-name-from-its-pid-in-linux
 */
void pusb_get_process_name(const pid_t pid, char *name, size_t name_len)
{
	char procfile[BUFSIZ];
	snprintf(procfile, sizeof(procfile), "/proc/%d/cmdline", pid);
	FILE* f = fopen(procfile, "r");
	if (f)
	{
		size_t size = fread(name, sizeof(char), name_len - 1, f);
		name[size] = '\0'; // Ensure null-termination
		fclose(f);
	}
}

/**
 * Get the parent PID from a PID
 * @param pid pid
 * @param ppid parent process id
 * 
 * Note: init is 1 and it has a parent id of 0.
 */
void pusb_get_process_parent_id(const pid_t pid, pid_t *ppid)
{
	char buffer[BUFSIZ];
	if (ppid == NULL)
	{
		return;
	}
	*ppid = 0;
	snprintf(buffer, sizeof(buffer), "/proc/%d/stat", pid);
	FILE* fp = fopen(buffer, "r");
	if (fp)
	{
		if (fgets(buffer, sizeof(buffer), fp) != NULL)
		{
			pusb_parse_process_stat_parent_id(buffer, ppid);
		}
		fclose(fp);
	}
}

/* Hard cap for /proc/PID/environ reads — well above /proc/sys/kernel/arg_max
 * (typically 128 KB) while bounding heap use to a known maximum. */
#define PUSB_ENVIRON_CAP (256 * 1024)

/**
 * Scan a NUL-delimited environ buffer for a variable value.
 *
 * @param buf  Buffer of NUL-separated KEY=value pairs (as from /proc/PID/environ).
 *             buf[size] must be '\0' (the caller ensures this) so that the last
 *             entry's value is always NUL-terminated for xstrdup.
 * @param size Number of data bytes in buf (not counting the sentinel NUL).
 * @param var  Variable name to find (without trailing '=').
 * @return Heap-allocated copy of the value, or NULL if not found.
 */
char *pusb_scan_environ_buffer(const char *buf, size_t size, const char *var)
{
	size_t var_len;
	size_t i;

	if (buf == NULL || var == NULL)
	{
		return NULL;
	}
	var_len = strlen(var); /* DevSkim: ignore DS140021 - var is a NUL-terminated C string, not a raw buffer */
	i = 0;

	while (i < size)
	{
		const char *entry = buf + i;
		size_t entry_len = strnlen(entry, size - i);

		if (entry_len > var_len &&
		    memcmp(var, entry, var_len) == 0 &&
		    entry[var_len] == '=')
		{
			return xstrdup(entry + var_len + 1);
		}

		i += entry_len + 1;
	}

	return NULL;
}

/**
 * Read environment variable of another process via its /proc/processId/environ file.
 *
 * @param pid pid of process to read the environment of
 * @param var envvar to look up
 *
 * @return content of var if found, else NULL
 *
 * @see https://man7.org/linux/man-pages/man7/environ.7.html
 * @see https://askubuntu.com/a/978715
 */
char *pusb_get_process_envvar(pid_t pid, char *var)
{
	char path[64];
	char *buffer;
	FILE *fp;
	size_t size;
	char *output;

	if (var == NULL)
	{
		return NULL;
	}
	if (snprintf(path, sizeof(path), "/proc/%d/environ", pid) >= (int)sizeof(path)) /* DevSkim: ignore DS185832 - path constructed from numeric PID, not user input */
	{
		return NULL;
	}
	fp = fopen(path, "r"); /* DevSkim: ignore DS154189 - path constructed from numeric PID only */
	if (!fp)
	{
		return NULL;
	}

	buffer = xmalloc(PUSB_ENVIRON_CAP);
	size = fread(buffer, sizeof(char), PUSB_ENVIRON_CAP - 1, fp);
	buffer[size] = '\0';
	fclose(fp);

	output = pusb_scan_environ_buffer(buffer, size, var);
	xfree(buffer);
	return output;
}

int pusb_parse_process_stat_parent_id(const char *stat_line, pid_t *ppid)
{
	char *endptr;
	long parsed;
	const char *comm_end;
	const char *cursor;

	if (stat_line == NULL || ppid == NULL)
		return 0;

	/*
	 * /proc/[pid]/stat field 2 (comm) is wrapped in parentheses and may
	 * contain spaces. Tokenizing the whole line on spaces can therefore read
	 * the wrong field for ppid. The fixed fields resume after the last ')':
	 * state is field 3 and ppid is field 4.
	 */
	comm_end = strrchr(stat_line, ')');
	if (comm_end == NULL)
		return 0;

	cursor = comm_end + 1;
	while (*cursor == ' ')
		cursor++;
	if (*cursor == '\0')
		return 0;

	/* Skip state field. */
	while (*cursor != '\0' && *cursor != ' ')
		cursor++;
	while (*cursor == ' ')
		cursor++;
	if (*cursor == '\0')
		return 0;

	errno = 0;
	parsed = strtol(cursor, &endptr, 10);
	if (cursor == endptr || errno != 0 || parsed < 0)
		return 0;

	*ppid = (pid_t)parsed;
	return 1;
}
