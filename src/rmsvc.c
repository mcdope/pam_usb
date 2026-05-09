/*
 * Copyright (c) 2026 Tobias Baeumer <tobiasbaeumer@gmail.com>
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
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <limits.h>
#include "conf.h"
#include "log.h"

/*
 * Returns 1 if raw_addr (as read from /proc/net/tcp in host byte order) is a
 * 127.x.x.x loopback address. Uses htonl so the comparison is correct on both
 * little-endian and big-endian machines.
 */
static int pusb_proc_tcp_is_loopback_v4(uint32_t raw_addr)
{
	return (htonl(raw_addr) >> 24) == 127;
}

/*
 * Returns 1 if hex32 (a 32-character hex string from /proc/net/tcp6) represents
 * the IPv6 loopback address ::1.
 *
 * On little-endian machines ::1 is printed as "00000000000000000000000001000000"
 * because each 32-bit group is stored/read in host byte order.
 * On big-endian it would be "00000000000000000000000000000001".
 * We check both representations.
 */
static int pusb_proc_tcp6_is_loopback(const char *hex32)
{
	if (strnlen(hex32, 33) < 32)
		return 0;
	/* little-endian representation of ::1 */
	if (strncmp(hex32, "00000000000000000000000001000000", 32) == 0) /* DevSkim: ignore */
		return 1;
	/* big-endian representation of ::1 */
	if (strncmp(hex32, "00000000000000000000000000000001", 32) == 0) /* DevSkim: ignore */
		return 1;
	return 0;
}

/*
 * Scans /proc/net/tcp (IPv4) for an ESTABLISHED connection with a non-loopback
 * remote address. If match_remote is 0, matches the local port; if 1, the remote.
 */
