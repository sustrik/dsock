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

#include "lz4/lz4frame.h"

#include "bsock.h"
#include "dsock.h"
#include "utils.h"

#define BLOCK_SIZE 8192

static const int lz4_type_placeholder = 0;
static const void *lz4_type = &lz4_type_placeholder;
static void lz4_close(int s);
static int lz4_bsend(int s, const void *buf, size_t len, int64_t deadline);
static int lz4_brecv(int s, void *buf, size_t len, int64_t deadline);

struct lz4sock {
    struct bsockvfptrs vfptrs;
    int s;
    size_t buflen;
    uint8_t *outbuf;
    uint8_t *inbuf;
    size_t inpos;
    size_t inlast;
    size_t toread;
    LZ4F_decompressionContext_t dctx;
};

int lz4_start(int s) {
    int err;
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, bsock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct lz4sock *obj = malloc(sizeof(struct lz4sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = lz4_close;
    obj->vfptrs.type = lz4_type;
    obj->vfptrs.bsend = lz4_bsend;
    obj->vfptrs.brecv = lz4_brecv;
    obj->s = s;
    obj->buflen = LZ4F_compressFrameBound(BLOCK_SIZE, NULL);
    obj->outbuf = malloc(obj->buflen);
    if(dsock_slow(!obj->outbuf)) {err = ENOMEM; goto error2;}
    obj->inbuf = malloc(obj->buflen);
    if(dsock_slow(!obj->inbuf)) {err = ENOMEM; goto error3;}
    obj->inpos = 0;
    obj->inlast = 0;
    obj->toread = 0;
    size_t ec = LZ4F_createDecompressionContext(&obj->dctx, LZ4F_VERSION);
    if(dsock_slow(LZ4F_isError(ec))) {err = EFAULT; goto error4;}
    /* Create the handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error5;}
    return h;
error5:
    ec = LZ4F_freeDecompressionContext(obj->dctx);
    dsock_assert(!LZ4F_isError(ec));
error4:
    free(obj->inbuf);
error3:
    free(obj->outbuf);
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int lz4_stop(int s) {
    struct lz4sock *obj = hdata(s, bsock_type);
    if(dsock_slow(obj && obj->vfptrs.type != lz4_type)) {
        errno = ENOTSUP; return -1;}
    size_t ec = LZ4F_freeDecompressionContext(obj->dctx);
    dsock_assert(!LZ4F_isError(ec));
    free(obj->inbuf);
    free(obj->outbuf);
    int u = obj->s;
    free(obj);
    return u;
}

static int lz4_bsend(int s, const void *buf, size_t len, int64_t deadline) {
    struct lz4sock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == lz4_type);
    if(dsock_slow(!buf && len > 0)) {errno = EINVAL; return -1;}
    /* Each 8kB of the data will go into separate LZ4 frame. */
    while(len) {
        /* Compress one block. */
        size_t srclen = len > BLOCK_SIZE ? BLOCK_SIZE : len;
        size_t dstlen = LZ4F_compressFrame(obj->outbuf, obj->buflen,
            buf, srclen, NULL);
        dsock_assert(!LZ4F_isError(dstlen));
        buf = (char*)buf + srclen;
        len -= srclen;
        /* Send the compressed frame. */
        uint8_t szbuf[2];
        dsock_puts(szbuf, dstlen);
        int rc = bsend(obj->s, szbuf, 2, deadline);
        if(dsock_slow(rc < 0)) return -1;
        rc = bsend(obj->s, obj->outbuf, dstlen, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
    return 0;
}

static int lz4_brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct lz4sock *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == lz4_type);
    if(dsock_slow(!buf && len > 0)) {errno = EINVAL; return -1;}
    while(len) {
        /* If there's no more data in the buffer read some from the network. */
        if(obj->inpos >= obj->inlast) {
            if(!obj->toread) {
                uint8_t szbuf[2];
                int rc = brecv(obj->s, szbuf, 2, deadline);
                if(dsock_slow(rc < 0)) return -1;
                obj->toread = dsock_gets(szbuf);
            }
            size_t toread = obj->toread <= obj->buflen ?
                obj->toread : obj->buflen;
            int rc = brecv(obj->s, obj->inbuf, toread, deadline);
            if(dsock_slow(rc < 0)) return -1;
            obj->inpos = 0;
            obj->inlast = toread;
        }
        /* Decompress the data. Try to fill in the users buffer. */
        size_t dstsz = len;
        size_t srcsz = obj->inlast - obj->inpos;
        size_t ec = LZ4F_decompress(obj->dctx, buf, &dstsz,
            obj->inbuf + obj->inpos, &srcsz, NULL);
        dsock_assert(!LZ4F_isError(ec));
        obj->inpos += srcsz;
        buf = (char*)buf + dstsz;
        len -= dstsz;
    }
    return 0;
} 

static void lz4_close(int s) {
    struct lz4sock *obj = hdata(s, bsock_type);
    dsock_assert(obj && obj->vfptrs.type == lz4_type);
    size_t ec = LZ4F_freeDecompressionContext(obj->dctx);
    dsock_assert(!LZ4F_isError(ec));
    free(obj->inbuf);
    free(obj->outbuf);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

