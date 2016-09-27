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

#include "msock.h"
#include "dsock.h"
#include "utils.h"

static const int mthrottler_type_placeholder = 0;
static const void *mthrottler_type = &mthrottler_type_placeholder;
static void mthrottler_close(int s);
static int mthrottler_msend(int s, const void *buf, size_t len,
    int64_t deadline);
static ssize_t mthrottler_mrecv(int s, void *buf, size_t len,
    int64_t deadline);

struct mthrottlersock {
    struct msockvfptrs vfptrs;
    int s;
    size_t send_full;
    size_t send_remaining;
    int64_t send_interval;
    int64_t send_last;
    size_t recv_full;
    size_t recv_remaining;
    int64_t recv_interval;
    int64_t recv_last;
};

int mthrottlerattach(int s,
      uint64_t send_throughput, int64_t send_interval,
      uint64_t recv_throughput, int64_t recv_interval) {
    if(dsock_slow(send_throughput != 0 && send_interval <= 0 )) {
        errno = EINVAL; return -1;}
    if(dsock_slow(recv_throughput != 0 && recv_interval <= 0 )) {
        errno = EINVAL; return -1;}
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, msock_type))) return -1;
    /* Create the object. */
    struct mthrottlersock *obj = malloc(sizeof(struct mthrottlersock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->vfptrs.hvfptrs.close = mthrottler_close;
    obj->vfptrs.type = mthrottler_type;
    obj->vfptrs.msend = mthrottler_msend;
    obj->vfptrs.mrecv = mthrottler_mrecv;
    obj->s = s;
    obj->send_full = 0;
    if(send_throughput > 0) {
        obj->send_full = send_throughput * send_interval / 1000;
        obj->send_remaining = obj->send_full;
        obj->send_interval = send_interval;
        obj->send_last = now();
    }
    obj->recv_full = 0;
    if(recv_throughput > 0) {
        obj->recv_full = recv_throughput * recv_interval / 1000;
        obj->recv_remaining = obj->recv_full;
        obj->recv_interval = recv_interval;
        obj->recv_last = now();
    }
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

int mthrottlerdetach(int s) {
    struct mthrottlersock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != mthrottler_type)) {
        errno = ENOTSUP; return -1;}
    int u = obj->s;
    free(obj);
    return u;
}

static int mthrottler_msend(int s, const void *buf, size_t len,
      int64_t deadline) {
    struct mthrottlersock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == mthrottler_type);
    /* If send-throttling is off forward the call. */
    if(obj->send_full == 0) return msend(obj->s, buf, len, deadline);
    while(1) {
        /* If there's capacity send the message. */
        if(obj->send_remaining) {
            int rc = msend(obj->s, buf, len, deadline);
            if(dsock_slow(rc < 0)) return -1;
            --obj->send_remaining;
            return 0;
        }
        /* Wait till capacity can be renewed. */
        int rc = msleep(obj->send_last + obj->send_interval);
        if(dsock_slow(rc < 0)) return -1;
        /* Renew the capacity. */
        obj->send_remaining = obj->send_full;
        obj->send_last = now();
    }
}

static ssize_t mthrottler_mrecv(int s, void *buf, size_t len,
      int64_t deadline) {
    struct mthrottlersock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == mthrottler_type);
    /* If recv-throttling is off forward the call. */
    if(obj->recv_full == 0) return mrecv(obj->s, buf, len, deadline);
    while(1) {
        /* If there's capacity receive the message. */
        if(obj->recv_remaining) {
            int rc = mrecv(obj->s, buf, len, deadline);
            if(dsock_slow(rc < 0)) return -1;
            --obj->recv_remaining;
            return 0;
        }
        /* Wait till capacity can be renewed. */
        int rc = msleep(obj->recv_last + obj->recv_interval);
        if(dsock_slow(rc < 0)) return -1;
        /* Renew the capacity. */
        obj->recv_remaining = obj->recv_full;
        obj->recv_last = now();
    }
} 

static void mthrottler_close(int s) {
    struct mthrottlersock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == mthrottler_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

