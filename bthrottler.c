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
    uint64_t send_throughput;
    uint64_t send_burst_size;
    uint64_t send_burst_time;
    uint64_t send_queued;
    int64_t send_last;
    uint64_t recv_throughput;
    uint64_t recv_burst_size;
    uint64_t recv_burst_time;
    uint64_t recv_queued;
    int64_t recv_last;
};

int bthrottlerattach(int s,
      uint64_t send_throughput, uint64_t send_burst_size,
      uint64_t recv_throughput, uint64_t recv_burst_size) {
    if(dsock_slow(send_throughput != 0 && send_burst_size == 0 )) {
        errno = EINVAL; return -1;}
    if(dsock_slow(recv_throughput != 0 && recv_burst_size == 0 )) {
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
    obj->send_throughput = send_throughput;
    if(send_throughput > 0) {
        obj->send_burst_size = send_burst_size;
        obj->send_burst_time = send_burst_size * 1000000000 /
            send_throughput / 1000000;
        obj->send_queued = 0;
        obj->send_last = now();
    }
    obj->recv_throughput = recv_throughput;
    if(recv_throughput > 0) {
        obj->recv_burst_size = recv_burst_size;
        obj->recv_burst_time = recv_burst_size * 1000000000 /
            recv_throughput / 1000000;
        obj->recv_queued = recv_burst_size;
        obj->recv_last = now();
    }
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
    /* If send-throttling is off forward the call. */
    if(obj->send_throughput == 0) return bsend(obj->s, buf, len, deadline);
    /* Compute the amount of data still in bucket based on previous know
       amount and the elapsed time. */
    int64_t nw = now();
    uint64_t drained = (nw - obj->send_last) *
        1000000000 / obj->send_throughput / 1000000;
    obj->send_queued = drained > obj->send_queued ? 0 :
        obj->send_queued - drained;
    while(1) {
        /* Send batch of data. We cannot send more that maximum burst size. */
        size_t tosend = obj->send_burst_size - obj->send_queued;
        if(tosend > 0) { 
            if(len < tosend) tosend = len;
            obj->send_queued += tosend;
            obj->send_last = nw;
            int rc = bsend(obj->s, buf, tosend, deadline);
            if(dsock_slow(rc < 0)) return -1;
            if(len == tosend) return 0;
            buf = (char*)buf + tosend;
            len -= tosend;
        }
        /* There are more bytes to send but we've already exhausted maximum
           burst size. We'll have to sleep while the burst is over. If deadline
           isn't sufficient to do the waiting we'll still wait till deadline
           expires so that send has nice consistent behaviour. */
        if(deadline == 0) {errno = ETIMEDOUT; return -1;}
        if(deadline > 0 && nw + obj->send_burst_time > deadline) {
            int rc = msleep(deadline);
            if(dsock_slow(rc < 0)) return -1;
            errno = ETIMEDOUT;
            return -1;
        }
        int rc = msleep(nw + obj->send_burst_time);
        if(dsock_slow(rc < 0)) return -1;
        obj->send_queued = 0;
        /* In case of CPU exhaustion we may have slept longer than we've asked
           for. Thus, we have to fetch the actual time. */
        nw = now();
    }
}

static int bthrottler_brecv(int s, void *buf, size_t len,
      int64_t deadline) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == bthrottler_type);
    /* If recv-throttling is off forward the call. */
    if(obj->recv_throughput == 0) return brecv(obj->s, buf, len, deadline);

    /* TODO */
    dsock_assert(0);
} 

static void bthrottler_close(int s) {
    struct bthrottlersock *obj = hdata(s, bsock_type);
    dsock_assert(obj && obj->vfptrs.type == bthrottler_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

