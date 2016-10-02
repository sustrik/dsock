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
#include <string.h>
#include <sys/uio.h>

#include "msock.h"
#include "dsock.h"
#include "utils.h"

static const int keepalive_type_placeholder = 0;
static const void *keepalive_type = &keepalive_type_placeholder;
static void keepalive_close(int s);
static int keepalive_msend(int s, const void *buf, size_t len,
    int64_t deadline);
static ssize_t keepalive_mrecv(int s, void *buf, size_t len, int64_t deadline);
static coroutine void keepalive_sender(int s, int64_t send_interval,
    const uint8_t *buf, size_t len, int sendch, int ackch);

#define KEEPALIVE_SEND 1
#define KEEPALIVE_RECV 2

struct keepalive_sock {
    struct msock_vfptrs vfptrs;
    int s;
    int64_t send_interval;
    int64_t recv_interval;
    uint8_t *buf;
    size_t len;
    int sendch;
    int ackch;
    int sender;
    int64_t last_recv;
};

int keepalive_start(int s, int64_t send_interval,
      int64_t recv_interval, const void *buf, size_t len) {
    int rc;
    int err;
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, msock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct keepalive_sock *obj = malloc(sizeof(struct keepalive_sock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = keepalive_close;
    obj->vfptrs.type = keepalive_type;
    obj->vfptrs.msend = keepalive_msend;
    obj->vfptrs.mrecv = keepalive_mrecv;
    obj->s = s;
    obj->send_interval = send_interval;
    obj->recv_interval = recv_interval;
    obj->buf = malloc(len);
    if(dsock_slow(!obj->buf)) {errno = ENOMEM; goto error2;}
    memcpy(obj->buf, buf, len);
    obj->len = len;
    obj->sendch = -1;
    obj->ackch = -1;
    obj->sender = -1;
    if(send_interval >= 0) {
        obj->sendch = channel(sizeof(struct iovec), 0);
        if(dsock_slow(obj->sendch < 0)) {err = errno; goto error3;}
        obj->ackch = channel(0, 0);
        if(dsock_slow(obj->ackch < 0)) {err = errno; goto error4;}
        obj->sender = go(keepalive_sender(s, send_interval, obj->buf, obj->len,
            obj->sendch, obj->ackch));
        if(dsock_slow(obj->sender < 0)) {err = errno; goto error5;}
    }
    obj->last_recv = now();
    /* Create the handle. */
    int h = handle(msock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error6;}
    return h;
error6:
    if(obj->sender >= 0) {
        rc = hclose(obj->sender);
        dsock_assert(rc == 0);
    }
error5:
    if(obj->ackch >= 0) {
        rc = hclose(obj->ackch);
        dsock_assert(rc == 0);
    }
error4:
    if(obj->sendch >= 0) {
        rc = hclose(obj->sendch);
        dsock_assert(rc == 0);
    }
error3:
    free(obj->buf);
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int keepalive_stop(int s) {
    struct keepalive_sock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != keepalive_type)) {
        errno = ENOTSUP; return -1;}
    if(obj->send_interval >= 0) {
        int rc = hclose(obj->sender);
        dsock_assert(rc == 0);
        rc = hclose(obj->ackch);
        dsock_assert(rc == 0);
        rc = hclose(obj->sendch);
        dsock_assert(rc == 0);
    }
    free(obj->buf);
    int u = obj->s;
    free(obj);
    return u;
}

static int keepalive_msend(int s, const void *buf, size_t len,
      int64_t deadline) {
    struct keepalive_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == keepalive_type);
    /* Send is done in a worker coroutine. */
    struct iovec vec = {(void*)buf, len};
    int rc = chsend(obj->sendch, &vec, sizeof(vec), deadline);
    if(dsock_slow(rc < 0)) return -1;
    /* Wait till worker is done. */
    rc = chrecv(obj->ackch, NULL, 0, deadline);
    if(dsock_slow(rc < 0)) return -1;
    return 0;
}

static coroutine void keepalive_sender(int s, int64_t send_interval,
      const uint8_t *buf, size_t len, int sendch, int ackch) {
    /* Last time something was sent. */
    int64_t last = now();
    while(1) {
        /* Get data to send from the user coroutine. */
        struct iovec vec;
        int rc = chrecv(sendch, &vec, sizeof(vec), last + send_interval);
        if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
        if(dsock_slow(rc < 0 && errno == ETIMEDOUT)) {
            /* Send a keepalive. */
            rc = msend(s, buf, len, -1);
            if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
            if(dsock_slow(rc < 0 && errno == ECONNRESET)) return;
            dsock_assert(rc == 0);
            last = now();
            continue;
        }
        dsock_assert(rc == 0);
        rc = msend(s, vec.iov_base, vec.iov_len, -1);
        if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
        if(dsock_slow(rc < 0 && errno == ECONNRESET)) return;
        dsock_assert(rc == 0);
        last = now();
    }
}

static ssize_t keepalive_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    struct keepalive_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == keepalive_type);
    /* If receive mode is off, just forward the call. */
    if(obj->recv_interval < 0) return mrecv(obj->s, buf, len, deadline);
    /* Compute the deadline. Take keepalive interval into consideration. */
retry:;
    int64_t keepalive_deadline = obj->last_recv + obj->recv_interval;
    int fail_on_deadline = 0;
    if(keepalive_deadline < deadline) {
       deadline = keepalive_deadline;
       fail_on_deadline = 1;
    }
    ssize_t sz = mrecv(obj->s, buf, len, deadline);
    if(dsock_slow(fail_on_deadline && sz < 0 && errno == ETIMEDOUT)) {
        errno = ECONNRESET; return -1;}
    if(dsock_fast(sz >= 0)) {
        int err = errno;
        obj->last_recv = now();
        errno = err;
        /* Filter out keep alive messages. */
        if(sz == obj->len && memcmp(buf, obj->buf, obj->len) == 0) goto retry;
    }
    return sz;
} 

static void keepalive_close(int s) {
    struct keepalive_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == keepalive_type);
    if(obj->send_interval >= 0) {
        int rc = hclose(obj->sender);
        dsock_assert(rc == 0);
        rc = hclose(obj->ackch);
        dsock_assert(rc == 0);
        rc = hclose(obj->sendch);
        dsock_assert(rc == 0);
    }
    free(obj->buf);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

