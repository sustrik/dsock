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

#include "bsock.h"
#include "dsock.h"
#include "utils.h"

static const int nagle_type_placeholder = 0;
static const void *nagle_type = &nagle_type_placeholder;
static void nagle_close(int s);
static int nagle_bsend(int s, const void *buf, size_t len, int64_t deadline);
static int nagle_brecv(int s, void *buf, size_t len, int64_t deadline);
static coroutine void nagle_sender(int s, size_t batch, int64_t interval,
    uint8_t *buf, int sendch, int ackch);

struct naglevec {
    const void *buf;
    size_t len;
};

struct naglesock {
    struct bsockvfptrs vfptrs;
    int s;
    uint8_t *buf;
    int sendch;
    int ackch;
    int sender;
};

int nagle_start(int s, size_t batch, int64_t interval) {
    int rc;
    int err;
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, bsock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct naglesock *obj = malloc(sizeof(struct naglesock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = nagle_close;
    obj->vfptrs.type = nagle_type;
    obj->vfptrs.bsend = nagle_bsend;
    obj->vfptrs.brecv = nagle_brecv;
    obj->s = s;
    obj->buf = malloc(batch);
    if(dsock_slow(!obj->buf)) {errno = ENOMEM; goto error2;}
    obj->sendch = channel(sizeof(struct naglevec), 0);
    if(dsock_slow(obj->sendch < 0)) {err = errno; goto error3;}
    obj->ackch = channel(0, 0);
    if(dsock_slow(obj->ackch < 0)) {err = errno; goto error4;}
    obj->sender = go(nagle_sender(s, batch, interval,
        obj->buf, obj->sendch, obj->ackch));
    if(dsock_slow(obj->sender < 0)) {err = errno; goto error5;}
    /* Create the handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error6;}
    return h;
error6:
    rc = hclose(obj->sender);
    dsock_assert(rc == 0);
error5:
    rc = hclose(obj->ackch);
    dsock_assert(rc == 0);
error4:
    rc = hclose(obj->sendch);
    dsock_assert(rc == 0);
error3:
    free(obj->buf);
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int nagle_stop(int s, int64_t deadline) {
    struct naglesock *obj = hdata(s, bsock_type);
    if(dsock_slow(obj && obj->vfptrs.type != nagle_type)) {
        errno = ENOTSUP; return -1;}
    /* TODO: Flush the data from the buffer! */
    int rc = hclose(obj->sender);
    dsock_assert(rc == 0);
    rc = hclose(obj->ackch);
    dsock_assert(rc == 0);
    rc = hclose(obj->sendch);
    dsock_assert(rc == 0);
    free(obj->buf);
    int u = obj->s;
    free(obj);
    return u;
}

static int nagle_bsend(int s, const void *buf, size_t len, int64_t deadline) {
    struct naglesock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == nagle_type);
    /* Send is done in a worker coroutine. */
    struct naglevec vec = {buf, len};
    int rc = chsend(obj->sendch, &vec, sizeof(vec), deadline);
    if(dsock_slow(rc < 0)) return -1;
    /* Wait till worker is done. */
    rc = chrecv(obj->ackch, NULL, 0, deadline);
    if(dsock_slow(rc < 0)) return -1;
    return 0;
}

static coroutine void nagle_sender(int s, size_t batch, int64_t interval,
      uint8_t *buf, int sendch, int ackch) {
    /* Amount of data in the buffer. */
    size_t len = 0;
    /* Last time at least one byte was sent. */
    int64_t last = now();
    while(1) {
        /* Get data to send from the user coroutine. */
        struct naglevec vec;
        int rc = chrecv(sendch, &vec, sizeof(vec),
            interval >= 0 && len ? last + interval : -1);
        if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
        /* Timeout expired. Flush the data in the buffer. */
        if(dsock_slow(rc < 0 && errno == ETIMEDOUT)) {
            rc = bsend(s, buf, len, -1);
            if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
            dsock_assert(rc == 0);
            len = 0;
            last = now();
            continue;
        }
        dsock_assert(rc == 0);
        /* If data fit into the buffer, store them there. */
        if(len + vec.len < batch) {
            memcpy(buf + len, vec.buf, vec.len);
            len += vec.len;
            rc = chsend(ackch, NULL, 0, -1);
            if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
            dsock_assert(rc == 0);
            continue;
        }
        if(len > 0) {
            /* Flush the buffer. */
            rc = bsend(s, buf, len, -1);
            if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
            dsock_assert(rc == 0);
            len = 0;
            last = now();
        }
        /* Once again: If data fit into buffer store them there. */
        if(vec.len < batch) {
            memcpy(buf, vec.buf, vec.len);
            len = vec.len;
            rc = chsend(ackch, NULL, 0, -1);
            if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
            dsock_assert(rc == 0);
            continue;
        }
        /* This is a big chunk of data, no need to Nagle it.
           We'll send it straight away. */
        rc = bsend(s, vec.buf, vec.len, -1);
        if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
        dsock_assert(rc == 0);
        last = now();
        rc = chsend(ackch, NULL, 0, -1);
        if(dsock_slow(rc < 0 && errno == ECANCELED)) return;
        dsock_assert(rc == 0);
    }
}

static int nagle_brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct naglesock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == nagle_type);
    return brecv(obj->s, buf, len, deadline);
} 

static void nagle_close(int s) {
    struct naglesock *obj = hdata(s, bsock_type);
    dsock_assert(obj && obj->vfptrs.type == nagle_type);
    int rc = hclose(obj->sender);
    dsock_assert(rc == 0);
    rc = hclose(obj->ackch);
    dsock_assert(rc == 0);
    rc = hclose(obj->sendch);
    dsock_assert(rc == 0);
    free(obj->buf);
    rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

