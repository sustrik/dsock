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

#include "bsock.h"
#include "dsock.h"
#include "iov.h"
#include "utils.h"

dsock_unique_id(blog_type);

static void *blog_hquery(struct hvfs *hvfs, const void *type);
static void blog_hclose(struct hvfs *hvfs);
static int blog_bsendv(struct bsock_vfs *bvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static int blog_brecvv(struct bsock_vfs *bvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

struct blog_sock {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    /* Underlying socket. */
    int s;
    /* This socket. */
    int h;
};

int blog_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hquery(s, bsock_type))) return -1;
    /* Create the object. */
    struct blog_sock *obj = malloc(sizeof(struct blog_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = blog_hquery;
    obj->hvfs.close = blog_hclose;
    obj->bvfs.bsendv = blog_bsendv;
    obj->bvfs.brecvv = blog_brecvv;
    obj->s = s;
    /* Create the handle. */
    int h = hcreate(&obj->hvfs);
    if(dsock_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    obj->h = h;
    return h;
}

int blog_stop(int s) {
    struct blog_sock *obj = hquery(s, blog_type);
    if(dsock_slow(!obj)) return -1;
    int u = obj->s;
    free(obj);
    return u;
}

static int blog_bsendv(struct bsock_vfs *bvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct blog_sock *obj = dsock_cont(bvfs, struct blog_sock, bvfs);
    size_t len = 0;
    size_t i, j;
    fprintf(stderr, "bsend(%d, 0x", obj->h);
    for(i = 0; i != iovlen; ++i) {
        for(j = 0; j != iov[i].iov_len; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
            ++len;
        }
    }
    fprintf(stderr, ", %zu)\n", len);
    return bsendv(obj->s, iov, iovlen, deadline);
}

static int blog_brecvv(struct bsock_vfs *bvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct blog_sock *obj = dsock_cont(bvfs, struct blog_sock, bvfs);
    int rc = brecvv(obj->s, iov, iovlen, deadline);
    if(dsock_slow(rc < 0)) return -1;
    size_t len = 0;
    size_t i, j;
    fprintf(stderr, "brecv(%d, 0x", obj->h);
    for(i = 0; i != iovlen; ++i) {
        for(j = 0; j != iov[i].iov_len; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
            ++len;
        }
    }
    fprintf(stderr, ", %zu)\n", len);
    return 0;
}

static void *blog_hquery(struct hvfs *hvfs, const void *type) {
    struct blog_sock *obj = (struct blog_sock*)hvfs;
    if(type == bsock_type) return &obj->bvfs;
    if(type == blog_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

static void blog_hclose(struct hvfs *hvfs) {
    struct blog_sock *obj = (struct blog_sock*)hvfs;
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

