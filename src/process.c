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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
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
	sprintf(procfile, "/proc/%d/cmdline", pid);
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
	sprintf(buffer, "/proc/%d/stat", pid);
	FILE* fp = fopen(buffer, "r");
	if (fp)
	{
		if (fgets(buffer, sizeof(buffer), fp) != NULL)
		{
			// See: https://man7.org/linux/man-pages/man5/proc.5.html section /proc/[pid]/stat
			strtok(buffer, " "); // (1) pid  %d
			strtok(NULL, " "); // (2) comm  %s
			strtok(NULL, " "); // (3) state  %c
			char *s_ppid = strtok(NULL, " "); // (4) ppid  %d
			*ppid = atoi(s_ppid);
		}
		fclose(fp);
	}
}

/**
 * Read environment variable of another process via its /proc/processId/environ file. To so so
 * it replaces the zero bytes by # to be able to use strtok on its content. When the requested variable 
 * is found its name and the equals/assign character will be cut off and the result then returned.
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
	char buffer[BUFSIZ];
	char *output = NULL;

	sprintf(buffer, "/proc/%d/environ", pid);
	FILE* fp = fopen(buffer, "r");
	if (fp)
	{
		size_t size = fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
		buffer[size] = '\0'; // Ensure null-termination
		fclose(fp);

		for (size_t i = 0; i < size; i++)
		{
			if (buffer[i] == '\0') buffer[i] = '#'; // replace \0 with "#" since strtok uses \0 internally
		}

		if (size > 0)
		{
			char* variable_content = strtok(buffer, "#");
			while (variable_content != NULL)
			{
				if (strncmp(var, variable_content, strlen(var)) == 0 && variable_content[strlen(var)] == '=')
				{
					output = strdup(variable_content + strlen(var) + 1); // Allocate memory and copy the content
					break;
				}

				variable_content = strtok(NULL, "#");
			}
		}
	}

	return output;
}