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
#include <stdio.h>
#include <stdlib.h>

#include "dillsocks.h"
#include "utils.h"

static const int dill_sock_type_placeholder = 0;
static const void *dill_sock_type = &dill_sock_type_placeholder;

static void dill_sock_close(int h);
static void dill_sock_dump(int h);

static const struct hvfptrs dill_sock_vfptrs = {
    dill_sock_close,
    dill_sock_dump
};

struct sock {
    const void *type;
    int flags;
    void *data;
    struct sockvfptrs vfptrs;
};

int sock(const void *type, int flags, void *data,
      const struct sockvfptrs *vfptrs) {
    if(dill_slow(!type)) {errno = EINVAL; return -1;}
    /* Let's guarantee forward compatibility. */
    if(dill_slow(flags & ~(SOCK_IN | SOCK_OUT | SOCK_INMSG | SOCK_OUTMSG |
          SOCK_INREL | SOCK_OUTREL | SOCK_INORD | SOCK_OUTORD))) {
        errno = EINVAL; return -1;}
    /* More sanity checking. */
    if(dill_slow(!vfptrs || !vfptrs->close)) {
        errno = EINVAL; return -1;}
    if(dill_slow((flags & SOCK_IN) && !vfptrs->recv)) {
        errno = EINVAL; return -1;}
    if(dill_slow((flags & SOCK_OUT) && !vfptrs->send)) {
        errno = EINVAL; return -1;}
    /* Create the object. */
    struct sock *sck = malloc(sizeof(struct sock));
    if(dill_slow(!sck)) {errno = ENOMEM; return -1;}
    sck->type = type;
    sck->flags = flags;
    sck->data = data;
    sck->vfptrs = *vfptrs;
    int h = handle(dill_sock_type, sck, &dill_sock_vfptrs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(sck);
        errno = err;
        return -1;
    }
    return h;
}

void *sockdata(int s, const void *type) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return NULL;
    if(dill_slow(type && sck->type != type)) {errno = ENOTSUP; return NULL;}
    return sck->data;
}

int sockflags(int s) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    return sck->flags;
}

static void dill_sock_close(int h) {
    struct sock *sck = hdata(h, dill_sock_type);
    dill_assert(sck);
    sck->vfptrs.close(h);
    free(sck);
}

static void dill_sock_dump(int h) {
    struct sock *sck = hdata(h, dill_sock_type);
    dill_assert(sck);
    if(sck->vfptrs.dump)
        sck->vfptrs.dump(h);
}

int socksend(int s, const void *buf, size_t len, int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.send)) {errno = ENOTSUP; return -1;}
    struct iovec iov;
    iov.iov_base = (void*)buf;
    iov.iov_len = len;
    return sck->vfptrs.send(s, &iov, 1, NULL, NULL, deadline);
}

int sockrecv(int s, void *buf, size_t len, size_t *outlen, int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.recv)) {errno = ENOTSUP; return -1;}
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;
    return sck->vfptrs.recv(s, &iov, 1, outlen, NULL, NULL, deadline);
}

int socksendv(int s, struct iovec *iovs, int niovs, int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.send)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.send(s, iovs, niovs, NULL, NULL, deadline);
}

int sockrecvv(int s, struct iovec *iovs, int niovs, size_t *outlen,
      int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.recv)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.recv(s, iovs, niovs, outlen, NULL, NULL, deadline);
}

int socksendmsg(int s, struct iovec *iovs, int niovs,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.send)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.send(s, iovs, niovs, inctrl, outctrl, deadline);

}

int sockrecvmsg(int s, struct iovec *iovs, int niovs,
      size_t *outlen, const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct sock *sck = hdata(s, dill_sock_type);
    if(dill_slow(!sck)) return -1;
    if(dill_slow(!sck->vfptrs.recv)) {errno = ENOTSUP; return -1;}
    return sck->vfptrs.recv(s, iovs, niovs, outlen, inctrl, outctrl, deadline);
}

