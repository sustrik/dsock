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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsock.h"
#include "dsock.h"
#include "iov.h"
#include "msock.h"
#include "utils.h"

static const int pfx_type_placeholder = 0;
static const void *pfx_type = &pfx_type_placeholder;
static void pfx_close(int s);
static int pfx_msendv(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);
static ssize_t pfx_mrecvv(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);

#define pfx_sock_PEERDONE 1

struct pfx_sock {
    struct msock_vfptrs vfptrs;
    int s;
    int flags;
};

int pfx_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, bsock_type))) return -1;
    /* Create the object. */
    struct pfx_sock *obj = malloc(sizeof(struct pfx_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->vfptrs.hvfptrs.close = pfx_close;
    obj->vfptrs.type = pfx_type;
    obj->vfptrs.msendv = pfx_msendv;
    obj->vfptrs.mrecvv = pfx_mrecvv;
    obj->s = s;
    obj->flags = 0;
    /* Create the handle. */
    int h = handle(msock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    return h;
}

int pfx_stop(int s, int64_t deadline) {
    int err;
    struct pfx_sock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != pfx_type)) {
        errno = ENOTSUP; return -1;}
    /* Send termination message. */
    uint64_t sz = 0xffffffffffffffff;
    int rc = bsend(obj->s, &sz, 8, deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error;}
    while(!obj->flags & pfx_sock_PEERDONE) {
        int rc = pfx_mrecvv(s, NULL, 0, deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dsock_slow(rc < 0 && errno != EMSGSIZE)) {err = errno; goto error;}
    }
    int u = obj->s;
    free(obj);
    return u;
error:
    rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
    errno = err;
    return -1;
}

static int pfx_msendv(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct pfx_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == pfx_type);
    uint8_t szbuf[8];
    size_t len = iov_size(iov, iovlen);
    dsock_putll(szbuf, (uint64_t)len);
    struct iovec vec[iovlen + 1];
    vec[0].iov_base = szbuf;
    vec[0].iov_len = 8;
    iov_copy(vec + 1, iov, iovlen);
    int rc = bsendv(obj->s, vec, iovlen + 1, deadline);
    if(dsock_slow(rc < 0)) return -1;
    return 0;
}

static ssize_t pfx_mrecvv(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct pfx_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == pfx_type);
    if(dsock_slow(obj->flags & pfx_sock_PEERDONE)) {errno = EPIPE; return -1;}
    uint8_t szbuf[8];
    int rc = brecv(obj->s, szbuf, 8, deadline);
    if(dsock_slow(rc < 0)) return -1;
    uint64_t sz = dsock_getll(szbuf);
    if(dsock_slow(sz == 0xffffffffffffffff)) {
        /* Peer is terminating. */
        obj->flags |= pfx_sock_PEERDONE;
        errno = EPIPE;
        return -1;
    }
    size_t len = iov_size(iov, iovlen);
    if(dsock_slow(len < sz)) {
        rc = brecv(obj->s, NULL, sz, deadline);
        if(dsock_slow(rc < 0)) return -1;
        errno = EMSGSIZE;
        return -1;
    }
    struct iovec vec[iovlen];
    size_t veclen = iov_cut(iov, vec, iovlen, 0, sz);
    rc = brecvv(obj->s, vec, veclen, deadline);
    if(dsock_slow(rc < 0)) return -1;
    return sz;
}

static void pfx_close(int s) {
    struct pfx_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == pfx_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

