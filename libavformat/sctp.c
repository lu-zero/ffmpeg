/*
 * SCTP protocol
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
#include "avformat.h"
#include <unistd.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#if HAVE_POLL_H
#include <poll.h>
#endif
#include <sys/time.h>
#include <netinet/sctp.h>

typedef struct SCTPContext {
    int fd;
} SCTPContext;

/* return non zero if error */
static int sctp_open(URLContext *h, const char *uri, int flags)
{
    struct addrinfo hints, *ai, *cur_ai;
    struct sctp_event_subscribe event;
    int port, fd = -1;
    SCTPContext *s = NULL;
    int ret;
    socklen_t optlen;
    char hostname[1024],proto[1024],path[1024];
    char portstr[10];

    av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
        &port, path, sizeof(path), uri);
    if (strcmp(proto,"sctp") || port <= 0 || port >= 65536)
        return AVERROR(EINVAL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);
    ret = getaddrinfo(hostname, portstr, &hints, &ai);
    if (ret) {
        av_log(NULL, AV_LOG_ERROR,
               "Failed to resolve hostname %s: %s\n",
               hostname, gai_strerror(ret));
        return AVERROR(EIO);
    }

    cur_ai = ai;

    fd = socket(cur_ai->ai_family, SOCK_STREAM, IPPROTO_SCTP);
    if (fd < 0)
        goto fail;
    ff_socket_nonblock(fd, 1);

    if(flags & URL_WRONLY) {
        ret = connect(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
    } else {
        ret = bind(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
        listen(fd, 100);
        fd = accept(fd, NULL, NULL);
    }

    event.sctp_data_io_event = 1;
    event.sctp_association_event = 1;
    event.sctp_address_event = 1;
    event.sctp_send_failure_event = 1;
    event.sctp_peer_error_event = 1;
    event.sctp_shutdown_event = 1;
    event.sctp_partial_delivery_event = 1;
    event.sctp_adaptation_layer_event = 1;

    if (setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &event,
                   sizeof(event)) != 0) {
        //XXX
    }

    s = av_malloc(sizeof(SCTPContext));

    if (!s) {
        freeaddrinfo(ai);
        return AVERROR(ENOMEM);
    }

    h->priv_data = s;
    h->is_streamed = 1;
    s->fd = fd;
    freeaddrinfo(ai);
    return 0;

fail:
    ret = AVERROR(EIO);
    freeaddrinfo(ai);
    return ret;
}

static int sctp_wait_fd(int fd, int write)
{
    int ev = write ? POLLOUT : POLLIN;
    struct pollfd p = { .fd = fd, .events = ev, .revents = 0 };
    int ret;

    av_log(NULL, AV_LOG_INFO, "POLL\n");
    ret = poll(&p, 1, 100);
    av_log(NULL, AV_LOG_INFO, "POLL res %d\n", ret);
    return ret < 0 ? ff_neterrno() : p.revents & ev ? 0 : AVERROR(EAGAIN);
}

static int sctp_read(URLContext *h, uint8_t *buf, int size)
{
    SCTPContext *s = h->priv_data;
    int ret;

    if (!(h->flags & URL_FLAG_NONBLOCK)) {
        ret = sctp_wait_fd(s->fd, 0);
        if (ret < 0)
            return ret;
    }

    av_log(NULL, AV_LOG_INFO, "READ\n");
    ret = recv(s->fd, buf, size, 0);
    av_log(NULL, AV_LOG_INFO, "READ res %d\n", ret);
    return ret < 0 ? ff_neterrno() : ret;
}

static int sctp_write(URLContext *h, const uint8_t *buf, int size)
{
    SCTPContext *s = h->priv_data;
    int ret;

    if (!(h->flags & URL_FLAG_NONBLOCK)) {
        ret = sctp_wait_fd(s->fd, 1);
        if (ret < 0)
            return ret;
    }
    av_log(NULL, AV_LOG_INFO, "WRITE\n");
    ret = send(s->fd, buf, size, 0);
    av_log(NULL, AV_LOG_INFO, "WRITE res %d\n", ret);
    return ret < 0 ? ff_neterrno() : ret;
}

static int sctp_close(URLContext *h)
{
    SCTPContext *s = h->priv_data;
    closesocket(s->fd);
    av_free(s);
    return 0;
}

static int sctp_get_file_handle(URLContext *h)
{
    SCTPContext *s = h->priv_data;
    return s->fd;
}

URLProtocol ff_sctp_protocol = {
    "sctp",
    sctp_open,
    sctp_read,
    sctp_write,
    NULL, /* seek */
    sctp_close,
    .url_get_file_handle = sctp_get_file_handle,
};
