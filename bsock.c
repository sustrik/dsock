
/*

  Copyright (c) 2015 Martin Sustrik

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
#include <stdio.h>
#include <stdlib.h>

#include "dillsocks.h"
#include "utils.h"

static const int dill_bsock_type_placeholder = 0;
static const void *dill_bsock_type = &dill_bsock_type_placeholder;
static void dill_bsock_close(int h);
static const struct hvfptrs dill_bsock_vfptrs = {dill_bsock_close};

struct dill_bsock {
    const void *type;
    void *data;
    struct bsockvfptrs vfptrs;
};

int bsock(const void *type, void *data, const struct bsockvfptrs *vfptrs) {
    if(dill_slow(!type || !data || !vfptrs)) {errno = EINVAL; return -1;}
    struct dill_bsock *sck = malloc(sizeof(struct dill_bsock));
    if(dill_slow(!sck)) {errno = ENOMEM; return -1;}
    sck->type = type;
    sck->data = data;
    sck->vfptrs = *vfptrs;
    int h = handle(dill_bsock_type, sck, &dill_bsock_vfptrs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(sck);
        errno = err;
        return -1;
    }
    return h;
}

void *bsockdata(int s, const void *type) {
    struct dill_bsock *sck = hdata(s, dill_bsock_type);
    if(dill_slow(!sck)) return NULL;
    if(dill_slow(sck->type != type)) {errno = ENOTSUP; return NULL;}
    return sck->data;
}

static void dill_bsock_close(int h) {
    struct dill_bsock *sck = hdata(h, dill_bsock_type);
    dill_assert(sck);
    dill_assert(sck->vfptrs.finish);
    int rc = sck->vfptrs.finish(h, 0);
    dill_assert(rc == 0);
    free(sck);
}

int bsend(int s, const void *buf, size_t len, int64_t deadline) {
    struct dill_bsock *sck = hdata(s, dill_bsock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.send)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.send(s, buf, len, deadline);
}

int brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct dill_bsock *sck = hdata(s, dill_bsock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.recv)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.recv(s, buf, len, deadline);
}

int bflush(int s, int64_t deadline) {
    struct dill_bsock *sck = hdata(s, dill_bsock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.flush)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.flush(s, deadline);
}

int bfinish(int s, int64_t deadline) {
    struct dill_bsock *sck = hdata(s, dill_bsock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.finish)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.finish(s, deadline);
}

