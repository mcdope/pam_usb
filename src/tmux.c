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

#define _GNU_SOURCE
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
    char *tmux_details_raw = secure_getenv("TMUX");
    int from_env = (tmux_details_raw != NULL);
    if (!from_env)
    {
        log_debug("		No TMUX env var, checking parent process in case this is a sudo request\n");
        tmux_details_raw = pusb_get_process_envvar(env_pid, "TMUX");
        if (tmux_details_raw == NULL)
        {
            return NULL;
        }
    }

    /* Always work on a private copy: strtok_r would otherwise corrupt the live
     * environment block (if from getenv) or leak the pusb_get_process_envvar
     * allocation (if from the process environ scan). */
    char *tmux_details = xstrdup(tmux_details_raw);
    if (!from_env)
        xfree(tmux_details_raw);

    char *tmux_client_id = strrchr(tmux_details, ',');
    if (tmux_client_id == NULL)
    {
        log_debug("		Malformed TMUX env var (no comma), cannot get client id\n");
        xfree(tmux_details);
        return NULL;
    }
    tmux_client_id++; // ... to strip leading comma
    log_debug("		Got tmux_client_id: %s\n", tmux_client_id);

    char *saveptr = NULL;
    char *tmux_socket_path = strtok_r(tmux_details, ",", &saveptr);
    log_debug("		Got tmux_socket_path: %s\n", tmux_socket_path);

    if (!pusb_tmux_is_safe_socket_path(tmux_socket_path))
    {
        log_error("TMUX socket path contains invalid characters, denying.\n");
        xfree(tmux_details);
        return NULL;
    }

    if (!pusb_tmux_is_numeric_id(tmux_client_id))
    {
        log_error("TMUX client ID is not numeric, denying.\n");
        xfree(tmux_details);
        return NULL;
    }

    size_t get_tmux_session_details_cmd_len =
        strlen("LC_ALL=C; /usr/bin/tmux -S \"\" list-clients -t \"\\$\"")
        + strlen(tmux_socket_path)
        + strlen(tmux_client_id)
        + 1;
    char *get_tmux_session_details_cmd = xmalloc(get_tmux_session_details_cmd_len);
    snprintf(get_tmux_session_details_cmd, get_tmux_session_details_cmd_len, "LC_ALL=C; /usr/bin/tmux -S \"%s\" list-clients -t \"\\$%s\"", tmux_socket_path, tmux_client_id);
    log_debug("		Built get_tmux_session_details_cmd: %s\n", get_tmux_session_details_cmd);

    char buf[BUFSIZ];
    FILE *fp;
    if ((fp = popen(get_tmux_session_details_cmd, "r")) == NULL)
    {
        log_error("tmux detected, but couldn't get session details. Denying since remote check impossible without it!\n");
        xfree(get_tmux_session_details_cmd);
        xfree(tmux_details);
        return NULL;
    }
    xfree(get_tmux_session_details_cmd);

    char *tmux_client_tty = NULL;
    char *result = NULL;
    if (fgets(buf, BUFSIZ, fp) != NULL)
    {
        char *strtok_buf_save = NULL;
        tmux_client_tty = strtok_r(buf, ":", &strtok_buf_save);
        if (tmux_client_tty == NULL)
        {
            log_error("tmux detected, but couldn't parse client tty. Denying.\n");
            pclose(fp);
            xfree(tmux_details);
            return NULL;
        }
        if (strncmp(tmux_client_tty, "/dev/", 5) != 0)
        {
            log_error("tmux detected, but client tty has unexpected format: %s. Denying.\n", tmux_client_tty);
            pclose(fp);
            xfree(tmux_details);
            return NULL;
        }
        tmux_client_tty += 5; /* strip /dev/ prefix */
        log_debug("		Got tmux_client_tty: %s\n", tmux_client_tty);

        if (pclose(fp) == -1)
        {
            log_debug("		Closing pipe for 'tmux list-clients' failed, this is quite a wtf...\n");
        }

        result = xstrdup(tmux_client_tty);
    }
    else
    {
        log_error("tmux detected, but couldn't get client details. Denying since remote check impossible without it!\n");
        pclose(fp);
    }

    xfree(tmux_details);
    return result;
}

