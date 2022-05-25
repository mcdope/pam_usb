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
#include <utmpx.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "log.h"
#include "conf.h"
#include "process.h"
#include "tmux.h"
#include "mem.h"

int pusb_is_tty_local(char *tty)
{
	struct utmpx	utsearch;
	struct utmpx	*utent;

	if (strstr(tty, "/dev/") != NULL) {
		tty += 5; // cut "/dev/"
	}

	snprintf(utsearch.ut_line, sizeof(utsearch.ut_line), "%s", tty);

	setutxent();
	utent = getutxline(&utsearch);
	endutxent();

	if (!utent)
	{
		log_debug("	No utmp entry found for tty \"%s\"\n", utsearch.ut_line);
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
			return (-1);
		} else {
			log_debug("		Checking utmp->ut_addr_v6[%d]\n", i);
		}
	}

	log_debug("	utmp check successful, request originates from a local source!\n");
	return (1);
}

char *pusb_get_tty_from_display_server(const char *display)
{
	DIR *d_proc = opendir("/proc");
	if (d_proc == NULL)
		return NULL;

	char *expected_core = (char *)xmalloc(12);
	char *cmdline_path = (char *)xmalloc(32);
	char *cmdline = (char *)xmalloc(4096);
	char *fd_path = (char *)xmalloc(32);
	char *link_path = (char *)xmalloc(32);
	char *fd_target = (char *)xmalloc(32);

	sprintf(expected_core, "-core %s", display);

	struct dirent *dent_proc;
	while ((dent_proc = readdir(d_proc)) != NULL)
	{
		if (dent_proc->d_type == DT_DIR && atoi(dent_proc->d_name) != 0 && strcmp(dent_proc->d_name, ".") != 0 && strcmp(dent_proc->d_name, "..") != 0)
		{
			memset(cmdline_path, 0, 32);
			sprintf(cmdline_path, "/proc/%s/cmdline", dent_proc->d_name);

			memset(cmdline, 0, 4096);
			int cmdline_file = open(cmdline_path, O_RDONLY | O_CLOEXEC);
			int bytes_read = read(cmdline_file, cmdline, 4096);
			close(cmdline_file);
			for (int i = 0 ; i < bytes_read; i++) {
				if (!cmdline[i] && i != bytes_read) cmdline[i] = ' '; // replace \0 with [space]
			}

			if ((strstr(cmdline, "Xorg") != NULL && strstr(cmdline, expected_core) != NULL)
				|| strstr(cmdline, "gnome-session-binary") != NULL
				|| strstr(cmdline, "gdm-wayland-session") != NULL) //@todo: find & add other wayland hosts
			{
				memset(fd_path, 0, 32);
				sprintf(fd_path, "/proc/%s/fd", dent_proc->d_name);

				DIR *d_fd = opendir(fd_path);
				if (d_fd == NULL) {
					log_debug("	Determining tty by display server failed (running 'pamusb-check' as user?)\n", fd_path);
					return NULL;
				}

				struct dirent *dent_fd;
				while ((dent_fd = readdir(d_fd)) != NULL)
				{
					if (dent_fd->d_type == DT_LNK && strcmp(dent_fd->d_name, ".") != 0 && strcmp(dent_fd->d_name, "..") != 0)
					{
						memset(link_path, 0, 32);
						memset(fd_target, 0, 32);

						sprintf(link_path, "/proc/%s/fd/%s", dent_proc->d_name, dent_fd->d_name);
						if (readlink(link_path, fd_target, 32) != -1)
						{
							if (strstr(fd_target, "/dev/tty") != NULL)
							{
								closedir(d_fd);
								closedir(d_proc);

								return fd_target;
							}
						}
					}
				}
				closedir(d_fd);
			}
		}
	}
	closedir(d_proc);

	return NULL;
}

char *pusb_get_tty_by_xorg_display(const char *display, const char *user)
{
	struct utmpx	*utent;

	setutxent();
	while ((utent = getutxent())) {
		if (strncmp(utent->ut_host, display, strnlen(display, sizeof(display))) == 0
			&& strncmp(utent->ut_user, user, strnlen(user, sizeof(user))) == 0
			&& (
				strncmp(utent->ut_line, "tty", sizeof(utent->ut_line)) == 0
				|| strncmp(utent->ut_line, "console", sizeof(utent->ut_line)) == 0
				|| strncmp(utent->ut_line, "pts", sizeof(utent->ut_line)) == 0
			)
		) {
			endutxent();
			return utent->ut_line;
		}
	}

	endutxent();
	return NULL;
}

char *pusb_get_tty_by_loginctl()
{
	struct stat sb;
	if (stat("/usr/bin/loginctl", &sb) != 0) {
		log_debug("		loginctl is not available, skipping\n");
		return (0);
	}

    char loginctl_cmd[BUFSIZ] = "loginctl show-session $(awk '/tty/ {print $1}' <(loginctl)) -p TTY | tail -1 | awk -F= '{print $2}'";
    char buf[BUFSIZ];
    FILE *fp;

    if ((fp = popen(loginctl_cmd, "r")) == NULL) {
        log_debug("		Opening pipe for 'loginctl' failed, this is quite a wtf...\n");
        return (0);
    }

    char *tty = NULL;
    if (fgets(buf, BUFSIZ, fp) != NULL) {
        tty = strtok(buf, "\n");
        log_debug("		Got tty: %s\n", tty);

        if (pclose(fp)) {
            log_debug("		Closing pipe for 'tmux loginctl-clients' failed, this is quite a wtf...\n");
        }

        return tty;
    } else {
        log_debug("		'loginctl' returned nothing.'\n");
        return (0);
    }
}

