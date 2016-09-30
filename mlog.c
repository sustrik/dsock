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

#include "msock.h"
#include "dsock.h"
#include "utils.h"

static const int mlog_type_placeholder = 0;
static const void *mlog_type = &mlog_type_placeholder;
static void mlog_close(int s);
static int mlog_msend(int s, const void *buf, size_t len, int64_t deadline);
static ssize_t mlog_mrecv(int s, void *buf, size_t len, int64_t deadline);

struct mlog_sock {
    struct msock_vfptrs vfptrs;
    int s;
};

int mlog_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, msock_type))) return -1;
    /* Create the object. */
    struct mlog_sock *obj = malloc(sizeof(struct mlog_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->vfptrs.hvfptrs.close = mlog_close;
    obj->vfptrs.type = mlog_type;
    obj->vfptrs.msend = mlog_msend;
    obj->vfptrs.mrecv = mlog_mrecv;
    obj->s = s;
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

int mlog_stop(int s) {
    struct mlog_sock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != mlog_type)) {
        errno = ENOTSUP; return -1;}
    int u = obj->s;
    free(obj);
    return u;
}

static int mlog_msend(int s, const void *buf, size_t len,
      int64_t deadline) {
    struct mlog_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == mlog_type);
    fprintf(stderr, "send %8zuB: 0x", len);
    size_t i;
    for(i = 0; i != len; ++i)
        fprintf(stderr, "%02x", (int)((uint8_t*)buf)[i]);
    fprintf(stderr, "\n");
    return msend(obj->s, buf, len, deadline);
}

static ssize_t mlog_mrecv(int s, void *buf, size_t len,
      int64_t deadline) {
    struct mlog_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == mlog_type);
    ssize_t sz = mrecv(obj->s, buf, len, deadline);
    if(dsock_slow(sz < 0)) return -1;
    fprintf(stderr, "recv %8zuB: 0x", len);
    size_t i;
    for(i = 0; i != sz && i != len; ++i)
        fprintf(stderr, "%02x", (int)((uint8_t*)buf)[i]);
    fprintf(stderr, "\n");
    return sz;
} 

static void mlog_close(int s) {
    struct mlog_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == mlog_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

