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
#include <ctype.h>
#include <regex.h>
#include "log.h"
#include "process.h"
#include "mem.h"

/* Reject characters that could break out of shell double-quotes or inject commands. */
static int pusb_tmux_is_safe_socket_path(const char *path)
{
    if (!path || *path == '\0') return 0;
    for (const char *p = path; *p; p++) {
        switch (*p) {
            case '"': case '\'': case '`': case '$': case '\\':
            case '\n': case '\r': case '\t': case '!': case ';':
            case '&': case '|': case '<': case '>': case '(':
            case ')': case '{': case '}':
                return 0;
        }
    }
    return 1;
}

/* Client ID from TMUX must be purely numeric. */
static int pusb_tmux_is_numeric_id(const char *s)
{
    if (!s || *s == '\0') return 0;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) return 0;
    }
    return 1;
}

/* Escape ERE metacharacters in username so it matches literally in the regex. */
static void pusb_tmux_escape_for_regex(const char *src, char *dst, size_t dstlen)
{
    const char *metachar = "\\.^$*+?{}[]|()";
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < dstlen; i++) {
        if (strchr(metachar, (int)src[i])) {
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

char *pusb_tmux_get_client_tty(pid_t env_pid)
{
    char *tmux_details = getenv("TMUX");
    if (tmux_details == NULL)
    {
        log_debug("		No TMUX env var, checking parent process in case this is a sudo request\n");
        tmux_details = pusb_get_process_envvar(env_pid, "TMUX");

        if (tmux_details == NULL)
        {
            return NULL;
        }
    }

    char *tmux_client_id = strrchr(tmux_details, ',');
    if (tmux_client_id == NULL)
    {
        log_debug("		Malformed TMUX env var (no comma), cannot get client id\n");
        return NULL;
    }
    tmux_client_id++; // ... to strip leading comma
    log_debug("		Got tmux_client_id: %s\n", tmux_client_id);

    char *tmux_socket_path = strtok(tmux_details, ",");
    log_debug("		Got tmux_socket_path: %s\n", tmux_socket_path);

    if (!pusb_tmux_is_safe_socket_path(tmux_socket_path))
    {
        log_error("TMUX socket path contains invalid characters, denying.\n");
        return NULL;
    }

    if (!pusb_tmux_is_numeric_id(tmux_client_id))
    {
        log_error("TMUX client ID is not numeric, denying.\n");
        return NULL;
    }

    size_t get_tmux_session_details_cmd_len = strlen(tmux_socket_path) + strlen(tmux_client_id) + 64;
    char *get_tmux_session_details_cmd = xmalloc(get_tmux_session_details_cmd_len);
    snprintf(get_tmux_session_details_cmd, get_tmux_session_details_cmd_len, "LC_ALL=C; tmux -S \"%s\" list-clients -t \"\\$%s\"", tmux_socket_path, tmux_client_id);
    log_debug("		Built get_tmux_session_details_cmd: %s\n", get_tmux_session_details_cmd);

    char buf[BUFSIZ];
    FILE *fp;
    if ((fp = popen(get_tmux_session_details_cmd, "r")) == NULL)
    {
        log_error("tmux detected, but couldn't get session details. Denying since remote check impossible without it!\n");
        xfree(get_tmux_session_details_cmd);
        return NULL;
    }
    xfree(get_tmux_session_details_cmd);

    char *tmux_client_tty = NULL;
    if (fgets(buf, BUFSIZ, fp) != NULL)
    {
        tmux_client_tty = strtok(buf, ":");
        if (tmux_client_tty == NULL)
        {
            log_error("tmux detected, but couldn't parse client tty. Denying.\n");
            pclose(fp);
            return NULL;
        }
        if (strncmp(tmux_client_tty, "/dev/", 5) != 0)
        {
            log_error("tmux detected, but client tty is not under /dev. Denying.\n");
            pclose(fp);
            return NULL;
        }
        tmux_client_tty += 5; // cut "/dev/"
        log_debug("		Got tmux_client_tty: %s\n", tmux_client_tty);

        if (pclose(fp))
        {
            log_debug("		Closing pipe for 'tmux list-clients' failed, this is quite a wtf...\n");
        }

        size_t tty_len = strlen(tmux_client_tty);
        char *result = xmalloc(tty_len + 1);
        memcpy(result, tmux_client_tty, tty_len + 1);
        return result;
    }
    else
    {
        log_error("tmux detected, but couldn't get client details. Denying since remote check impossible without it!\n");
        pclose(fp);
        return NULL;
    }
}

/**
 * @todo: Github user @fuseteam will still find ways to circumvent this, guess this will need
 *        refactoring into a generic check with variable command name to also cover other
 *        multiplexers
 */
int pusb_tmux_has_remote_clients(const char* username)
{
    int status;
    FILE *fp;
    regex_t regex;
    char regex_raw[BUFSIZ];
    char buf[BUFSIZ];
    char msgbuf[100];
    const char *regex_tpl[2] = {
        "(.+)([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})(.+)tmux(.+)", //v4
        "(.+)([0-9A-Fa-f]{1,4}):([0-9A-Fa-f]{1,4}):([0-9A-Fa-f]{1,4}):([0-9A-Fa-f]{1,4})(.+)tmux(.+)" // v6
    }; // ... yes, these allow invalid addresses. No, I don't care. This isn't about validation but detecting remote access. Good enough ¯\_(ツ)_/¯

    /* Max Linux username is 32 chars; doubled for escaping + null terminator = 65 bytes. */
    char escaped_username[65] = {0};
    pusb_tmux_escape_for_regex(username, escaped_username, sizeof(escaped_username));

    for (int i = 0; i <= 1; i++)
    {
        log_debug("		Checking for IPv%d connections...\n", (4 + (i * 2)));

        if ((fp = popen("LC_ALL=C; /usr/bin/w", "r")) == NULL)
        {
            log_error("tmux detected, but couldn't get `w`. Denying since remote check for tmux impossible without it!\n");
            return -1;
        }

        while (fgets(buf, BUFSIZ, fp) != NULL)
        {
            snprintf(regex_raw, BUFSIZ, "%s%s", escaped_username, regex_tpl[i]);

            status = regcomp(&regex, regex_raw, REG_EXTENDED);
            if (status)
            {
                log_debug("		Couldn't compile regex!\n");
                regfree(&regex);
                pclose(fp);
                return -1;
            }

            status = regexec(&regex, buf, 0, NULL, 0);
            if (!status)
            {
                log_error("tmux detected and at least one remote client is connected to the session, denying!\n");
                regfree(&regex);
                pclose(fp);
                return 1;
            }
            else if (status != REG_NOMATCH)
            {
                regerror(status, &regex, msgbuf, sizeof(msgbuf));
                log_debug("		Regex match failed: %s\n", msgbuf);
                regfree(&regex);
                pclose(fp);
                return -1;
            }

            regfree(&regex);
        }

        if (pclose(fp))
        {
            log_debug("		Closing pipe for 'w' failed, this is quite a wtf...\n");
        }
    }

    // If we had detected a remote access we would have returned by now. Safe to return 0 now
    return 0;
}
