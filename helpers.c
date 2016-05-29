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

#include <fcntl.h>
#include <libdill.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "dillsocks.h"
#include "utils.h"

int dsunblock(int s) {
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    return rc >= 0 ? 0 : -1;
}

int dsconnect(int s, const struct sockaddr *addr, socklen_t addrlen,
      int64_t deadline) {
    int err;
    /* Initiate connect. */
    int rc = connect(s, addr, addrlen);
    if(rc == 0) return 0;
    if(dill_slow(errno != EINPROGRESS)) {err = errno; goto error;}
    /* Connect is in progress. Let's wait till it's done. */
    rc = fdout(s, deadline);
    if(dill_slow(rc == -1)) {err = errno; goto error;}
    /* Retrieve the error from the socket, if any. */
    socklen_t errsz = sizeof(err);
    rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
    if(dill_slow(rc != 0)) {err = errno; goto error;}
    if(dill_slow(err != 0)) goto error;
    return 0;
error:
    fdclean(s);
    close(s);
    errno = err;
    return -1;
}

int dsaccept(int s, struct sockaddr *addr, socklen_t *addrlen,
      int64_t deadline) {
    int as;
    while(1) {
        /* Try to accept new connection synchronously. */
        as = accept(s, addr, addrlen);
        if(dill_fast(as >= 0))
            break;
        if(dill_slow(errno != EAGAIN && errno != EWOULDBLOCK)) return -1;
        /* Operation is in progress. Wait till new connection is available. */
        int rc = fdin(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
    int rc = dsunblock(as);
    dill_assert(rc == 0);
    return as;
}

int dssend(int s, const void *buf, size_t *len, int64_t deadline) {
    size_t sent = 0;
    while(sent < *len) {
        ssize_t sz = send(s, ((char*)buf) + sent, *len - sent, 0);
        if(sz < 0) {
            if(dill_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                *len = sent;
                return -1;
            }
            int rc = fdout(s, deadline);
            if(dill_slow(rc < 0)) return -1;
            continue;
        }
        sent += sz;
    }
    return 0;
}

int dsrecv(int s, void *buf, size_t *len, int64_t deadline) {
    size_t received = 0;
    while(1) {
        ssize_t sz = recv(s, ((char*)buf) + received, *len - received, 0);
        if(dill_slow(sz == 0)) {
            *len = received;
            errno = ECONNRESET;
            return -1;
        }
        if(sz < 0) {
            if(dill_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                *len = received;
                return -1;
            }
        }
        else {
            received += sz;
            if(received >= *len)
                return 0;
        }
        int rc = fdin(s, deadline);
        if(dill_slow(rc < 0)) {
            *len = received;
            return -1;
        }
    }
}

int dsclose(int s) {
    fdclean(s);
    return close(s);
}

