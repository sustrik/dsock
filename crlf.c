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

#include "dsockimpl.h"
#include "iov.h"
#include "utils.h"

dsock_unique_id(crlf_type);

static void *crlf_hquery(struct hvfs *hvfs, const void *type);
static void crlf_hclose(struct hvfs *hvfs);
static int crlf_msendv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static ssize_t crlf_mrecvv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

#define CRLF_SOCK_PEERDONE 1

struct crlf_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    /* Given that we are doing one recv call per byte, let's cache the pointer
       to bsock interface of the underlying socket. */
    struct bsock_vfs *uvfs;
    int txerr;
    int rxerr;
};

static void *crlf_hquery(struct hvfs *hvfs, const void *type) {
    struct crlf_sock *obj = (struct crlf_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == crlf_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int crlf_start(int s) {
    int err;
    /* Create the object. */
    struct crlf_sock *obj = malloc(sizeof(struct crlf_sock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->hvfs.query = crlf_hquery;
    obj->hvfs.close = crlf_hclose;
    obj->mvfs.msendv = crlf_msendv;
    obj->mvfs.mrecvv = crlf_mrecvv;
    obj->u = -1;
    obj->uvfs = hquery(s, bsock_type);
    if(dsock_slow(!obj->uvfs)) {err = errno; goto error2;}
    obj->txerr = 0;
    obj->rxerr = 0;
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    /* Make a private copy of the underlying socket. */
    obj->u = hdup(s);
    if(dsock_slow(obj->u < 0)) {err = errno; goto error3;}
    int rc = hclose(s);
    dsock_assert(rc == 0);
    return h;
error3:
    rc = hclose(h);
    dsock_assert(rc == 0);
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int crlf_done(int s, int64_t deadline) {
    struct crlf_sock *obj = hquery(s, crlf_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->txerr)) {errno = obj->txerr; return -1;}
    /* Send termination message. */
    int rc = bsend(obj->u, "\r\n", 2, deadline);
    if(dsock_slow(rc < 0)) {obj->txerr = errno; return -1;}
    obj->txerr = EPIPE;
    return 0;
}

int crlf_stop(int s, int64_t deadline) {
    int err;
    struct crlf_sock *obj = hquery(s, crlf_type);
    if(dsock_slow(!obj)) return -1;
    /* Send the termination message. */
    if(dsock_slow(obj->txerr != 0 && obj->txerr != EPIPE)) {
        err = obj->txerr; goto error;}
    if(obj->txerr == 0) {
        int rc = bsend(obj->u, "\r\n", 2, deadline);
        if(dsock_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Drain incoming messages until termination message is received. */
    while(1) {
        ssize_t sz = crlf_mrecvv(&obj->mvfs, NULL, 0, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(dsock_slow(sz < 0)) {err = errno; goto error;}
    }
    int u = obj->u;
    free(obj);
    return u;
error:;
    int rc = hclose(obj->u);
    dsock_assert(rc == 0);
    free(obj);
    errno = err;
    return -1;
}

static int crlf_msendv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct crlf_sock *obj = dsock_cont(mvfs, struct crlf_sock, mvfs);
    if(dsock_slow(obj->txerr)) {errno = obj->txerr; return -1;}
    /* Make sure that message doesn't contain CRLF sequence. */
    uint8_t c = 0;
    size_t sz = 0;
    int i, j;
    for(i = 0; i != iovlen; ++i) {
        for(j = 0; j != iov[i].iov_len; ++j) {
            uint8_t c2 = ((uint8_t*)iov[i].iov_base)[j];
            if(dsock_slow(c == '\r' && c2 == '\n')) {errno = EINVAL; return -1;}
            c = c2;
        }
        sz += iov[i].iov_len;
    }
    /* Can't send empty line. Empty line is used as protocol terminator. */
    if(dsock_slow(sz == 0)) {errno = EINVAL; return -1;}
    struct iovec vec[iovlen + 1];
    iov_copy(vec, iov, iovlen);
    vec[iovlen].iov_base = (void*)"\r\n";
    vec[iovlen].iov_len = 2;
    int rc = obj->uvfs->bsendv(obj->uvfs, vec, iovlen + 1, deadline);
    if(dsock_slow(rc < 0)) {obj->txerr= errno; return -1;}
    return 0;
}

static ssize_t crlf_mrecvv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct crlf_sock *obj = dsock_cont(mvfs, struct crlf_sock, mvfs);
    if(dsock_slow(obj->rxerr)) {errno = obj->rxerr; return -1;}
    size_t row = 0;
    size_t column = 0;
    size_t sz = 0;
    char c = 0;
    struct iovec vec = {&c, 1};
    char pc;
    while(1) {
        pc = c;
        int rc = obj->uvfs->brecvv(obj->uvfs, &vec, 1, deadline);
        if(dsock_slow(rc < 0)) {obj->rxerr = errno; return -1;}
        if(row < iovlen && iov && iov[row].iov_base) {
            ((char*)iov[row].iov_base)[column] = c;
            if(column == iov[row].iov_len) {
                ++row;
                column = 0;
            }
            else {
                ++column;
            }
        }
        ++sz;
        if(pc == '\r' && c == '\n') break;
    }
    /* Peer is terminating. */
    if(dsock_slow(sz == 2)) {errno = obj->rxerr = EPIPE; return -1;}
    return sz - 2;
}

static void crlf_hclose(struct hvfs *hvfs) {
    struct crlf_sock *obj = (struct crlf_sock*)hvfs;
    if(dsock_fast(obj->u >= 0)) {
        int rc = hclose(obj->u);
        dsock_assert(rc == 0);
    }
    free(obj);
}

