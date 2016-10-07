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

#ifndef DSOCK_FD_H_INCLUDED
#define DSOCK_FD_H_INCLUDED

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#if defined MSG_NOSIGNAL
#define FD_NOSIGNAL MSG_NOSIGNAL
#else
#define FD_NOSIGNAL 0
#endif

struct fdrxbuf {
    size_t len;
    size_t pos;
    uint8_t data[2000];
};

void fdinitrxbuf(
    struct fdrxbuf *rxbuf);
int fdunblock(
    int s);
int fdconnect(
    int s,
    const struct sockaddr *addr,
    socklen_t addrlen,
    int64_t deadline);
int fdaccept(
    int s,
    struct sockaddr *addr,
    socklen_t *addrlen,
    int64_t deadline);
int fdsend(
    int s,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);
int fdrecv(
    int s,
    struct fdrxbuf *rxbuf,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);
int fdclose(
    int s);

#endif

