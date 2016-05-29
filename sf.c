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

#include <string.h>

#include "dillsocks.h"
#include "utils.h"

static const int sf_type_placeholder = 0;
static const void *sf_type = &sf_type_placeholder;
static void sf_close(int s);
static int sf_send(int s, const void *buf, size_t len, int64_t deadline);
static int sf_recv(int s, void *buf, size_t *len, int64_t deadline);

static const struct msockvfptrs sf_vfptrs = {
    sf_close,
    sf_send,
    sf_recv
};

struct sf {
    int u;
    int ochan;
    int ichan;
    int oworker;
    int iworker;
};

struct msg {
    char *buf;
    size_t len;
};

static coroutine void sf_oworker(struct sf *conn) {
    struct msg msg;
    while(1) {
        int rc = chrecv(conn->ochan, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) return;
        dill_assert(rc == 0);
        uint64_t hdr;
        dill_putll((uint8_t*)&hdr, msg.len);
        rc = bsend(conn->u, &hdr, sizeof(hdr), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {free(msg.buf); return;}
        if(dill_slow(rc < 0 && errno == ECONNRESET)) {free(msg.buf); return;}
        dill_assert(rc == 0);
        rc = bsend(conn->u, msg.buf, msg.len, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {free(msg.buf); return;}
        if(dill_slow(rc < 0 && errno == ECONNRESET)) {free(msg.buf); return;}
        dill_assert(rc == 0);
        free(msg.buf);
        rc = bflush(conn->u, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) return;
        if(dill_slow(rc < 0 && errno == ECONNRESET)) return;
        dill_assert(rc == 0);
    }
}

static coroutine void sf_iworker(struct sf *conn) {
    while(1) {
        struct msg msg;
        uint64_t hdr;
        int rc = brecv(conn->u, &hdr, sizeof(hdr), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) return;
        if(dill_slow(rc < 0 && errno == ECONNRESET)) return;
        dill_assert(rc >= 0);
        if(dill_slow(hdr == 0xffffffffffffffff)) break;
        msg.len = (size_t)dill_getll((uint8_t*)&hdr);
        msg.buf = malloc(msg.len);
        dill_assert(msg.buf);
        rc = brecv(conn->u, msg.buf, msg.len, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {free(msg.buf); return;}
        if(dill_slow(rc < 0 && errno == ECONNRESET)) {free(msg.buf); return;}
        dill_assert(rc == 0);
        rc = chsend(conn->ichan, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {free(msg.buf); return;}
        dill_assert(rc == 0);
    }
}

int sfattach(int s) {
    /* Check that s is a bytestream. */
    /* TODO */
    int err;
    int rc;
    struct sf *conn = malloc(sizeof(struct sf));
    if(dill_slow(!conn)) {err = ENOMEM; goto error1;}
    conn->u = s;
    conn->ochan = channel(sizeof(struct msg), 0);
    if(dill_slow(conn->ochan < 0)) {err = errno; goto error2;}
    conn->ichan = channel(sizeof(struct msg), 0);
    if(dill_slow(conn->ichan < 0)) {err = errno; goto error3;}
    conn->oworker = go(sf_oworker(conn));
    if(dill_slow(conn->oworker < 0)) {err = errno; goto error4;}
    conn->iworker = go(sf_iworker(conn));
    if(dill_slow(conn->iworker < 0)) {err = errno; goto error5;}
    /* Bind the object to a handle. */
    int h = msock(sf_type, conn, &sf_vfptrs);
    if(dill_slow(h < 0)) {err = errno; goto error6;}
    return h;
error6:
    rc = hclose(conn->iworker);
    dill_assert(rc == 0);
error5:
    rc = hclose(conn->oworker);
    dill_assert(rc == 0);
error4:
    rc = hclose(conn->ichan);
    dill_assert(rc == 0);
error3:
    rc = hclose(conn->ochan);
    dill_assert(rc == 0);
error2:
    free(conn);
error1:
    errno = err;
    return -1;
}

int sfdetach(int s, int64_t deadline);

static void sf_close(int s) {
    struct sf *conn = msockdata(s, sf_type);
    dill_assert(conn);
    int rc = hclose(conn->iworker);
    dill_assert(rc == 0);
    rc = hclose(conn->oworker);
    dill_assert(rc == 0);
    rc = hclose(conn->ichan);
    dill_assert(rc == 0);
    rc = hclose(conn->ochan);
    dill_assert(rc == 0);
    rc = hclose(conn->u);
    dill_assert(rc == 0);
}

static int sf_send(int s, const void *buf, size_t len, int64_t deadline) {
    struct sf *conn = msockdata(s, sf_type);
    if(dill_slow(!conn)) return -1;
    /* Create a message object. */
    struct msg msg;
    msg.buf = malloc(len);
    if(dill_slow(!msg.buf)) {errno = ENOMEM; return -1;}
    memcpy(msg.buf, buf, len);
    msg.len = len;
    /* Send it to the worker. */
    int rc = chsend(conn->ochan, &msg, sizeof(msg), deadline);
    if(dill_fast(rc >= 0)) return 0;
    int err = errno;
    free(msg.buf);
    errno = err;
    return -1;
}

static int sf_recv(int s, void *buf, size_t *len, int64_t deadline) {
    if(dill_slow(!len)) {errno = EINVAL; return -1;}
    struct sf *conn = msockdata(s, sf_type);
    if(dill_slow(!conn)) return -1;
    /* Get message from the worker. */
    struct msg msg;
    int rc = chrecv(conn->ichan, &msg, sizeof(msg), deadline);
    if(dill_slow(rc < 0)) return -1;
    /* Fill in user's buffer. */
    size_t tocopy = *len < msg.len ? *len : msg.len;
    memcpy(buf, msg.buf, tocopy);
    *len = msg.len;
    free(msg.buf);
    return 0;
}