static int pusb_proc_tcp4_scan(const char *path, uint16_t port, int match_remote)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	char line[512];
	if (!fgets(line, sizeof(line), f)) {	/* skip header */
		fclose(f);
		return 0;
	}

	while (fgets(line, sizeof(line), f)) {
		unsigned int local_addr, rem_addr;
		unsigned int local_port, rem_port, state;

		if (sscanf(line, " %*d: %8x:%4x %8x:%4x %2x",
				   &local_addr, &local_port, &rem_addr, &rem_port, &state) < 5)
			continue;

		if (state != 0x01)
			continue;	/* not ESTABLISHED */
		if ((uint16_t)(match_remote ? rem_port : local_port) != port)
			continue;
		if (!pusb_proc_tcp_is_loopback_v4((uint32_t)rem_addr)) {
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	return 0;
}

static int pusb_proc_tcp4_has_established(const char *path, uint16_t port)
{
	return pusb_proc_tcp4_scan(path, port, 0);
}

static int pusb_proc_tcp4_has_established_to(const char *path, uint16_t port)
{
	return pusb_proc_tcp4_scan(path, port, 1);
}

/*
 * Scans /proc/net/tcp6 (IPv6) for an ESTABLISHED connection with a non-loopback
 * remote address. If match_remote is 0, matches the local port; if 1, the remote.
 */
static int pusb_proc_tcp6_scan(const char *path, uint16_t port, int match_remote)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	char line[512];
	if (!fgets(line, sizeof(line), f)) {	/* skip header */
		fclose(f);
		return 0;
	}

	while (fgets(line, sizeof(line), f)) {
		char local_str[80], rem_str[80];
		unsigned int state;

		if (sscanf(line, " %*d: %79s %79s %2x", local_str, rem_str, &state) < 3)
			continue;

		if (state != 0x01)
			continue;	/* not ESTABLISHED */

		/* port lives after ':' in local_str (incoming) or rem_str (outbound) */
		char *colon = strchr(match_remote ? rem_str : local_str, ':');
		if (!colon)
			continue;
		unsigned int parsed_port;
		if (sscanf(colon + 1, "%4x", &parsed_port) < 1)
			continue;
		if ((uint16_t)parsed_port != port)
			continue;

		char rem_addr[33];
		memcpy(rem_addr, rem_str, 32);
		rem_addr[32] = '\0';

		if (!pusb_proc_tcp6_is_loopback(rem_addr)) {
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	return 0;
}

static int pusb_proc_tcp6_has_established(const char *path, uint16_t port)
{
	return pusb_proc_tcp6_scan(path, port, 0);
}

static int pusb_proc_tcp6_has_established_to(const char *path, uint16_t port)
{
	return pusb_proc_tcp6_scan(path, port, 1);
}

/*
 * Returns 1 if a process whose cmdline contains 'name' exists under procfs_root.
 * Scans procfs_root/PID/cmdline for all numeric PID directories.
 */
static int pusb_process_name_exists(const char *procfs_root, const char *name)
{
	DIR *d = opendir(procfs_root);
	if (!d)
		return 0;

	char cmdline_path[PATH_MAX];
	char cmdline[4096];
	int found = 0;
	char *endptr;

	struct dirent *ent;
	while (!found && (ent = readdir(d)) != NULL) {
		if (strtol(ent->d_name, &endptr, 10) <= 0 || *endptr != '\0')
			continue;	/* skip non-PID entries */

		snprintf(cmdline_path, sizeof(cmdline_path), "%s/%s/cmdline",
				 procfs_root, ent->d_name);

		int fd = open(cmdline_path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		int bytes = read(fd, cmdline, sizeof(cmdline) - 1);
		close(fd);

		if (bytes <= 0)
			continue;
		cmdline[bytes] = '\0';

		/* Only match against argv[0] (stops at first NUL) to avoid false
		 * positives where the target name appears in a later argument
		 * (e.g. "sudo apt install ./anydesk.deb" matching "anydesk"). */
		const char *base = strrchr(cmdline, '/');
		base = base ? base + 1 : cmdline;
		if (strstr(base, name) != NULL)
			found = 1;
	}

	closedir(d);
	return found;
}

int pusb_has_active_remote_service(t_pusb_options *opts)
{
	(void)opts;

	log_debug("	Checking for known remote desktop processes...\n");

	/* Process-only services: their mere presence means a session is active */
	static const char *process_only[] = { "anydesk", NULL };
	for (int i = 0; process_only[i] != NULL; i++) {
		if (pusb_process_name_exists("/proc", process_only[i])) {
			log_debug("		Found remote service process: %s\n", process_only[i]);
			return 1;
		}
	}

	/* VNC-based services: process + external connection on port 5900 required */
	int vnc_external = (
		pusb_proc_tcp4_has_established("/proc/net/tcp",  5900) ||
		pusb_proc_tcp6_has_established("/proc/net/tcp6", 5900)
	);

	if (vnc_external) {
		log_debug("	Detected external VNC connection, checking for VNC server process...\n");
		static const char *vnc_processes[] = { "gnome-remote-de", "x11vnc", "Xvnc", NULL };
		for (int i = 0; vnc_processes[i] != NULL; i++) {
			if (pusb_process_name_exists("/proc", vnc_processes[i])) {
				log_debug("		Found VNC remote desktop service: %s\n", vnc_processes[i]);
				return 1;
			}
		}
	}

	/* RDP-based services: process + external connection on port 3389 required */
	int rdp_external = (
		pusb_proc_tcp4_has_established("/proc/net/tcp",  3389) ||
		pusb_proc_tcp6_has_established("/proc/net/tcp6", 3389)
	);

	if (rdp_external) {
		log_debug("	Detected external RDP connection, checking for RDP server process...\n");
		static const char *rdp_processes[] = { "gnome-remote-de", NULL };
		for (int i = 0; rdp_processes[i] != NULL; i++) {
			if (pusb_process_name_exists("/proc", rdp_processes[i])) {
				log_debug("		Found RDP remote desktop service: %s\n", rdp_processes[i]);
				return 1;
			}
		}
	}

	/* TeamViewer: deny only when actively connected to TV relay servers (remote port 5938) */
	int tv_connected = (
		pusb_proc_tcp4_has_established_to("/proc/net/tcp",  5938) ||
		pusb_proc_tcp6_has_established_to("/proc/net/tcp6", 5938)
	);

	if (tv_connected) {
		log_debug("	Detected active TeamViewer relay connection, checking for TV process...\n");
		static const char *tv_processes[] = { "TeamViewer_Desk", "teamviewerd", NULL };
		for (int i = 0; tv_processes[i] != NULL; i++) {
			if (pusb_process_name_exists("/proc", tv_processes[i])) {
				log_debug("		Found TeamViewer process: %s\n", tv_processes[i]);
				return 1;
			}
		}
	}

	return 0;
}
