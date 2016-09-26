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
#include "utils.h"

static const int bthrottler_type_placeholder = 0;
static const void *bthrottler_type = &bthrottler_type_placeholder;
static void bthrottler_close(int s);
static int bthrottler_bsend(int s, const void *buf, size_t len,
    int64_t deadline);
static int bthrottler_brecv(int s, void *buf, size_t len,
    int64_t deadline);

struct bthrottlersock {
    struct bsockvfptrs vfptrs;
    int s;
    uint64_t bandwidth;
    uint64_t burst_size;
    uint64_t burst_time;
    uint64_t queued;
    int64_t last;
};

int bthrottlerattach(int s, uint64_t bandwidth, uint64_t max_burst_size) {
    if(dsock_slow(bandwidth == 0 || max_burst_size == 0)) {
        errno = EINVAL; return -1;}
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, bsock_type))) return -1;
    /* Create the object. */
    struct bthrottlersock *obj = malloc(sizeof(struct bthrottlersock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->vfptrs.hvfptrs.close = bthrottler_close;
    obj->vfptrs.type = bthrottler_type;
    obj->vfptrs.bsend = bthrottler_bsend;
    obj->vfptrs.brecv = bthrottler_brecv;
    obj->s = s;
    obj->bandwidth = bandwidth;
    obj->burst_size = max_burst_size;
    obj->burst_time = max_burst_size * 1000000000 / bandwidth / 1000000;
    obj->queued = 0;
    obj->last = now();
    /* Create the handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    return h;
}

int bthrottlerdetach(int s) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    if(dsock_slow(obj && obj->vfptrs.type != bthrottler_type)) {
        errno = ENOTSUP; return -1;}
    int u = obj->s;
    free(obj);
    return u;
}

static int bthrottler_bsend(int s, const void *buf, size_t len,
      int64_t deadline) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == bthrottler_type);
    /* Compute the amount of data still in bucket based on previous know
       amount and the elapsed time. */
    int64_t nw = now();
    uint64_t drained = (nw - obj->last) *
        1000000000 / obj->bandwidth / 1000000;
    obj->queued = drained > obj->queued ? 0 : obj->queued - drained;
    while(1) {
        /* Send batch of data. We cannot send more that maximum burst size. */
        size_t tosend = obj->burst_size - obj->queued;
        if(len < tosend) tosend = len;
        obj->queued += tosend;
        obj->last = nw;
        int rc = bsend(obj->s, buf, tosend, deadline);
        if(dsock_slow(rc < 0)) return -1;
        if(len == tosend) return 0;
        buf = (char*)buf + tosend;
        len -= tosend;
        /* There are more bytes to send but we've already exhausted maximum
           burst size. We'll have to sleep while the burst is over. If deadline
           isn't sufficient to do the waiting we'll still wait till deadline
           expires so that send has nice consistent behaviour. */
        if(nw + obj->burst_time > deadline) {
            rc = msleep(deadline);
            if(dsock_slow(rc < 0)) return -1;
            errno = ETIMEDOUT;
            return -1;
        }
        rc = msleep(nw + obj->burst_time);
        if(dsock_slow(rc < 0)) return -1;
        obj->queued = 0;
        /* In case of CPU exhaustion we may have slept longer than we've asked
           for. Thus, we have to fetch the actual time. */
        nw = now();
    }
}

static int bthrottler_brecv(int s, void *buf, size_t len,
      int64_t deadline) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == bthrottler_type);
    return brecv(obj->s, buf, len, deadline);
}

static void bthrottler_close(int s) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    dsock_assert(obj && obj->vfptrs.type == bthrottler_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

