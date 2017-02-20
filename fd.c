/*

  Copyright (c) 2016 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <fcntl.h>
#include <libdill.h>
#include <string.h>
#include <unistd.h>

#include "fd.h"
#include "iov.h"
#include "utils.h"

void fd_initrxbuf(struct fd_rxbuf *rxbuf) {
    dsock_assert(rxbuf);
    rxbuf->len = 0;
    rxbuf->pos = 0;
}

int fd_unblock(int s) {
    /* Switch to non-blocking mode. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    dsock_assert(rc == 0);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    dsock_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    dsock_assert (rc == 0 || errno == EINVAL);
#endif
    return 0;
}

int fd_connect(int s, const struct sockaddr *addr, socklen_t addrlen,
      int64_t deadline) {
    /* Initiate connect. */
    int rc = connect(s, addr, addrlen);
    if(rc == 0) return 0;
    if(dsock_slow(errno != EINPROGRESS)) return -1;
    /* Connect is in progress. Let's wait till it's done. */
    rc = fdout(s, deadline);
    if(dsock_slow(rc == -1)) return -1;
    /* Retrieve the error from the socket, if any. */
    int err = 0;
    socklen_t errsz = sizeof(err);
    rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
    if(dsock_slow(rc != 0)) return -1;
    if(dsock_slow(err != 0)) {errno = err; return -1;}
    return 0;
}

int fd_accept(int s, struct sockaddr *addr, socklen_t *addrlen,
      int64_t deadline) {
    int as;
    while(1) {
        /* Try to accept new connection synchronously. */
        as = accept(s, addr, addrlen);
        if(dsock_fast(as >= 0))
            break;
        /* If connection was aborted by the peer grab the next one. */
        if(dsock_slow(errno == ECONNABORTED)) continue;
        /* Propagate other errors to the caller. */
        if(dsock_slow(errno != EAGAIN && errno != EWOULDBLOCK)) return -1;
        /* Operation is in progress. Wait till new connection is available. */
        int rc = fdin(s, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
    int rc = fd_unblock(as);
    dsock_assert(rc == 0);
    return as;
}

int fd_send(int s, const struct iovec *iov, size_t iovlen, int64_t deadline) {
    if(dsock_slow(iovlen > 0 && !iov)) {errno = EINVAL; return -1;}
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    size_t len = iov_size(iov, iovlen);
    ssize_t sent = 0;
    while(sent < len) {
        struct iovec vec[iovlen];
        size_t veclen = iov_cut(vec, iov, iovlen, sent, len - sent);
        hdr.msg_iov = vec;
        hdr.msg_iovlen = veclen;
        ssize_t sz = sendmsg(s, &hdr, FD_NOSIGNAL);
        if(sz < 0) {
            if(dsock_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                if(errno == EPIPE) errno = ECONNRESET;
                return -1;
            }
            int rc = fdout(s, deadline);
            if(dsock_slow(rc < 0)) {
                if(errno == EPIPE) errno = ECONNRESET;
                return -1;
            }
            continue;
        }
        sent += sz;
    }
    return 0;
}

static ssize_t fdget(int s, struct iovec *iov, size_t iovlen, int block,
      int64_t deadline) {
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    size_t pos = 0;
    size_t len = iov_size(iov, iovlen);
    while(1) {
        struct iovec vec[iovlen];
        size_t veclen = iov_cut(vec, iov, iovlen, pos, len);
        hdr.msg_iov = vec;
        hdr.msg_iovlen = veclen;
        ssize_t sz = recvmsg(s, &hdr, 0);
        if(dsock_fast(sz == len)) return pos + sz;
        if(dsock_slow(sz == 0)) {errno = EPIPE; return -1;}
        if(dsock_slow(sz < 0 && errno != EWOULDBLOCK && errno != EAGAIN))
            return -1;
        if(dsock_fast(sz > 0)) {
            if(!block) return sz;
            pos += sz;
            len -= sz;
        }
        int rc = fdin(s, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
}

int fd_recv(int s, struct fd_rxbuf *rxbuf, const struct iovec *iov,
      size_t iovlen, int64_t deadline) {
    dsock_assert(rxbuf);
    dsock_assert(iovlen);
    struct iovec vec[iovlen];
    size_t pos = 0;
    size_t len = iov_size(iov, iovlen);
    while(1) {
        /* Use data from rxbuf. */
        size_t remaining = rxbuf->len - rxbuf->pos;
        size_t tocopy = remaining < len ? remaining : len;    
        iov_copyto(iov, iovlen, (char*)(rxbuf->data) + rxbuf->pos, pos, tocopy);
        rxbuf->pos += tocopy;
        pos += tocopy;
        len -= tocopy;
        if(!len) return 0;
        /* If requested amount of data is large avoid the copy
           and read it directly into user's buffer. */
        if(len >= sizeof(rxbuf->data)) {
            size_t veclen = iov_cut(vec, iov, iovlen, pos, len);
            ssize_t sz = fdget(s, vec, veclen, 1, deadline);
            if(dsock_slow(sz < 0)) return -1;
            return 0;
        }
        /* Read as much data as possible into rxbuf. */
        dsock_assert(rxbuf->len == rxbuf->pos);
        vec[0].iov_base = rxbuf->data;
        vec[0].iov_len = sizeof(rxbuf->data);
        ssize_t sz = fdget(s, vec, 1, 0, deadline);
        if(dsock_slow(sz < 0)) return -1;
        rxbuf->len = sz;
        rxbuf->pos = 0;
    }
}

int fd_close(int s) {
    fdclean(s);
    /* Discard any pending outbound data. If SO_LINGER option cannot
       be set, never mind and continue anyway. */
    struct linger lng;
    lng.l_onoff=1;
    lng.l_linger=0;
    setsockopt(s, SOL_SOCKET, SO_LINGER, (void*)&lng, sizeof(lng));
    return close(s);
}

