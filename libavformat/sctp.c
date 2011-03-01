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
#include "libavutil/parseutils.h"
#include <unistd.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#include "libavutil/intreadwrite.h"
#if HAVE_POLL_H
#include <poll.h>
#endif
#include <sys/time.h>
#include <netinet/sctp.h>

/*
 * The sctp_recvmsg and sctp_sendmsg functions are part of the user
 * library that offers support
 * for the SCTP kernel Implementation. The main purpose of this
 * code is to provide the SCTP Socket API mappings for user
 * application to interface with the SCTP in kernel.
 *
 * This implementation is based on the Socket API Extensions for SCTP
 * defined in <draft-ietf-tsvwg-sctpsocket-10.txt>
 *
 * Copyright (c) 2003 International Business Machines, Corp.
 *
 * Written or modified by:
 *  Ryan Layer <rmlayer@us.ibm.com>
 *
 */

static
int ff_sctp_recvmsg(int s, void *msg, size_t len, struct sockaddr *from,
                    socklen_t *fromlen, struct sctp_sndrcvinfo *sinfo,
                    int *msg_flags)
{
    int error;
    struct iovec iov;
    struct msghdr inmsg;
    char incmsg[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];
    struct cmsghdr *cmsg = NULL;

    memset(&inmsg, 0, sizeof (inmsg));

    iov.iov_base = msg;
    iov.iov_len = len;

    inmsg.msg_name = from;
    inmsg.msg_namelen = fromlen ? *fromlen : 0;
    inmsg.msg_iov = &iov;
    inmsg.msg_iovlen = 1;
    inmsg.msg_control = incmsg;
    inmsg.msg_controllen = sizeof(incmsg);

    error = recvmsg(s, &inmsg, msg_flags ? *msg_flags : 0);
    if (error < 0)
        return error;

    if (fromlen)
        *fromlen = inmsg.msg_namelen;
    if (msg_flags)
        *msg_flags = inmsg.msg_flags;

    if (!sinfo)
        return error;

    for (cmsg = CMSG_FIRSTHDR(&inmsg); cmsg != NULL;
                 cmsg = CMSG_NXTHDR(&inmsg, cmsg)){
        if ((IPPROTO_SCTP == cmsg->cmsg_level) &&
            (SCTP_SNDRCV == cmsg->cmsg_type))
            break;
    }

    /* Copy sinfo. */
    if (cmsg)
        memcpy(sinfo, CMSG_DATA(cmsg), sizeof(struct sctp_sndrcvinfo));

    return (error);
}

static
int ff_sctp_send(int s, const void *msg, size_t len,
                 const struct sctp_sndrcvinfo *sinfo, int flags)
{
    struct msghdr outmsg;
    struct iovec iov;

    outmsg.msg_name = NULL;
    outmsg.msg_namelen = 0;
    outmsg.msg_iov = &iov;
    iov.iov_base = (void *)msg;
    iov.iov_len = len;
    outmsg.msg_iovlen = 1;
    outmsg.msg_controllen = 0;

    if (sinfo) {
        char outcmsg[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];
        struct cmsghdr *cmsg;

        outmsg.msg_control = outcmsg;
        outmsg.msg_controllen = sizeof(outcmsg);
        outmsg.msg_flags = 0;

        cmsg = CMSG_FIRSTHDR(&outmsg);
        cmsg->cmsg_level = IPPROTO_SCTP;
        cmsg->cmsg_type = SCTP_SNDRCV;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));

        outmsg.msg_controllen = cmsg->cmsg_len;
        memcpy(CMSG_DATA(cmsg), sinfo, sizeof(struct sctp_sndrcvinfo));
    }

    return sendmsg(s, &outmsg, flags);
}

typedef struct SCTPContext {
    int fd;
    int max_streams;
    struct sockaddr_storage dest_addr;
    socklen_t dest_addr_len;
} SCTPContext;

/**
 * the sctp url_protocol
 *
 * url syntax: sctp://host:port[?option=val...]
 * option: 'listen'        : listen for an incoming connection
 *         'max_streams=n' : set the maximum number of streams
 *         'reuse=1'       : enable reusing the socket [TBD]
 *
 * by setting the maximum number of streams the protocol will use the
 * first two bytes of the incoming/outgoing buffer to store the
 * stream number of the packet being read/written.
 * @see sctp_read
 * @see sctp_write
 *
 */
static int sctp_open(URLContext *h, const char *uri, int flags)
{
    struct addrinfo hints, *ai, *cur_ai;
    struct sctp_event_subscribe event = {0};
    struct sctp_initmsg initparams = {0};
    int port, fd = -1;
    SCTPContext *s = NULL;
    const char *p;
    char buf[256];
    int ret, listen_socket = 0;
    socklen_t optlen;
    char hostname[1024],proto[1024],path[1024];
    char portstr[10];

    av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
        &port, path, sizeof(path), uri);
    if (strcmp(proto,"sctp") || port <= 0 || port >= 65536)
        return AVERROR(EINVAL);

    s = av_malloc(sizeof(SCTPContext));
    if (!s) {
        return AVERROR(ENOMEM);
    }
    s->max_streams = 0;
    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "listen", p)) {
            listen_socket = 1;
        }
        if (av_find_info_tag(buf, sizeof(buf), "max_streams", p)) {
            s->max_streams = strtol(buf, NULL, 10);
        }
    }

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

    s->dest_addr_len = sizeof(s->dest_addr);

    if(listen_socket) {
        //int fd1;
        ret = bind(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
        listen(fd, 100);
        fd = accept(fd, NULL, NULL);
        av_log(NULL, AV_LOG_INFO, "Accepting %d\n", fd);
        //closesocket(fd);
        //fd = fd1;
    } else {
        ret = connect(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
    }

    ff_socket_nonblock(fd, 1);

    event.sctp_data_io_event = 1;
/*    event.sctp_association_event = 1;
    event.sctp_address_event = 1;
    event.sctp_send_failure_event = 1;
    event.sctp_peer_error_event = 1;
    event.sctp_shutdown_event = 1;
    event.sctp_partial_delivery_event = 1;
    event.sctp_adaptation_layer_event = 1; */

    if (setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &event,
                   sizeof(event)) != 0) {
        //XXX
    }

    if (s->max_streams) {
        initparams.sinit_max_instreams = s->max_streams;
        initparams.sinit_num_ostreams = s->max_streams;
        if (setsockopt(fd, SOL_SCTP, SCTP_INITMSG, &initparams,
                           sizeof(initparams)) < 0) {
            //XXX
        }
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

    ret = poll(&p, 1, 100);
//    av_log(NULL, AV_LOG_INFO, "Polling %d %s\n", ret, strerror(errno));
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
    if (s->max_streams) {
        struct sctp_sndrcvinfo info = {0};
        ret = ff_sctp_recvmsg(s->fd, buf+2, size-2, NULL, 0, &info, 0);
//        av_log(NULL, AV_LOG_INFO, "Stream %d \n", info.sinfo_stream);
        AV_WB16(buf, info.sinfo_stream);
    } else {
        ret = recv(s->fd, buf, size, 0);
    }
//    av_log(NULL, AV_LOG_INFO, "Receiving %d %s\n", ret, strerror(errno));
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

    if (s->max_streams) {
        struct sctp_sndrcvinfo info = {0};
        info.sinfo_stream = AV_RB16(buf);
        ret = ff_sctp_send(s->fd, buf+2, size-2, &info, 0);
    } else {
        ret = send(s->fd, buf, size, 0);
    }
//    av_log(NULL, AV_LOG_INFO, "Writing res %d %s\n", ret, strerror(errno));
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