int pusb_local_login(t_pusb_options *opts, const char *user, const char *service)
{
	if (!opts->deny_remote)
	{
		log_debug("deny_remote is disabled. Skipping local check.\n");
		return (1);
	}

	log_debug("Checking whether the caller (%s) is local or not...\n", service);

	char name[BUFSIZ];
	pid_t pid = getpid();
	pid_t previous_pid = 0;
	pid_t tmux_pid = 0;
	int local_request = 0;

	while (pid != 0) {
		pusb_get_process_name(pid, name);
		log_debug("	Checking pid %6d (%s)...\n", pid, name);

		if (strstr(name, "tmux") != NULL) {
			log_debug("		Setting pid %d as fallback for tmux check\n", previous_pid);
			tmux_pid = previous_pid;
		}

		previous_pid = pid;
		pusb_get_process_parent_id(pid, & pid);
		if (strstr(name, "sshd") != NULL || strstr(name, "telnetd") != NULL) {
			log_error("One of the parent processes found to be a remote access daemon, denying.\n");
			return (0);
		}
	}

	/**
	 * These services are whitelisted because either a) they are graphical login managers and we assume these
	 * to be available only locally or b) they are authorization agents afters successful authentication.
	 */
	if (strcmp(service, "pamusb-agent") == 0 ||
		strcmp(service, "gdm-password") == 0 ||
		strcmp(service, "xdm") == 0 ||
		strcmp(service, "lightdm") == 0 ||
		strcmp(service, "sddm") == 0 ||
		strcmp(service, "polkit-1") == 0 ||
		strcmp(service, "login") == 0 // @todo: see issue #115, if we continue the check past here we gonna close the session for some reason
	) {
		log_debug("Whitelisted request by %s detected, assuming local.\n", service);
		local_request = 1;
	}

	const char	*session_tty;
	char	*display = getenv("DISPLAY");

	if (local_request == 0 && strstr(name, "tmux") != NULL && tmux_pid != 0) {
		char *tmux_client_tty = pusb_tmux_get_client_tty(tmux_pid);
		if (tmux_client_tty != NULL && tmux_client_tty != 0) {
			local_request = pusb_is_tty_local(tmux_client_tty);
		} else if (tmux_client_tty == 0) {
			return 0;
		}
	}

	if (local_request == 0 && strstr(name, "tmux") != NULL) {
		log_debug("	Checking for remote clients attached to tmux...\n");
		if (pusb_tmux_has_remote_clients(user) != 0) { // tmux has at least one remote client, can't be sure it isn't this one so denying...
			return 0;
		}
	}

	if (local_request == 0 && display != NULL) {
		log_debug("	Using DISPLAY %s for utmp search\n", display);

		if (strstr(display, ".0") != NULL) {
			// DISPLAY contains not only display but also default screen, truncate screen part in this case
			log_debug("	DISPLAY contains screen, truncating...\n");
			char display_tmp[sizeof(display)];
			snprintf(display_tmp, sizeof(display) - 2, "%s", display);
			snprintf(display, strnlen(display_tmp, sizeof(display_tmp)), "%s", display_tmp);
		}

		local_request = pusb_is_tty_local((char *) display);

		char *xorg_tty = (char *)xmalloc(32);
		if (local_request == 0)
		{
			log_debug("	Trying to get tty from display server\n");
			xorg_tty = pusb_get_tty_from_display_server(display);

			if (xorg_tty != NULL)
			{
				log_debug("	Retrying with tty %s, obtained from display server, for utmp search\n", xorg_tty);
				local_request = pusb_is_tty_local(xorg_tty);
			} else {
				log_debug("		Failed, no result while trying to get TTY from display server\n");
			}

			if (local_request == 0)
			{
				log_debug("	Trying to get tty by DISPLAY\n");
				xorg_tty = pusb_get_tty_by_xorg_display(display, user);

				if (xorg_tty != NULL)
				{
					log_debug("	Retrying with tty %s, obtained by DISPLAY, for utmp search\n", xorg_tty);
					local_request = pusb_is_tty_local(xorg_tty);
				} else {
					log_debug("		Failed, no result while searching utmp for display %s owned by user %s\n", display, user);
				}
			}
		}
	}

	if (local_request == 0) {
		log_debug("	Trying to get tty by loginctl\n");


		char *loginctl_tty = (char *)xmalloc(32);
		loginctl_tty = pusb_get_tty_by_loginctl();
		if (loginctl_tty != 0)
		{
			log_debug("	Retrying with tty %s, obtained by loginctl, for utmp search\n", loginctl_tty);
			local_request = pusb_is_tty_local(loginctl_tty);
		} else {
			log_debug("		Failed, no result while searching utmp for tty %s\n", loginctl_tty);
		}
	}

	if (local_request == 0) {
		session_tty = ttyname(STDIN_FILENO);
		if (!session_tty || !(*session_tty))
		{
			log_error("Couldn't retrieve login tty, assuming remote\n");
		} else {
			log_debug("	Fallback: Using TTY %s from ttyname() for search\n", session_tty);
			local_request = pusb_is_tty_local((char *) session_tty);
		}
	}

	if (local_request == 1) {
		log_debug("No remote access detected, seems to be local request - allowing.\n");
	} else if (local_request == 0) {
		log_debug("Couldn't confirm login tty to be neither local or remote - denying.\n");
	} else if (local_request == -1) {
		log_debug("Confirmed remote request - denying.\n");
	}

	return local_request;
}