/**
 * @todo: Github user @fuseteam will still find ways to circumvent this, guess this will need
 *        refactoring into a generic check with variable command name to also cover other
 *        multiplexers
 */
int pusb_tmux_has_remote_clients(const char* username)
{
    if (username == NULL)
    {
        log_error("Username is NULL, cannot check for remote tmux clients.\n");
        return -1;
    }

    if (strlen(username) > 255) /* DevSkim: ignore DS140021 - username is a PAM-provided NUL-terminated string */
    {
        log_error("Username exceeds 255 characters, denying to prevent regex bypass.\n");
        return -1;
    }

    int status;
    int n;
    int i;
    FILE *fp;
    regex_t regex[3];
    char regex_raw[BUFSIZ];
    char buf[BUFSIZ];
    char msgbuf[100];
    int result = 0;
    const char *regex_tpl[3] = {
        "([[:space:]]+)([^[:space:]]+)([[:space:]]+)([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})([[:space:]]+)(.+)tmux(.+)", //v4: anchored to FROM field; prevents username-prefix false positives
        "([[:space:]]+)([^[:space:]]+)([[:space:]]+)(([0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4})[^[:space:]]*([[:space:]]+)(.+)tmux(.+)", // v6: anchored to FROM field; [^[:space:]]* absorbs zone index (e.g. %eth0)
        "([[:space:]]+)([^[:space:]]+)([[:space:]]+)::ffff:([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})([[:space:]]+)(.+)tmux(.+)" // v4-mapped v6 (::ffff:x.x.x.x): bypasses both above patterns without this
    }; // ... yes, these allow invalid addresses. No, I don't care. This isn't about validation but detecting remote access. Good enough ¯\_(ツ)_/¯

    /* Up to 255-char usernames (SSSD/AD); doubled for escaping + null terminator = 511, rounded to 512. */
    char escaped_username[512] = {0};
    pusb_tmux_escape_for_regex(username, escaped_username, sizeof(escaped_username));

    for (i = 0; i < 3; i++)
    {
        n = snprintf(regex_raw, BUFSIZ, "^%s%s", escaped_username, regex_tpl[i]);
        if (n < 0 || (size_t)n >= BUFSIZ)
        {
            log_debug("		regex_raw buffer overflow for pattern %d.\n", i);
            for (int j = 0; j < i; j++) regfree(&regex[j]);
            return -1;
        }

        status = regcomp(&regex[i], regex_raw, REG_EXTENDED | REG_NOSUB);
        if (status)
        {
            log_debug("		Couldn't compile regex %d!\n", i);
            for (int j = 0; j < i; j++) regfree(&regex[j]);
            return -1;
        }
    }

    if ((fp = popen("LC_ALL=C; /usr/bin/w -i", "re")) == NULL)
    {
        log_error("tmux detected, but couldn't get `w`. Denying since remote check for tmux impossible without it!\n");
        for (i = 0; i < 3; i++) regfree(&regex[i]);
        return -1;
    }

    while (fgets(buf, BUFSIZ, fp) != NULL)
    {
        for (i = 0; i < 3; i++)
        {
            status = regexec(&regex[i], buf, 0, NULL, 0);
            if (!status)
            {
                log_error("tmux detected and at least one remote client is connected to the session, denying!\n");
                result = 1;
                break;
            }
            else if (status != REG_NOMATCH)
            {
                regerror(status, &regex[i], msgbuf, sizeof(msgbuf));
                log_debug("		Regex match failed: %s\n", msgbuf);
                result = -1;
                break;
            }
        }
        if (result != 0)
            break;
    }

    for (i = 0; i < 3; i++) regfree(&regex[i]);

    if (pclose(fp) == -1)
    {
        log_debug("		Closing pipe for 'w' failed, this is quite a wtf...\n");
    }

    return result;
}
