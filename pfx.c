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

dsock_unique_id(pfx_type);

static void *pfx_hquery(struct hvfs *hvfs, const void *type);
static void pfx_hclose(struct hvfs *hvfs);
static int pfx_msendv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static ssize_t pfx_mrecvv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

struct pfx_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int s;
    int txerr;
    int rxerr;
};

int pfx_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hquery(s, bsock_type))) return -1;
    /* Create the object. */
    struct pfx_sock *obj = malloc(sizeof(struct pfx_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = pfx_hquery;
    obj->hvfs.close = pfx_hclose;
    obj->mvfs.msendv = pfx_msendv;
    obj->mvfs.mrecvv = pfx_mrecvv;
    obj->s = s;
    obj->txerr = 0;
    obj->rxerr = 0;
    /* Create the handle. */
    int h = hcreate(&obj->hvfs);
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
    struct pfx_sock *obj = hquery(s, pfx_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->rxerr)) {err = obj->rxerr; goto error;}
    if(dsock_slow(obj->txerr)) {err = obj->txerr; goto error;}
    /* Send termination message. */
    uint64_t sz = 0xffffffffffffffff;
    int rc = bsend(obj->s, &sz, 8, deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error;}
    while(1) {
        /* TODO: What about oversized messages here? Possible DoS attack. */
        int rc = pfx_mrecvv(&obj->mvfs, NULL, 0, deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dsock_slow(rc < 0)) {err = errno; goto error;}
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

static int pfx_msendv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct pfx_sock *obj = dsock_cont(mvfs, struct pfx_sock, mvfs);
    if(dsock_slow(obj->txerr)) return obj->txerr;
    uint8_t szbuf[8];
    size_t len = iov_size(iov, iovlen);
    dsock_putll(szbuf, (uint64_t)len);
    struct iovec vec[iovlen + 1];
    vec[0].iov_base = szbuf;
    vec[0].iov_len = 8;
    iov_copy(vec + 1, iov, iovlen);
    int rc = bsendv(obj->s, vec, iovlen + 1, deadline);
    if(dsock_slow(rc < 0)) {obj->txerr = errno; return -1;}
    return 0;
}

static ssize_t pfx_mrecvv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct pfx_sock *obj = dsock_cont(mvfs, struct pfx_sock, mvfs);
    if(dsock_slow(obj->rxerr)) return obj->rxerr;
    uint8_t szbuf[8];
    int rc = brecv(obj->s, szbuf, 8, deadline);
    if(dsock_slow(rc < 0)) {obj->rxerr = errno; return -1;}
    uint64_t sz = dsock_getll(szbuf);
    /* Peer is terminating. */
    if(dsock_slow(sz == 0xffffffffffffffff)) {
        errno = obj->rxerr = EPIPE; return -1;}
    size_t len = iov_size(iov, iovlen);
    if(dsock_slow(len < sz)) {errno = obj->rxerr = EMSGSIZE; return -1;}
    struct iovec vec[iovlen];
    size_t veclen = iov_cut(vec, iov, iovlen, 0, sz);
    rc = brecvv(obj->s, vec, veclen, deadline);
    if(dsock_slow(rc < 0)) {obj->rxerr = errno; return -1;}
    return sz;
}

static void *pfx_hquery(struct hvfs *hvfs, const void *type) {
    struct pfx_sock *obj = (struct pfx_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == pfx_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

static void pfx_hclose(struct hvfs *hvfs) {
    struct pfx_sock *obj = (struct pfx_sock*)hvfs;
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

