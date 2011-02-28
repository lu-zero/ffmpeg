/*
 * Unix protocol
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "avformat.h"
#include <unistd.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#if HAVE_POLL_H
#include <poll.h>
#endif
#include <sys/time.h>

#include <sys/un.h>

typedef struct UnixContext {
    int fd;
    struct sockaddr_un addr;
    int addr_len;
} UnixContext;

static int unix_open(URLContext *h, const char *filename, int flags)
{
    int fd = -1;
    UnixContext *s = NULL;
    int ret = 1;

    s = av_malloc(sizeof(UnixContext));
    if (!s) {
        return AVERROR(ENOMEM);
    }

    av_strstart(filename, "unix:", &filename);
    s->addr.sun_family = AF_UNIX;
    av_strlcpy(s->addr.sun_path, filename, sizeof(s->addr.sun_path));
    s->addr_len = sizeof(s->addr.sun_family) + strlen(s->addr.sun_path);

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (fd < 0) {
        av_free(s);
        return AVERROR(ENOMEM);
    }

    if (flags & URL_WRONLY)
        ret = connect(fd, (struct sockaddr *)&s->addr, s->addr_len);
    else
        ret = bind(fd, (struct sockaddr *)&s->addr, s->addr_len);

    if (ret < 0) {
        av_free(s);
        closesocket(fd);
        return AVERROR(EIO);
    }

    ff_socket_nonblock(fd, 1);

    h->priv_data = s;
    h->is_streamed = 1;
    s->fd = fd;

    return 0;
}

static int unix_wait_fd(int fd, int write)
{
    int ev = write ? POLLOUT : POLLIN;
    struct pollfd p = { .fd = fd, .events = ev, .revents = 0 };
    int ret;

    ret = poll(&p, 1, 100);
    return ret < 0 ? ff_neterrno() : p.revents & ev ? 0 : AVERROR(EAGAIN);
}

static int unix_read(URLContext *h, uint8_t *buf, int size)
{
    UnixContext *s = h->priv_data;
    int ret;

    if (!(h->flags & URL_FLAG_NONBLOCK)) {
        ret = unix_wait_fd(s->fd, 0);
        if (ret < 0)
            return ret;
    }
    ret = recv(s->fd, buf, size, 0);
    return ret < 0 ? ff_neterrno() : ret;
}

static int unix_write(URLContext *h, const uint8_t *buf, int size)
{
    UnixContext *s = h->priv_data;
    int ret;

    if (!(h->flags & URL_FLAG_NONBLOCK)) {
        ret = unix_wait_fd(s->fd, 1);
        if (ret < 0)
            return ret;
    }
    ret = send(s->fd, buf, size, 0);
    return ret < 0 ? ff_neterrno() : ret;
}

static int unix_close(URLContext *h)
{
    UnixContext *s = h->priv_data;
    closesocket(s->fd);
    if (h->flags & URL_RDONLY)
        unlink(s->addr.sun_path);
    av_free(s);
    return 0;
}

static int unix_get_file_handle(URLContext *h)
{
    UnixContext *s = h->priv_data;
    return s->fd;
}

URLProtocol ff_unix_protocol = {
    "unix",
    unix_open,
    unix_read,
    unix_write,
    NULL, /* seek */
    unix_close,
    .url_get_file_handle = unix_get_file_handle,
};
