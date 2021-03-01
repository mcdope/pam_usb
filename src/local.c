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

int pusb_local_login(t_pusb_options *opts, const char *user)
{
	if (!opts->deny_remote)
	{
	  log_debug("deny_remote is disabled. Skipping local check.\n");
	  return (1);
	}

	log_debug("Checking whether the caller is local or not...\n");

    char name[BUFSIZ];
	pid_t pid = getpid();
	pid_t current_pid;

	while (pid != 0) {
		current_pid = pid;

		get_process_name(pid, name);
		get_process_parent_id(pid, & pid);

		log_debug("    Checking pid %6d (%s)...\n", current_pid, name);
		if (strstr(name, "sshd") != NULL || strstr(name, "telnetd") != NULL) {
			log_error("One of the parent processes found to be a remote access daemon, denying.\n");
			return (0);
		}
	}

    // No obvious remote access found, use utmp approach on top
    log_debug("    Checking utmp...\n");

    struct utmp	utsearch;
    struct utmp	*utent;
    const char	*from;
    char	*tty = NULL;
    const char	*display = getenv("DISPLAY");

    /**
     * Magic for tmux, use env var to get tmux client id, which willich  be used to get tty, to set it in utsearch
     * If display != null we can use that to verify the session, even on tmux.
     */
    if (strstr(name, "tmux") != NULL && display == NULL) {
        const char *tmux_details = getenv("TMUX");
        if (tmux_details == NULL) {
            log_error("tmux detected, but TMUX env var missing. Denying since remote check impossible without it!\n");
            return (0);
        }

        char *tmux_client_id = strrchr(tmux_details, ',');
        tmux_client_id++; // ... to strip leading comma
        log_debug("        Got tmux_client_id: %s\n", tmux_client_id);

        char get_tmux_session_details_cmd[32];
        sprintf(get_tmux_session_details_cmd, "tmux list-clients -t %s", tmux_client_id);
        log_debug("        Built get_tmux_session_details_cmd: %s\n", get_tmux_session_details_cmd);

        char buf[BUFSIZ];
        FILE *fp;
        if ((fp = popen(get_tmux_session_details_cmd, "r")) == NULL) {
            log_error("tmux detected, but couldn't get session details. Denying since remote check impossible without it!\n");
            return (0);
        }

        char *tmux_client_tty = NULL;
        if (fgets(buf, BUFSIZ, fp) != NULL) {
            tmux_client_tty = strtok(buf, ":");
            tmux_client_tty += strlen("/dev/");
            log_debug("        Got tmux_client_tty: %s\n", tmux_client_tty);
        }

        if(pclose(fp)) {
            log_debug("        Closing pipe for 'tmux list-clients' failed, this is quite a wtf...");
        }

        strncpy(utsearch.ut_line, tmux_client_tty, sizeof(utsearch.ut_line) - 1);
    } else if (display == NULL) {
        // No tmux, no Xsession detected, set process tty in utsearch
        from = ttyname(STDIN_FILENO);
        get_process_tty(current_pid, tty);
        log_debug("    Process uses TTY (ttyname): %s\n", from);
        log_debug("    Process uses TTY (get_process_tty): %s\n", tty);
        if (!strncmp(from, "/dev/", strlen("/dev/"))) {
            from += strlen("/dev/");
        }

        if (!from || !(*from))
        {
            log_error("Couldn't retrieve login tty, assuming remote\n");
            return (0);
        }

        log_debug("        Using TTY %s for search\n", from);
        strncpy(utsearch.ut_line, from, sizeof(utsearch.ut_line) - 1);
    } else {
        // No tmux, Xsession detected, set DISPLAY in utsearch
        log_debug("        Using DISPLAY %s for search\n", display);
        strncpy(utsearch.ut_line, display, sizeof(utsearch.ut_line) - 1);
    }

    setutent();
    utent = getutline(&utsearch);
    endutent();

    if (!utent)
    {
        log_error("No utmp entry found for tty \"%s\", assuming remote\n", utsearch.ut_line);
        return (0);
    } else {
        log_debug("        utmp entry found\n");
        log_debug("            utmp->ut_pid: %d\n", utent->ut_pid);
        log_debug("            utmp->ut_user: %s\n", utent->ut_user);
    }

    int	i;
    for (i = 0; i < 4; ++i)
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
            log_debug("        Checking utmp->ut_addr_v6[%d]\n", i);
        }
    }

    log_debug("    utmp check successful!\n");
	
	log_debug("No remote daemons or multiplexer sessions found in process chain, seems to be local request - allowing.\n");
	return (1);
}
