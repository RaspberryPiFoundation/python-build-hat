/* dummy_i2c.c
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Limited
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *     Copyright (c) 2020 Kynesim Ltd
 *     Copyright (c) 2017-2020 LEGO System A/S
 *
 * Dummy I2C device, actually an ethernet local loopback
 */


#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>


int open_dummy_i2c_socket(const char **perrstr)
{
    struct addrinfo hints;
    struct addrinfo *info;
    int rv;
    int fd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo("127.0.0.1", "2020", &hints, &info)) != 0)
    {
        *perrstr = gai_strerror(rv);
        return -1;
    }

    fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd == -1)
    {
        *perrstr = strerror(errno);
        freeaddrinfo(info);
        return -1;
    }
    if (connect(fd, info->ai_addr, info->ai_addrlen) < 0)
    {
        *perrstr = strerror(errno);
        freeaddrinfo(info);
        return -1;
    }

    freeaddrinfo(info);
    return fd;
}


int dummy_i2c_read(int fd, uint8_t *buffer, size_t nbytes)
{
    struct pollfd poll_data;
    int rv;
    ssize_t nread;

    /* Check for anything to read */
    poll_data.fd = fd;
    poll_data.events = POLLIN;
    if ((rv = poll(&poll_data, 1, 0)) < 0)
        return -1;
    if (rv == 0)
    {
        *buffer = 0;
        return 1;
    }

    rv = nbytes;
    while (nbytes > 0)
    {
        nread = read(fd, buffer, nbytes);
        if (nread < 0)
            return -1;
        buffer += nread;
        nbytes -= nread;
    }

    return rv;
}


int dummy_i2c_write(int fd, uint8_t *buffer, size_t nbytes)
{
    ssize_t nwritten;
    int rv = nbytes;

    while (nbytes > 0)
    {
        nwritten = write(fd, buffer, nbytes);
        if (nwritten < 0)
            return -1;
        buffer += nwritten;
        nbytes -= nwritten;
    }

    return rv;
}
