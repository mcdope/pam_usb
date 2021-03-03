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
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "log.h"

#define XDMCP_PORT 177

static const int UDP_CONNECT_TIMEOUT = 250 * 100000;

/**
 * Connects to localhost:177 (UDP) to check if XDMCP is enabled
 * 
 * Based on https://www.geeksforgeeks.org/udp-server-client-implementation-c/
 * 
 * @see https://www.x.org/releases/X11R7.7/doc/libXdmcp/xdmcp.html
 **/
int is_xdmcp_enabled() {
    int sockfd;
    char buffer[BUFSIZ];
    char *content = "QUERY";
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        log_debug("UDP socket socket creation failed!\n");
        return 0;
    }

    // Set timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = UDP_CONNECT_TIMEOUT;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        log_debug("Setting UDP socket timeout failed!\n");
    }

    // Fill connect information
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(XDMCP_PORT);
    servaddr.sin_addr.s_addr = INADDR_LOOPBACK;

    // Send QUERY packet, if there is a server - it will respond
    int n;
    socklen_t len;

    sendto(
        sockfd,
        (const char *)content,
        strlen(content),
        MSG_CONFIRM,
        (const struct sockaddr *) &servaddr,
        sizeof(servaddr)
    );

    // Wait till we got a response (or timeout kicks in)
    n = recvfrom(
        sockfd,
        (char *)buffer,
        BUFSIZ,
        MSG_WAITALL,
        (struct sockaddr *) &servaddr,
        &len
    );
    buffer[n] = '\0';
    close(sockfd);

    if (n > 0) {
        log_debug("Server response (%d bytes): %s\n", n, buffer);
        return 1;
    }

    log_debug("No server response\n");
    return 0;
}