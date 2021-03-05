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
#include <stdlib.h>
#include "log.h"
#include "conf.h"
#include "process.h"
#include "tmux.h"

int pusb_is_tty_local(char *tty)
{
    struct utmp	utsearch;
    struct utmp	*utent;

    strncpy(utsearch.ut_line, tty, sizeof(utsearch.ut_line) - 1);

    setutent();
    utent = getutline(&utsearch);
    endutent();

    if (!utent)
    {
        log_error("No utmp entry found for tty \"%s\"\n", utsearch.ut_line);
        return (0);
    } else {
        log_debug("		utmp entry for tty \"%s\" found\n", tty);
        log_debug("			utmp->ut_pid: %d\n", utent->ut_pid);
        log_debug("			utmp->ut_user: %s\n", utent->ut_user);
    }

    for (int i = 0; i < 4; ++i)
    {
        /**
         * Note: despite the property name this also works for IPv4, v4 addr would be in ut_addr_v6[0] solely.
         * See utmp man (http://manpages.ubuntu.com/manpages/bionic/de/man5/utmp.5.html)
         **/
        if (utent->ut_addr_v6[i] != 0)
        {
            log_error("Remote authentication request: %s\n", utent->ut_host);
            return (0);
        } else {
            log_debug("		Checking utmp->ut_addr_v6[%d]\n", i);
        }
    }

    log_debug("	utmp check successful, request originates from a local source!\n");
    return (1);
}

int pusb_local_login(t_pusb_options *opts, const char *user, const char *service)
{
	if (!opts->deny_remote)
	{
		log_debug("deny_remote is disabled. Skipping local check.\n");
		return (1);
	}

	log_debug("Checking whether the caller is local or not...\n");

	char name[BUFSIZ];
	pid_t pid = getpid();
    int local_request = 0;

	while (pid != 0) {
		get_process_name(pid, name);
		log_debug("	Checking pid %6d (%s)...\n", pid, name);
		get_process_parent_id(pid, & pid);

		if (strstr(name, "sshd") != NULL || strstr(name, "telnetd") != NULL) {
			log_error("One of the parent processes found to be a remote access daemon, denying.\n");
			return (0);
		}
	}

	if (strcmp(service, "gdm-password") == 0 || strcmp(service, "xdm") == 0 || strcmp(service, "lightdm") == 0) {
		log_debug("	Graphical login request detected, assuming local.");
		local_request = 1;
	}

	const char	*session_tty;
	const char	*display = getenv("DISPLAY");

	/**
	 * Magic for tmux, use env var to get tmux client id, which will be used to get tty, to set it in utsearch
	 * If display != null we can use that to verify the session, even on tmux.
	 */
	if (!local_request && strstr(name, "tmux") != NULL && display == NULL) {
		char *tmux_client_tty = tmux_get_client_tty();
		local_request = pusb_is_tty_local(tmux_client_tty);
	}

	if (!local_request && display != NULL) {
		log_debug("		Using DISPLAY %s for utmp search\n", display);
		local_request = pusb_is_tty_local((char *) display);
	}

	if (!local_request) {
		session_tty = ttyname(STDIN_FILENO);
		if (!session_tty || !(*session_tty))
		{
			log_error("Couldn't retrieve login tty, assuming remote\n");
		} else {
            if (!strncmp(session_tty, "/dev/", strlen("/dev/"))) {
                session_tty += strlen("/dev/");
            }

            log_debug("		Using TTY %s for search\n", session_tty);
            local_request = pusb_is_tty_local((char *) session_tty);
		}
	}

    if (local_request) {
        log_debug("No remote access detected, seems to be local request - allowing.\n");
    } else {
        log_debug("Couldn't confirm login tty to be local - denying.\n");
    }

    return local_request;
}

