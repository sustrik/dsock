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

#include "msock.h"
#include "dsock.h"
#include "utils.h"

#define BLOCK_SIZE 8192

static const int lz4_type_placeholder = 0;
static const void *lz4_type = &lz4_type_placeholder;
static void lz4_close(int s);
static int lz4_msend(int s, const void *buf, size_t len, int64_t deadline);
static ssize_t lz4_mrecv(int s, void *buf, size_t len, int64_t deadline);

struct lz4_sock {
    struct msock_vfptrs vfptrs;
    int s;
    uint8_t *outbuf;
    size_t outlen;
    uint8_t *inbuf;
    size_t inlen;
    LZ4F_decompressionContext_t dctx;
};

int lz4_start(int s) {
    int err;
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, msock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct lz4_sock *obj = malloc(sizeof(struct lz4_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = lz4_close;
    obj->vfptrs.type = lz4_type;
    obj->vfptrs.msend = lz4_msend;
    obj->vfptrs.mrecv = lz4_mrecv;
    obj->s = s;
    obj->outbuf = NULL;
    obj->outlen = 0;
    obj->inbuf = NULL;
    obj->inlen = 0;        
    size_t ec = LZ4F_createDecompressionContext(&obj->dctx, LZ4F_VERSION);
    if(dsock_slow(LZ4F_isError(ec))) {err = EFAULT; goto error2;}
    /* Create the handle. */
    int h = handle(msock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    ec = LZ4F_freeDecompressionContext(obj->dctx);
    dsock_assert(!LZ4F_isError(ec));
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int lz4_stop(int s, int64_t deadline) {
    struct lz4_sock *obj = hdata(s, msock_type);
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

static int lz4_msend(int s, const void *buf, size_t len, int64_t deadline) {
    struct lz4_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == lz4_type);
    if(dsock_slow(!buf && len > 0)) {errno = EINVAL; return -1;}
    /* Adjust the buffer size as needed. */
    size_t maxlen = LZ4F_compressFrameBound(len, NULL);
    if(obj->outlen < maxlen) {
        uint8_t *newbuf = realloc(obj->outbuf, maxlen);
        if(dsock_slow(!newbuf)) {errno = ENOMEM; return -1;}
        obj->outbuf = newbuf;
        obj->outlen = maxlen;
    }
    /* Compress the data. */
    LZ4F_preferences_t prefs = {0};
    prefs.frameInfo.contentSize = len;
    size_t dstlen = LZ4F_compressFrame(obj->outbuf, obj->outlen,
        buf, len, &prefs);
    dsock_assert(!LZ4F_isError(dstlen));
    dsock_assert(dstlen <= obj->outlen);
    /* Send the compressed frame. */
    return msend(obj->s, obj->outbuf, dstlen, deadline);
}

static ssize_t lz4_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    struct lz4_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == lz4_type);
    if(dsock_slow(!buf && len > 0)) {errno = EINVAL; return -1;}
    /* Adjust the buffer size as needed. */
    size_t maxlen = LZ4F_compressFrameBound(len, NULL);
    if(obj->inlen < maxlen) {
        uint8_t *newbuf = realloc(obj->inbuf, maxlen);
        if(dsock_slow(!newbuf)) {errno = ENOMEM; return -1;}
        obj->inbuf = newbuf;
        obj->inlen = maxlen;
    }
    /* Get the compressed message. */
    ssize_t sz = mrecv(obj->s, obj->inbuf, obj->inlen, deadline);
    if(dsock_slow(sz < 0)) return -1;
    /* Extract size of the uncompressed message from LZ4 frame header. */
    LZ4F_frameInfo_t info;
    size_t infolen = sz;
    size_t ec = LZ4F_getFrameInfo(obj->dctx, &info, obj->inbuf, &infolen);
    dsock_assert(!LZ4F_isError(ec));
    /* Size is a required field. */
    if(dsock_slow(info.contentSize == 0)) {errno = ECONNRESET; return -1;}
    /* Decompressed message would exceed the buffer size. */
    if(dsock_slow(info.contentSize > len)) {errno = EMSGSIZE; return -1;}
    /* Decompress. */
    size_t dstlen = len;
    size_t srclen = sz - infolen;
    ec = LZ4F_decompress(obj->dctx, buf, &dstlen,
        obj->inbuf + infolen, &srclen, NULL);
    dsock_assert(ec == 0);
    dsock_assert(srclen == sz - infolen);
    return dstlen;
} 

static void lz4_close(int s) {
    struct lz4_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == lz4_type);
    size_t ec = LZ4F_freeDecompressionContext(obj->dctx);
    dsock_assert(!LZ4F_isError(ec));
    free(obj->inbuf);
    free(obj->outbuf);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

