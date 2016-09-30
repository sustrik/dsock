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
#include "msock.h"
#include "utils.h"

static const int pfx_type_placeholder = 0;
static const void *pfx_type = &pfx_type_placeholder;
static void pfx_close(int s);
static int pfx_msend(int s, const void *buf, size_t len, int64_t deadline);
static ssize_t pfx_mrecv(int s, void *buf, size_t len, int64_t deadline);

struct pfxsock {
    struct msockvfptrs vfptrs;
    int s;
};

int pfx_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hdata(s, bsock_type))) return -1;
    /* Create the object. */
    struct pfxsock *obj = malloc(sizeof(struct pfxsock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->vfptrs.hvfptrs.close = pfx_close;
    obj->vfptrs.type = pfx_type;
    obj->vfptrs.msend = pfx_msend;
    obj->vfptrs.mrecv = pfx_mrecv;
    obj->s = s;
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

int pfx_stop(int s, int64_t deadline) {
    struct pfxsock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != pfx_type)) {
        errno = ENOTSUP; return -1;}
    int u = obj->s;
    free(obj);
    return u;
}

static int pfx_msend(int s, const void *buf, size_t len, int64_t deadline) {
    struct pfxsock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == pfx_type);
    uint8_t szbuf[8];
    dsock_putll(szbuf, (uint64_t)len);
    int rc = bsend(obj->s, szbuf, 8, deadline);
    if(dsock_slow(rc < 0)) return -1;
    rc = bsend(obj->s, buf, len, deadline);
    if(dsock_slow(rc < 0)) return -1;
    return 0;
}

static ssize_t pfx_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    struct pfxsock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == pfx_type);
    uint8_t szbuf[8];
    int rc = brecv(obj->s, szbuf, 8, deadline);
    if(dsock_slow(rc < 0)) return -1;
    uint64_t sz = dsock_getll(szbuf);
    size_t torecv = (size_t)(len < sz ? len : sz);
    rc = brecv(obj->s, buf, torecv, deadline);
    if(dsock_slow(rc < 0)) return -1;
    if(torecv < sz) {
        rc = brecv(obj->s, NULL, sz - torecv, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
    return sz;
}

static void pfx_close(int s) {
    struct pfxsock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == pfx_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

