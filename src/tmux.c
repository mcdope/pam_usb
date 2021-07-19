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
#include <stdlib.h>,
#include <regex.h>
#include "log.h"
#include "process.h"

char *pusb_tmux_get_client_tty(pid_t env_pid)
{
    char *tmux_details = getenv("TMUX");
    if (tmux_details == NULL) {
        log_debug("		No TMUX env var, checking parent process in case this is a sudo request\n");

        tmux_details = (char *)malloc(BUFSIZ);
        tmux_details = pusb_get_process_envvar(env_pid, "TMUX");

        if (tmux_details == NULL) {
            return NULL;
        }
    }

    char *tmux_client_id = strrchr(tmux_details, ',');
    tmux_client_id++; // ... to strip leading comma
    log_debug("		Got tmux_client_id: %s\n", tmux_client_id);

    char *tmux_socket_path = strtok(tmux_details, ",");
    log_debug("		Got tmux_socket_path: %s\n", tmux_socket_path);

    char get_tmux_session_details_cmd[64];
    sprintf(get_tmux_session_details_cmd, "tmux -S %s list-clients -t %s", tmux_socket_path, tmux_client_id);
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

        if (pclose(fp)) {
            log_debug("		Closing pipe for 'tmux list-clients' failed, this is quite a wtf...\n");
        }

        return tmux_client_tty;
    } else {
        log_error("tmux detected, but couldn't get client details. Denying since remote check impossible without it!\n");
        return (0);
    }
}

int pusb_tmux_has_remote_clients(char* username)
{
    char buf[BUFSIZ];
    FILE *fp;
    if ((fp = popen("w", "r")) == NULL) {
        log_error("tmux detected, but couldn't get `w`. Denying since remote check for tmux impossible without it!\n");
        return (-1);
    }

    regex_t regex;
    char regex_raw[BUFSIZ];
    int status;
    char msgbuf[100];
    char expr[2][BUFSIZ] = {
        "(.+)([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})(.+)tmux a", //v4
        "(.+)([0-9A-F]{1,4}):([0-9A-F]{1,4}):([0-9A-F]{1,4}):([0-9A-F]{1,4})(.+)tmux a" // v6
    }; // ... yes, these allow invalid addresses. No, I don't care. This isn't about validation but detecting remote access. Good enough ¯\_(ツ)_/¯

    for (int i = 0; i <= 1; i++) {
        while (fgets(buf, BUFSIZ, fp) != NULL) {
            log_debug("Checking w line: %s", buf);
            sprintf(regex_raw, "%s%s", username, expr[i]);
            log_debug("Current regex: %s\n", regex_raw);

            /* Compile regular expression */
            status = regcomp(&regex, regex_raw, REG_EXTENDED);
            if (status) {
                log_debug("Couldn't compile regex!\n");
                regfree(&regex);
                return (-1);
            }

            /* Execute regular expression */
            status = regexec(&regex, buf, 0, NULL, 0);
            if (!status) {
                log_error("tmux detected and at least one remote client is connected to the session, denying!\n");
                regfree(&regex);
                return 1;
            }
            else if (status == REG_NOMATCH) {
                log_debug("		who line checked, no match (this is good :P)\n");
            }
            else {
                regerror(status, &regex, msgbuf, sizeof(msgbuf));
                log_debug("wtf - regex match failed: %s\n", msgbuf);
                regfree(&regex);
                return -1;
            }

            regfree(&regex);
        }

        rewind(fp); //@todo this doesnt work for pipe streams, need to re-open the pipe
    }

    if (pclose(fp)) {
        log_debug("		Closing pipe for 'who' failed, this is quite a wtf...\n");
    }

    // If we would have detected a remote access we would have returned by now. Safe to return 0 now
    return 0;
}