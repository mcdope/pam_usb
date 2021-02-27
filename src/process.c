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
#include <proc/readproc.h>
#include <proc/devname.h>
#include "process.h"

/**
 * Get a process name from its PID.
 * @param pid PID of the process
 * @param name Name of the process
 * 
 * Source: http://stackoverflow.com/questions/15545341/process-name-from-its-pid-in-linux
 */
void get_process_name(const pid_t pid, char * name) {
	char procfile[BUFSIZ];
	sprintf(procfile, "/proc/%d/cmdline", pid);
	FILE* f = fopen(procfile, "r");
	if (f) {
		size_t size;
		size = fread(name, sizeof (char), sizeof (procfile), f);
		if (size > 0) {
			if ('\n' == name[size - 1])
				name[size - 1] = '\0';
		}
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
void get_process_parent_id(const pid_t pid, pid_t * ppid) {
	char buffer[BUFSIZ];
	sprintf(buffer, "/proc/%d/stat", pid);
	FILE* fp = fopen(buffer, "r");
	if (fp) {
		size_t size = fread(buffer, sizeof (char), sizeof (buffer), fp);
		if (size > 0) {
			// See: http://man7.org/linux/man-pages/man5/proc.5.html section /proc/[pid]/stat
			strtok(buffer, " "); // (1) pid  %d
			strtok(NULL, " "); // (2) comm  %s
			strtok(NULL, " "); // (3) state  %c
			char * s_ppid = strtok(NULL, " "); // (4) ppid  %d
			*ppid = atoi(s_ppid);
		}
		fclose(fp);
	}
}

/**
 * Get TTY for process from its PID.
 * @param pid PID of the process
 * @param name Will receive TTY of the process
 * 
 * @author Tobias Bäumer <tobiasbaeumer@gmail.com>
 */
void get_process_tty(const pid_t pid, char * name) 
{  
	printf("process.c -> get_process_tty() -> invoked for pid: %d\n", pid);
	PROCTAB *proctab = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
	proc_t procinfo;
	memset(&procinfo, 0, sizeof(procinfo));
	char tty_name[TTY_NAME_MAX];
	
	while (readproc(proctab, &procinfo) != NULL)
	{
		if (procinfo.tid == pid) {
			printf("process.c -> get_process_tty() -> tid: %d\n", procinfo.tid);
			printf("process.c -> get_process_tty() -> cmdline: %s\n", procinfo.cmd);
			printf("process.c -> get_process_tty() -> tty: %d\n", procinfo.tty);

			dev_to_tty(tty_name, TTY_NAME_MAX, procinfo.tty, pid, ABBREV_DEV);
			printf("process.c -> get_process_tty() -> tty written back: %s\n", tty_name);
			
			break;
		}
	}
	
	closeproc(proctab);
}