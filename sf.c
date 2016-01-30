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

#include <libdill.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "dillsocks.h"
#include "utils.h"

static int sfrecv(sock s, void *buf, size_t *len, int64_t deadline);
static int sfsend(sock s, const void *buf, size_t len, int64_t deadline);
static int sfflush(sock s, int64_t deadline);

struct sfmsg {
    uint8_t *buf;
    size_t len;
};

struct sfsock {
    struct sock_vfptr *vfptr;
    sock u;
    struct sfmsg smsg;
    chan chsender;
    coro sender;
    chan chreceiver;
    coro receiver;
};

static struct sock_vfptr sfsock_vfptr = {
    NULL,
    NULL,
    NULL,
    sfrecv,
    sfsend,
    sfflush
};


static coroutine void sfsender(struct sfsock *s) {
    while(1) {
        struct sfmsg msg;
        int rc = chrecv(s->chsender, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED))
            return;
        /* TODO: Convert to network byte order. */
        uint8_t buf[8];
        *(int64_t*)buf = msg.len;
        rc = bsend(s->u, buf, 8, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {
            free(msg.buf);
            return;
        }
        dill_assert(rc == 0); /* TODO */
        rc = bsend(s->u, msg.buf, msg.len, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {
            free(msg.buf);
            return;
        }
        dill_assert(rc == 0); /* TODO */
        rc = bflush(s->u, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {
            free(msg.buf);
            return;
        }
        dill_assert(rc == 0); /* TODO */

        free(msg.buf);
    }
}

static coroutine void sfreceiver(struct sfsock *s) {
    while(1) {
        uint8_t buf[8];
        int rc = brecv(s->u, buf, 8, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED))
            return;
        dill_assert(rc == 0); /* TODO */
        /* TODO: Convert from network byte order. */
        struct sfmsg msg;
        msg.len = *(size_t*)buf;
        msg.buf = malloc(msg.len);
        dill_assert(msg.buf);
        rc = brecv(s->u, msg.buf, msg.len, -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {
            free(msg.buf);
            return;
        }
        dill_assert(rc == 0); /* TODO */
        rc = chsend(s->chsender, &msg, sizeof(msg), -1);
        if(dill_slow(rc < 0 && errno == ECANCELED)) {
            free(msg.buf);
            return;
        }
        dill_assert(rc == 0); 
    }
}

sock sfattach(sock s) {
    int err;
    /* Simple framing is built on top of a bytestream. */
    if(dill_slow(!bcansend(s) || !bcanrecv(s))) {err = EPROTOTYPE; goto err0;}
    /* Allocate the object. */
    struct sfsock *sfs = malloc(sizeof(struct sfsock));
    if(dill_slow(!sfs)) {err = ENOMEM; goto err0;}
    sfs->vfptr = &sfsock_vfptr;
    sfs->u = s;
    sfs->smsg.buf = NULL;
    sfs->smsg.len = 0;
    /* Start the worker coroutines. */
    sfs->chsender = channel(sizeof(struct sfmsg), 0);
    if(dill_slow(!sfs->chsender)) {err = errno; goto err1;}
    sfs->chreceiver = channel(sizeof(struct sfmsg), 0);
    if(dill_slow(!sfs->chreceiver)) {err = errno; goto err2;}
    sfs->sender = go(sfsender(sfs));
    if(dill_slow(!sfs->sender)) {err = errno; goto err3;}
    sfs->receiver = go(sfreceiver(sfs));
    if(dill_slow(!sfs->receiver)) {err = errno; goto err4;}
    return (sock)sfs;
    /* Error handling. */
    err4:
    gocancel(&sfs->sender, 1, 0);
    err3:
    chclose(sfs->chreceiver);
    err2:
    chclose(sfs->chsender);
    err1:
    free(sfs);
    err0:
    errno = err;
    return NULL;
}

static int sfrecv(sock s, void *buf, size_t *len, int64_t deadline) {
    struct sfsock *sfs = (struct sfsock*)s;
    struct sfmsg msg;
    int rc = chrecv(sfs->chreceiver, &msg, sizeof(msg), deadline);
    if(dill_slow(rc < 0)) return -1;
    size_t tocopy = *len < msg.len ? *len : msg.len;
    memcpy(buf, msg.buf, tocopy);
    *len = msg.len;
    free(msg.buf);
    return 0;
}

static int sfsend(sock s, const void *buf, size_t len, int64_t deadline) {
    struct sfsock *sfs = (struct sfsock*)s;
    if(dill_slow(len & !buf)) {errno = EINVAL; return -1;}
    uint8_t *newbuf = realloc(sfs->smsg.buf, sfs->smsg.len + len);
    if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
    memcpy(newbuf + sfs->smsg.len, buf, len);
    sfs->smsg.buf = newbuf;
    sfs->smsg.len += len;
    return 0;
}

static int sfflush(sock s, int64_t deadline) {
    struct sfsock *sfs = (struct sfsock*)s;
    int rc = chsend(sfs->chsender, &sfs->smsg, sizeof(sfs->smsg), deadline);
    if(dill_slow(rc < 0)) return -1;
    sfs->smsg.buf = NULL;
    sfs->smsg.len = 0;
    return 0;
}

int sfclose(sock s, int64_t deadline) {
    dill_assert(0);
}

