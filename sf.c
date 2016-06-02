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

#define SF_ACTIVE 0
#define SF_DONE 1
#define SF_RESET 2

static uint64_t sf_termsequence = 0xffffffffffffffff;

static const int sf_type_placeholder = 0;
static const void *sf_type = &sf_type_placeholder;
static int sf_finish(int s, int64_t deadline);
static int sf_send(int s, const void *buf, size_t len, int64_t deadline);
static int sf_recv(int s, void *buf, size_t *len, int64_t deadline);

static const struct msockvfptrs sf_vfptrs = {
    sf_finish,
    sf_send,
    sf_recv
};

struct sf {
    int u;
    int ochan;
    int ores;
    int ichan;
    int ires;
    int oworker;
    int iworker;
    int res;
};

struct msg {
    char *buf;
    size_t len;
};

static coroutine void sf_iworker(struct sf *conn);
static coroutine void sf_oworker(struct sf *conn);

int sfattach(int s) {
    int err;
    int rc;
    /* This will ensure that s is actually a bytestream. */
    rc = bflush(s, -1);
    if(dill_slow(rc < 0)) return -1;
    /* Create a sf socket. */
    struct sf *conn = malloc(sizeof(struct sf));
    if(dill_slow(!conn)) {err = ENOMEM; goto error1;}
    conn->u = s;
    conn->ochan = channel(sizeof(struct msg), 0);
    if(dill_slow(conn->ochan < 0)) {err = errno; goto error2;}
    conn->ores = SF_ACTIVE;
    conn->ichan = channel(sizeof(struct msg), 0);
    if(dill_slow(conn->ichan < 0)) {err = errno; goto error3;}
    conn->ires = SF_ACTIVE;
    conn->oworker = go(sf_oworker(conn));
    if(dill_slow(conn->oworker < 0)) {err = errno; goto error4;}
    conn->iworker = go(sf_iworker(conn));
    if(dill_slow(conn->iworker < 0)) {err = errno; goto error5;}
    conn->res = SF_ACTIVE;
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

int sfdetach(int s, int64_t deadline) {
    int err = 0;
    struct sf *conn = msockdata(s, sf_type);
    if(dill_slow(!conn)) return -1;
    /* If connection is broken don't even try to do termination handshake. */
    if(conn->res == SF_RESET) {err = ECONNRESET; goto dealloc;}
    /* Ask oworker to exit. */
    struct msg msg = {NULL, 0};
    int rc = chsend(conn->ochan, &msg, sizeof(msg), deadline);
    if(dill_slow(rc < 0 && errno == EPIPE)) {err = ECONNRESET; goto dealloc;}
    if(dill_slow(rc < 0)) {err = errno; goto dealloc;}
    /* Given that there's no way for oworker to receive this message,
       the function only exits when it closes the channel. */
    rc = chsend(conn->ochan, &msg, sizeof(msg), deadline);
    dill_assert(rc < 0);
    if(dill_slow(errno != EPIPE)) {err = errno; goto dealloc;}
    if(dill_slow(conn->ores == SF_RESET)) {err = ECONNRESET; goto dealloc;}
    dill_assert(conn->ores == SF_DONE);
    /* Now that oworker have exited send the termination sequence. */
    rc = bsend(conn->u, &sf_termsequence, sizeof(sf_termsequence), -1);
    if(dill_slow(rc < 0)) {err = errno; goto dealloc;}
    rc = bflush(conn->u, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto dealloc;} 
    /* Read and drop any pending inbound messages. By doing this we'll ensure
       that reading on the underlying socket will continue from the first byte
       following the sf termination sequence. */
    if(conn->res == SF_ACTIVE) {
        while(1) {
            struct msg msg;
            rc = chrecv(conn->ichan, &msg, sizeof(msg), deadline);
            if(rc < 0) break;
            free(msg.buf);
        }
        if(dill_slow(errno != EPIPE)) {err = errno; goto dealloc;}
        if(dill_slow(conn->ires == SF_RESET)) {err = ECONNRESET; goto dealloc;}
        dill_assert(conn->ires == SF_DONE);
    }
dealloc:
    /* Deallocate the object. */
    rc = hclose(conn->iworker);
    dill_assert(rc == 0);
    rc = hclose(conn->oworker);
    dill_assert(rc == 0);
    rc = hclose(conn->ichan);
    dill_assert(rc == 0);
    rc = hclose(conn->ochan);
    dill_assert(rc == 0);
    int u = conn->u;
    free(conn);
    if(err == 0) return u;
    rc = hclose(u);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

static int sf_finish(int s, int64_t deadline) {
    int u = sfdetach(s, deadline);
    if(dill_slow(u < 0)) return -1;
    int rc = bfinish(u, deadline);
    if(dill_slow(rc != 0)) return -1;
    return 0;
}

#define CHECKRC \
    if(dill_slow(rc < 0 && errno == ECANCELED)) break;\
    if(dill_slow(rc < 0 && errno == ECONNRESET)) {\
        conn->ores = SF_RESET; break;}\
    dill_assert(rc == 0);

static int sf_send(int s, const void *buf, size_t len, int64_t deadline) {
    struct sf *conn = msockdata(s, sf_type);
    if(dill_slow(!conn)) return -1;
    if(dill_slow(conn->res != SF_ACTIVE)) {errno = ECONNRESET; return -1;}
    /* Create a message object. */
    struct msg msg;
    msg.buf = malloc(len);
    if(dill_slow(!msg.buf)) {errno = ENOMEM; return -1;}
    memcpy(msg.buf, buf, len);
    msg.len = len;
    /* Send it to the worker. */
    int rc = chsend(conn->ochan, &msg, sizeof(msg), deadline);
    if(dill_fast(rc >= 0)) return 0;
    /* Closed pipe means that the connection was terminated. */
    if(errno == EPIPE) {
        dill_assert(conn->ores == SF_RESET);
        conn->res = SF_RESET;
        errno = ECONNRESET;
    }
    /* Clean up. */
    int err = errno;
    free(msg.buf);
    errno = err;
    return -1;
}

static coroutine void sf_oworker(struct sf *conn) {
    struct msg msg = {NULL, 0};
    while(1) {
        int rc = chrecv(conn->ochan, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) break;
        dill_assert(rc == 0);
        /* User requests that the coroutine stops. */
        if(!msg.buf) {conn->ores = SF_DONE; break;}
        uint64_t hdr;
        dill_putll((uint8_t*)&hdr, msg.len);
        rc = bsend(conn->u, &hdr, sizeof(hdr), -1);
        CHECKRC
        rc = bsend(conn->u, msg.buf, msg.len, -1);
        CHECKRC
        free(msg.buf);
        msg.buf = NULL;
        msg.len = 0;
        rc = bflush(conn->u, -1);
        CHECKRC
    }
    free(msg.buf);
    int rc = chdone(conn->ochan);
    dill_assert(rc == 0);
}

static int sf_recv(int s, void *buf, size_t *len, int64_t deadline) {
    if(dill_slow(!len)) {errno = EINVAL; return -1;}
    struct sf *conn = msockdata(s, sf_type);
    if(dill_slow(!conn)) return -1;
    if(dill_slow(conn->res != SF_ACTIVE)) {errno = ECONNRESET; return -1;}
    /* Get message from the worker. */
    struct msg msg;
    int rc = chrecv(conn->ichan, &msg, sizeof(msg), deadline);
    /* Worker signals disconnection by closing the pipe. */
    if(dill_slow(rc < 0 && errno == EPIPE)) {
        if(conn->res != SF_RESET)
            conn->res = conn->ires;
        errno = ECONNRESET;
        return -1;
    }
    if(dill_slow(rc < 0)) return -1;
    /* Fill in user's buffer. */
    size_t tocopy = *len < msg.len ? *len : msg.len;
    memcpy(buf, msg.buf, tocopy);
    *len = msg.len;
    free(msg.buf);
    return 0;
}

static coroutine void sf_iworker(struct sf *conn) {
    struct msg msg = {NULL, 0};
    while(1) {
        uint64_t hdr;
        int rc = brecv(conn->u, &hdr, sizeof(hdr), -1);
        CHECKRC
        if(dill_slow(hdr == sf_termsequence)) {conn->ires = SF_DONE; break;}
        msg.len = (size_t)dill_getll((uint8_t*)&hdr);
        msg.buf = malloc(msg.len);
        dill_assert(msg.buf);
        rc = brecv(conn->u, msg.buf, msg.len, -1);
        CHECKRC
        rc = chsend(conn->ichan, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) break;
        dill_assert(rc == 0);
        msg.buf = NULL;
        msg.len = 0;
    }
    free(msg.buf);
    int rc = chdone(conn->ichan);
    dill_assert(rc == 0);
}

