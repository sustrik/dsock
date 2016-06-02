
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

static const int dill_msock_type_placeholder = 0;
static const void *dill_msock_type = &dill_msock_type_placeholder;
static void dill_msock_close(int h);
static const struct hvfptrs dill_msock_vfptrs = {dill_msock_close};

struct dill_msock {
    const void *type;
    void *data;
    struct msockvfptrs vfptrs;
};

int msock(const void *type, void *data, const struct msockvfptrs *vfptrs) {
    if(dill_slow(!type || !data || !vfptrs)) {errno = EINVAL; return -1;}
    struct dill_msock *sck = malloc(sizeof(struct dill_msock));
    if(dill_slow(!sck)) {errno = ENOMEM; return -1;}
    sck->type = type;
    sck->data = data;
    sck->vfptrs = *vfptrs;
    int h = handle(dill_msock_type, sck, &dill_msock_vfptrs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(sck);
        errno = err;
        return -1;
    }
    return h;
}

void *msockdata(int s, const void *type) {
    struct dill_msock *sck = hdata(s, dill_msock_type);
    if(dill_slow(!sck)) return NULL;
    if(dill_slow(sck->type != type)) {errno = ENOTSUP; return NULL;}
    return sck->data;
}

static void dill_msock_close(int h) {
    struct dill_msock *sck = hdata(h, dill_msock_type);
    dill_assert(sck);
    int rc = sck->vfptrs.finish(h, 0);
    dill_assert(rc == 0);
    free(sck);
}

int msend(int s, const void *buf, size_t len, int64_t deadline) {
    struct dill_msock *sck = hdata(s, dill_msock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.send)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.send(s, buf, len, deadline);
}

int mrecv(int s, void *buf, size_t *len, int64_t deadline) {
    struct dill_msock *sck = hdata(s, dill_msock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.recv)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.recv(s, buf, len, deadline);
}

int mfinish(int s, int64_t deadline) {
    struct dill_msock *sck = hdata(s, dill_msock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.finish)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.finish(s, deadline);
}

