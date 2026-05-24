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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"
#include "conf.h"
#include "process.h"
#include "tmux.h"
#include "mem.h"
#include "rmsvc.h"
#include "evdev.h"

#define PUSB_DISPLAY_MAX 256

static int pusb_utmpx_field_equals(const char *field, size_t field_len, const char *value)
{
	size_t value_len;

	if (field == NULL || value == NULL)
	{
		return 0;
	}

	value_len = strnlen(value, field_len + 1);
	if (value_len > field_len)
	{
		return 0;
	}

	if (memcmp(field, value, value_len) != 0)
	{
		return 0;
	}

	return value_len == field_len || field[value_len] == '\0';
}

static int pusb_utmpx_field_starts_with(const char *field, size_t field_len, const char *prefix)
{
	size_t prefix_len;

	if (field == NULL || prefix == NULL)
	{
		return 0;
	}

	prefix_len = strlen(prefix);
	return prefix_len <= field_len && memcmp(field, prefix, prefix_len) == 0;
}

int pusb_is_tty_local(char *tty)
{
	struct utmpx utsearch;
	struct utmpx *utent;

	if (strstr(tty, "/dev/") != NULL) 
	{
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

	/**
	 * Note: despite the property name this also works for IPv4, v4 addr would be in ut_addr_v6[0] solely while for v6 it will have just a part of the ip. All four words are checked so that IPv6 addresses with a zero first word (e.g. ::ffff:x.x.x.x mapped addresses) are not incorrectly treated as local.
	 **/
	if (utent->ut_addr_v6[0] || utent->ut_addr_v6[1] ||
	    utent->ut_addr_v6[2] || utent->ut_addr_v6[3]) {
		char ipbuf[INET6_ADDRSTRLEN];
		const char *ipaddr;
		if (utent->ut_addr_v6[1] || utent->ut_addr_v6[2] || utent->ut_addr_v6[3]) {
			/* IPv6: all four 32-bit words are used */
			ipaddr = inet_ntop(AF_INET6, utent->ut_addr_v6, ipbuf, sizeof(ipbuf));
		} else {
			/* IPv4: only ut_addr_v6[0] is populated */
			struct in_addr ipnetw;
			ipnetw.s_addr = utent->ut_addr_v6[0];
			ipaddr = inet_ntop(AF_INET, &ipnetw, ipbuf, sizeof(ipbuf));
		}
		if (ipaddr == NULL) ipaddr = "(unknown)";

		log_error("Remote authentication request, host: %.*s, ip: %s\n",
		          (int)sizeof(utent->ut_host), utent->ut_host, ipaddr);
		return (-1);
	}

	log_debug("	utmp check successful, request originates from a local source!\n");
	return (1);
}

char *pusb_get_tty_from_display_server(const char *display)
{
	DIR *d_proc = opendir("/proc");
	if (d_proc == NULL) {
		return NULL;
	}

	char *cmdline_path = (char *)xmalloc(PATH_MAX);
	char *cmdline = (char *)xmalloc(4096);
	char *fd_path = (char *)xmalloc(PATH_MAX);
	char *link_path = (char *)xmalloc(PATH_MAX);
	char *fd_target = (char *)xmalloc(PATH_MAX);

	struct dirent *dent_proc;
	while ((dent_proc = readdir(d_proc)) != NULL)
	{
		if (dent_proc->d_type == DT_DIR && atoi(dent_proc->d_name) != 0 && strcmp(dent_proc->d_name, ".") != 0 && strcmp(dent_proc->d_name, "..") != 0)
		{
			memset(cmdline_path, 0, PATH_MAX);
			snprintf(cmdline_path, PATH_MAX, "/proc/%s/cmdline", dent_proc->d_name);

			memset(cmdline, 0, 4096);
			int cmdline_file = open(cmdline_path, O_RDONLY | O_CLOEXEC);
			if (cmdline_file < 0) continue;
			int bytes_read = read(cmdline_file, cmdline, 4096);
			close(cmdline_file);
			for (int i = 0 ; i < bytes_read; i++) 
			{
				if (!cmdline[i] && i != bytes_read) 
				{
					cmdline[i] = ' '; // replace \0 with [space]
				}
			}

			if ((strstr(cmdline, "Xorg") != NULL && strstr(cmdline, display) != NULL)
				|| strstr(cmdline, "gnome-session-binary") != NULL
				|| strstr(cmdline, "gdm-wayland-session") != NULL) //@todo: find & add other wayland hosts
			{
				memset(fd_path, 0, PATH_MAX);
				snprintf(fd_path, PATH_MAX, "/proc/%s/fd", dent_proc->d_name);

				DIR *d_fd = opendir(fd_path);
				if (d_fd == NULL) {
					log_debug("	Determining tty by display server failed on %s (running 'pamusb-check' as user?)\n", fd_path);

					xfree(cmdline_path);
					xfree(cmdline);
					xfree(fd_path);
					xfree(link_path);
					xfree(fd_target);
					closedir(d_proc);

					return NULL;
				}

				struct dirent *dent_fd;
				while ((dent_fd = readdir(d_fd)) != NULL)
				{
					if (dent_fd->d_type == DT_LNK && strcmp(dent_fd->d_name, ".") != 0 && strcmp(dent_fd->d_name, "..") != 0)
					{
						memset(link_path, 0, PATH_MAX);
						memset(fd_target, 0, PATH_MAX);

						snprintf(link_path, PATH_MAX, "/proc/%s/fd/%s", dent_proc->d_name, dent_fd->d_name);
						ssize_t rlen = readlink(link_path, fd_target, PATH_MAX - 1);
						if (rlen != -1)
						{
							fd_target[rlen] = '\0';
							if (strstr(fd_target, "/dev/tty") != NULL)
							{
								closedir(d_fd);
								closedir(d_proc);

								xfree(cmdline_path);
								xfree(cmdline);
								xfree(fd_path);
								xfree(link_path);

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

	xfree(cmdline_path);
	xfree(cmdline);
	xfree(fd_path);
	xfree(link_path);
	xfree(fd_target);

	return NULL;
}

char *pusb_get_tty_by_xorg_display(const char *display, const char *user)
{
	struct utmpx *utent;

	setutxent();
	while ((utent = getutxent())) 
	{
		if (pusb_utmpx_field_equals(utent->ut_host, sizeof(utent->ut_host), display)
			&& pusb_utmpx_field_equals(utent->ut_user, sizeof(utent->ut_user), user)
			&& (
				pusb_utmpx_field_starts_with(utent->ut_line, sizeof(utent->ut_line), "tty")
				|| pusb_utmpx_field_starts_with(utent->ut_line, sizeof(utent->ut_line), "console")
				|| pusb_utmpx_field_starts_with(utent->ut_line, sizeof(utent->ut_line), "pts")
			)
		)
		{
			endutxent();
			return xstrdup(utent->ut_line);
		}
	}

	endutxent();
	return NULL;
}

static const char *pusb_loginctl_parse_output(char *buf)
{
	char *saveptr = NULL;
	const char *val = strtok_r(buf, "\n", &saveptr);
	return (val && *val != '\0') ? val : NULL;
}

char *pusb_get_tty_by_loginctl()
{
	char loginctl_cmd[BUFSIZ] = "LC_ALL=C; LOGINCTL_SESSION_ID=`/usr/bin/loginctl user-status | /usr/bin/grep -m 1  \"├─session-\" | /usr/bin/grep -o '[0-9]\\+'`; /usr/bin/loginctl show-session $LOGINCTL_SESSION_ID -p TTY | /usr/bin/awk -F= '{print $2}'";
	char buf[BUFSIZ];
	FILE *fp;

	if ((fp = popen(loginctl_cmd, "r")) == NULL)
	{
		log_debug("		Opening pipe for 'loginctl' failed, this is quite a wtf...\n");
		return NULL;
	}

	const char *tty = NULL;
	if (fgets(buf, BUFSIZ, fp) != NULL)
	{
		tty = pusb_loginctl_parse_output(buf);
		if (!tty)
		{
			log_debug("		'loginctl' returned empty TTY field, treating as unknown.\n");
			if (pclose(fp))
				log_debug("		Closing pipe for 'loginctl' failed, this is quite a wtf...\n");
			return NULL;
		}
		log_debug("		Got tty: %s\n", tty);

		if (pclose(fp))
		{
			log_debug("		Closing pipe for 'loginctl' failed, this is quite a wtf...\n");
		}

		return xstrdup(tty);
	}
	else
	{
		log_debug("		'loginctl' returned nothing.\n");
		if (pclose(fp))
		{
		    log_debug("		Closing pipe for 'loginctl' failed, this is quite a wtf...\n");
		}

		return NULL;
	}
}

int pusb_is_sddm_active(void)
{
	const char *cmd = "LC_ALL=C /usr/bin/loginctl list-sessions --no-legend | /usr/bin/awk '$3 == \"sddm\" {print \"1\"; exit}'";
	char buf[BUFSIZ];
	FILE *fp;

	if ((fp = popen(cmd, "r")) == NULL)
	{
		int errsv = errno;
		log_debug("		Opening pipe for SDDM active check failed.\n");
		errno = errsv;
		return 0;
	}

	int active = (fgets(buf, BUFSIZ, fp) != NULL) ? 1 : 0;

	int errsv = errno;
	if (pclose(fp) != 0)
		log_debug("		Closing pipe for SDDM active check failed.\n");
	errno = errsv;

	return active;
}

char *pusb_get_tty_by_sddm(void)
{
	const char *cmd = "LC_ALL=C /usr/bin/loginctl list-sessions --no-legend | /usr/bin/awk '$3 == \"sddm\" {print $5}'";
	char buf[BUFSIZ];
	FILE *fp;
	char *result = NULL;

	if ((fp = popen(cmd, "r")) == NULL)
	{
		int errsv = errno;
		log_debug("		Opening pipe for SDDM loginctl check failed.\n");
		errno = errsv;
		return NULL;
	}

	if (fgets(buf, BUFSIZ, fp) != NULL)
	{
		const char *tty = pusb_loginctl_parse_output(buf);
		if (tty)
		{
			log_debug("		Got SDDM tty: %s\n", tty);
			result = xstrdup(tty);
		}
		else
		{
			log_debug("		SDDM loginctl returned empty TTY, treating as unknown.\n");
		}
	}
	else
	{
		log_debug("		SDDM loginctl returned nothing (SDDM not running or no session found).\n");
	}

	int errsv = errno;
	if (pclose(fp) != 0)
		log_debug("		Closing pipe for SDDM loginctl check failed.\n");
	errno = errsv;

	return result;
}

int pusb_is_loginctl_local()
{
	char loginctl_cmd[BUFSIZ] = "LC_ALL=C; LOGINCTL_SESSION_ID=`/usr/bin/loginctl user-status | /usr/bin/grep -m 1  \"├─session-\" | /usr/bin/grep -o '[0-9]\\+'`; /usr/bin/loginctl show-session $LOGINCTL_SESSION_ID -p Remote | /usr/bin/awk -F= '{print $2}'";
	char buf[BUFSIZ];
	FILE *fp;

	if ((fp = popen(loginctl_cmd, "r")) == NULL)
	{
		log_debug("		Opening pipe for 'loginctl' failed, this is quite a wtf...\n");
		return 0;
	}

	const char *is_remote = NULL;
	if (fgets(buf, BUFSIZ, fp) != NULL)
	{
		is_remote = pusb_loginctl_parse_output(buf);
		if (!is_remote)
		{
			log_debug("		loginctl returned empty Remote field, treating as unknown.\n");
			if (pclose(fp))
				log_debug("		Closing pipe for 'loginctl' failed, this is quite a wtf...\n");
			return 0;
		}

		log_debug("		loginctl considers this session to be remote: %s\n", is_remote);

		if (pclose(fp))
		{
			log_debug("		Closing pipe for 'loginctl' failed, this is quite a wtf...\n");
		}

		if (strcmp(is_remote, "no") == 0)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		log_debug("		'loginctl' returned nothing.\n");
		return 0;
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

	char *xrdpSession = getenv("XRDP_SESSION");
	if (xrdpSession != NULL) {
		log_error("XRDP session detected (%s), denying.\n", xrdpSession);
		return (0);
	}

	if (opts->remote_desktop_check) {
		log_debug("Checking for active remote desktop services...\n");
		if (pusb_has_active_remote_service(opts) == 1) {
			log_error("Active remote desktop service with connection detected, denying.\n");
			return (0);
		}
		int evdev_result = pusb_has_virtual_input_device("/dev/input");
		if (evdev_result == 1) {
			log_error("Virtual input device detected (possible remote desktop tool), denying.\n");
			return (0);
		} else if (evdev_result == -1) {
			log_error("Cannot check for virtual input devices (permission denied on /dev/input). "
			          "Run pamusb-check as root or add user to the 'input' group for reliable "
			          "remote desktop detection. If using AppArmor/SELinux, check policy.\n");
		}
	}

	while (pid != 0) 
	{
		pusb_get_process_name(pid, name, BUFSIZ);
		log_debug("	Checking pid %6d (%s)...\n", pid, name);

		if (strstr(name, "tmux") != NULL) 
		{
			log_debug("		Setting pid %d as fallback for tmux check\n", previous_pid);
			tmux_pid = previous_pid;
		}

		previous_pid = pid;
		pusb_get_process_parent_id(pid, & pid);
		if (strstr(name, "sshd") != NULL
			|| strstr(name, "telnetd") != NULL
			|| (strstr(name, "code") != NULL && strstr(name, "tunnel") != NULL)
		) {
			log_error("One of the parent processes found to be a remote access daemon, denying.\n");
			return (0);
		}
	}

	const char *session_tty;
	const char *display_env = secure_getenv("DISPLAY");

	if (local_request == 0 && tmux_pid != 0)
	{
		log_debug("	Checking for remote clients attached to tmux before getting client tty...\n");
		if (pusb_tmux_has_remote_clients(user) != 0)
		{ // tmux has at least one remote client, can't be sure it isn't this one so denying...
			return 0;
		}

		char *tmux_client_tty = pusb_tmux_get_client_tty(tmux_pid);
		if (tmux_client_tty != NULL)
		{
			local_request = pusb_is_tty_local(tmux_client_tty);
			xfree(tmux_client_tty);
		}
		else
		{
			return 0;
		}
	}

	if (local_request == 0 && display_env != NULL)
	{
		char display[PUSB_DISPLAY_MAX];
		snprintf(display, sizeof(display), "%s", display_env);
		log_debug("	Using DISPLAY %s for utmp search\n", display);

		if (strstr(display, ".0") != NULL)
		{
			// DISPLAY contains not only display but also default screen, truncate screen part in this case
			log_debug("	DISPLAY contains screen, truncating...\n");
			memset(display + strlen(display) - 2, 0, 2);
		}

		local_request = pusb_is_tty_local(display);

		if (local_request == 0)
		{
			log_debug("	Trying to get tty from display server\n");
			char *display_server_tty = pusb_get_tty_from_display_server(display);

			if (display_server_tty != NULL)
			{
				log_debug("	Retrying with tty %s, obtained from display server, for utmp search\n", display_server_tty);
				local_request = pusb_is_tty_local(display_server_tty);
				xfree(display_server_tty);
			}
			else
			{
				log_debug("		Failed, no result while trying to get TTY from display server\n");
			}

			if (local_request == 0)
			{
				log_debug("	Trying to get tty by DISPLAY\n");
				char *xorg_tty = pusb_get_tty_by_xorg_display(display, user);

				if (xorg_tty != NULL)
				{
					log_debug("	Retrying with tty %s, obtained by DISPLAY, for utmp search\n", xorg_tty);
					local_request = pusb_is_tty_local(xorg_tty);
					xfree(xorg_tty);
				}
				else
				{
					log_debug("		Failed, no result while searching utmp for display %s owned by user %s\n", display, user);
				}
			}
		}
	}

	if (local_request == 0) 
	{
		struct stat sb;
		if (stat("/usr/bin/loginctl", &sb) != 0)
		{
			log_debug("	loginctl is not available, skipping checks using it\n");
		} 
		else 
		{
			log_debug("	Trying to check for remote access by loginctl\n");

			int loginctl_remote = pusb_is_loginctl_local();
			if (loginctl_remote == 1)
			{
				log_debug("	loginctl says this session is local\n");
				local_request = 1;
			}
			else if (loginctl_remote == -1)
			{
				log_debug("	loginctl says this session is remote\n");
				return 0;
			}
			else
			{
				log_debug("	Trying to get tty by loginctl\n");

				char *loginctl_tty = pusb_get_tty_by_loginctl();
				if (loginctl_tty != NULL)
				{
					log_debug("	Retrying with tty %s, obtained by loginctl, for utmp search\n", loginctl_tty);
					local_request = pusb_is_tty_local(loginctl_tty);
					xfree(loginctl_tty);
				}
				else
				{
					log_debug("		Failed, could not obtain tty from loginctl - see line before this for reason.\n");
				}
			}

			if (local_request == 0)
			{
				log_debug("	Checking if SDDM is active\n");
				if (pusb_is_sddm_active())
				{
					log_debug("	SDDM is active, trying to get tty by SDDM\n");
					char *sddm_tty = pusb_get_tty_by_sddm();
					if (sddm_tty != NULL)
					{
						log_debug("	Retrying with tty %s, obtained from SDDM session, for utmp search\n", sddm_tty);
						local_request = pusb_is_tty_local(sddm_tty);
						xfree(sddm_tty);
					}
					else
					{
						log_debug("		Failed, could not obtain tty from SDDM check.\n");
					}
				}
				else
				{
					log_debug("	SDDM is not active, skipping SDDM TTY check.\n");
				}
			}
		}
	}

	if (local_request == 0) 
	{
		session_tty = ttyname(STDIN_FILENO);
		if (!session_tty || !(*session_tty))
		{
			log_error("Couldn't retrieve login tty, assuming remote\n");
		} 
		else {
			log_debug("	Fallback: Using TTY %s from ttyname() for search\n", session_tty);
			local_request = pusb_is_tty_local((char *) session_tty);
		}
	}

	if (local_request == 1) 
	{
		log_debug("No remote access detected, seems to be local request - allowing.\n");
	} 
	else if (local_request == 0) 
	{
		log_debug("Couldn't confirm login tty to be neither local or remote - denying.\n");
	} 
	else if (local_request == -1) 
	{
		log_debug("Confirmed remote request - denying.\n");
	}

	return local_request;
}
