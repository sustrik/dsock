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

#include "dillsocks.h"
#include "utils.h"

int socksend(sock s, const void *buf, size_t len, int64_t deadline) {
    if(dill_slow(!(*s)->send)) {errno = EOPNOTSUPP; return -1;}
    struct iovec iov;
    iov.iov_base = (void*)buf;
    iov.iov_len = len;
    return (*s)->send(s, &iov, 1, NULL, NULL, deadline);
}

int sockrecv(sock s, void *buf, size_t *len, int64_t deadline) {
    if(dill_slow(!len)) {errno = EINVAL; return -1;}
    if(dill_slow(!(*s)->recv)) {errno = EOPNOTSUPP; return -1;}
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = *len;
    return (*s)->recv(s, &iov, 1, len, NULL, NULL, deadline);
}

int socksendv(sock s, struct iovec *iovs, int niovs, int64_t deadline) {
    if(dill_slow(!(*s)->send)) {errno = EOPNOTSUPP; return -1;}
    return (*s)->send(s, iovs, niovs, NULL, NULL, deadline);
}

int sockrecvv(sock s, struct iovec *iovs, int niovs, size_t *len,
      int64_t deadline) {
    if(dill_slow(!(*s)->recv)) {errno = EOPNOTSUPP; return -1;}
    return (*s)->recv(s, iovs, niovs, len, NULL, NULL, deadline);
}

