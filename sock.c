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
#include <stdlib.h>

#include "dillsocks.h"
#include "utils.h"

static const int dill_sock_type_placeholder = 0;
static const void *dill_sock_type = &dill_sock_type_placeholder;

struct sock {
    const void *type;
    void *data;
    socksend_fn send_fn;
    sockrecv_fn recv_fn;
};

int sock(const void *type, void *data, sockstop_fn stop_fn,
      socksend_fn send_fn, sockrecv_fn recv_fn) {
    if(dill_slow(!type)) {errno = EINVAL; return -1;}
    struct sock *sck = malloc(sizeof(struct sock));
    if(dill_slow(!sck)) {errno = ENOMEM; return -1;}
    sck->type = type;
    sck->data = data;
    sck->send_fn = send_fn;
    sck->recv_fn = recv_fn;
    int h = handle(dill_sock_type, sck, stop_fn);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(sck);
        errno = err;
        return -1;
    }
    return h;
}

const void *socktype(int s) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return NULL;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return NULL;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    return sck->type;
}

void *sockdata(int s) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return NULL;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return NULL;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    return sck->data;
}

int sockdone(int s) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return -1;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    int rc = handledone(s);
    if(dill_slow(rc < 0)) return -1;
    free(sck);
    return 0;
}

int socksend(int s, const void *buf, size_t len, int64_t deadline) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return -1;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    if(dill_slow(!sck->send_fn)) {errno = EOPNOTSUPP; return -1;}
    struct iovec iov;
    iov.iov_base = (void*)buf;
    iov.iov_len = len;
    return sck->send_fn(s, &iov, 1, NULL, NULL, deadline);
}

int sockrecv(int s, void *buf, size_t *len, int64_t deadline) {
    if(dill_slow(!len)) {errno = EINVAL; return -1;}
    const void *type = handletype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return -1;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    if(dill_slow(!sck->recv_fn)) {errno = EOPNOTSUPP; return -1;}
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = *len;
    return sck->recv_fn(s, &iov, 1, len, NULL, NULL, deadline);
}

int socksendv(int s, struct iovec *iovs, int niovs, int64_t deadline) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return -1;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    if(dill_slow(!sck->send_fn)) {errno = EOPNOTSUPP; return -1;}
    return sck->send_fn(s, iovs, niovs, NULL, NULL, deadline);
}

int sockrecvv(int s, struct iovec *iovs, int niovs, size_t *len,
      int64_t deadline) {
    const void *type = handletype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != dill_sock_type)) {errno = ENOTSOCK; return -1;}
    struct sock *sck = handledata(s);
    dill_assert(sck);
    if(dill_slow(!sck->recv_fn)) {errno = EOPNOTSUPP; return -1;}
    return sck->recv_fn(s, iovs, niovs, len, NULL, NULL, deadline);
}

