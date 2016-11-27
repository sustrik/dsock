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
#include <stdio.h>
#include <stdlib.h>

#include "iov.h"
#include "dsockimpl.h"
#include "utils.h"

dsock_unique_id(mtrace_type);

static void *mtrace_hquery(struct hvfs *hvfs, const void *type);
static void mtrace_hclose(struct hvfs *hvfs);
static int mtrace_msendv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static ssize_t mtrace_mrecvv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

struct mtrace_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    /* Underlying socket. */
    int s;
    /* This socket. */
    int h;
};

static void *mtrace_hquery(struct hvfs *hvfs, const void *type) {
    struct mtrace_sock *obj = (struct mtrace_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == mtrace_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int mtrace_start(int s) {
    /* Check whether underlying socket is message-based. */
    if(dsock_slow(!hquery(s, msock_type))) return -1;
    /* Create the object. */
    struct mtrace_sock *obj = malloc(sizeof(struct mtrace_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = mtrace_hquery;
    obj->hvfs.close = mtrace_hclose;
    obj->mvfs.msendv = mtrace_msendv;
    obj->mvfs.mrecvv = mtrace_mrecvv;
    obj->s = s;
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dsock_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    obj->h = h;
    return h;
}

int mtrace_done(int s) {
    dsock_assert(0);
}

int mtrace_stop(int s) {
    struct mtrace_sock *obj = hquery(s, mtrace_type);
    if(dsock_slow(!obj)) return -1;
    int u = obj->s;
    free(obj);
    return u;
}

static int mtrace_msendv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct mtrace_sock *obj = dsock_cont(mvfs, struct mtrace_sock, mvfs);
    size_t len = 0;
    size_t i, j;
    fprintf(stderr, "msend(%d, 0x", obj->h);
    for(i = 0; i != iovlen; ++i) {
        for(j = 0; j != iov[i].iov_len; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
            ++len;
        }
    }
    fprintf(stderr, ", %zu)\n", len);
    return msendv(obj->s, iov, iovlen, deadline);
}

static ssize_t mtrace_mrecvv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct mtrace_sock *obj = dsock_cont(mvfs, struct mtrace_sock, mvfs);
    ssize_t sz = mrecvv(obj->s, iov, iovlen, deadline);
    if(dsock_slow(sz < 0)) return -1;
    size_t i, j;
    fprintf(stderr, "mrecv(%d, 0x", obj->h);
    size_t toprint = sz;
    for(i = 0; i != iovlen && toprint; ++i) {
        for(j = 0; j != iov[i].iov_len && toprint; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
            --toprint;
        }
    }
    fprintf(stderr, ", %zu)\n", (size_t)sz);
    return sz;
}

static void mtrace_hclose(struct hvfs *hvfs) {
    struct mtrace_sock *obj = (struct mtrace_sock*)hvfs;
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

