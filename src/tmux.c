/*
 * Copyright (c) 2021 Tobias Bäumer <tobiasbaeumer@gmail.com>
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
#include "log.h"

char* tmux_get_client_tty()
{
    const char *tmux_details = getenv("TMUX");
    if (tmux_details == NULL) {
        log_error("tmux detected, but TMUX env var missing. Denying since remote check impossible without it!\n");
        return (0);
    }

    char *tmux_client_id = strrchr(tmux_details, ',');
    tmux_client_id++; // ... to strip leading comma
    log_debug("		Got tmux_client_id: %s\n", tmux_client_id);

    char get_tmux_session_details_cmd[32];
    sprintf(get_tmux_session_details_cmd, "tmux list-clients -t %s", tmux_client_id);
    log_debug("		Built get_tmux_session_details_cmd: %s\n", get_tmux_session_details_cmd);

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
        log_debug("		Got tmux_client_tty: %s\n", tmux_client_tty);
    }

    if (pclose(fp)) {
        log_debug("		Closing pipe for 'tmux list-clients' failed, this is quite a wtf...");
    }

    return tmux_client_tty;
}