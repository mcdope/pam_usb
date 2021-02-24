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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include "log.h"
#include "conf.h"
#include "process.h"

int pusb_local_login(t_pusb_options *opts, const char *user)
{
	if (!opts->deny_remote)
	{
	  log_debug("deny_remote is disabled. Skipping local check.\n");
	  return (1);
	}

	log_debug("Checking whether the caller is local or not...\n");
	
	pid_t pid = getpid();
	while (pid != 0) {
		char name[BUFSIZ];
		get_process_name(pid, name);
		log_debug("    Checking pid %6d (%s)...\n", pid, name);
		get_process_parent_id(pid, & pid);

		if (strstr(name, "sshd") != NULL || strstr(name, "telnetd") != NULL) {
			log_error("One of the parent processes found to be a remote access daemon, denying.\n");
			return (0);
		}

		// Check for multiplexer, in case found > use utmp approach on top
		if (strstr(name, "tmux") != NULL || strstr(name, "byobu") != NULL) {
			log_debug("    Multiplexer detected - checking utmp...\n");
			
			struct utmp	utsearch;
			struct utmp	*utent;
			const char	*from;
			from = ttyname(STDIN_FILENO); // @todo should use get_process_tty() for pid
			int	i;

			if (!from || !(*from))
			{
				log_error("        Couldn't retrieve the tty name, assuming remote\n");
				return (0);
			}

			if (!strncmp(from, "/dev/", strlen("/dev/"))) {
				from += strlen("/dev/");
			}

			log_debug("        TTY used: %s\n", from);
			strncpy(utsearch.ut_line, from, sizeof(utsearch.ut_line) - 1);
			setutent();
			utent = getutline(&utsearch);
			endutent();
			
			if (!utent)
			{
				log_error("No utmp entry found for tty \"%s\", assuming remote\n", from);
				return (0);
			}

			for (i = 0; i < 4; ++i)
			{
				/** 
				 * Note: despite the property name this also works for IPv4, v4 addr would be in ut_addr_v6[0] solely. 
				 * See utmp man (http://manpages.ubuntu.com/manpages/bionic/de/man5/utmp.5.html)
				 **/
				if (utent->ut_addr_v6[i] != 0) 
				{
					log_error("        Remote authentication request: %s\n", utent->ut_host);
					return (0);
				}
			}
		}
	}
	
	log_debug("No remote daemons or multiplexer sessions found in process chain, seems to be local request - allowing.\n");
	return (1);
}
