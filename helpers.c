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

#include <fcntl.h>
#include <libdill.h>
#include <unistd.h>

#include "helpers.h"
#include "utils.h"

int dsunblock(int s) {
    /* Switch to non-blocking mode. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    dsock_assert(rc == 0);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    dsock_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    dsock_assert (rc == 0 || errno == EINVAL);
#endif

}

int dsconnect(int s, const struct sockaddr *addr, socklen_t addrlen,
      int64_t deadline) {
    /* Initiate connect. */
    int rc = connect(s, addr, addrlen);
    if(rc == 0) return 0;
    if(dsock_slow(errno != EINPROGRESS)) return -1;
    /* Connect is in progress. Let's wait till it's done. */
    rc = fdout(s, deadline);
    if(dsock_slow(rc == -1)) return -1;
    /* Retrieve the error from the socket, if any. */
    int err = 0;
    socklen_t errsz = sizeof(err);
    rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
    if(dsock_slow(rc != 0)) return -1;
    if(dsock_slow(err != 0)) {errno = err; return -1;}
    return 0;
}

int dsaccept(int s, struct sockaddr *addr, socklen_t *addrlen,
      int64_t deadline) {
    int as;
    while(1) {
        /* Try to accept new connection synchronously. */
        as = accept(s, addr, addrlen);
        if(dsock_fast(as >= 0))
            break;
        if(dsock_slow(errno != EAGAIN && errno != EWOULDBLOCK)) return -1;
        /* Operation is in progress. Wait till new connection is available. */
        int rc = fdin(s, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
    int rc = dsunblock(as);
    dsock_assert(rc == 0);
    return as;
}

int dssend(int s, const void *buf, size_t len, int64_t deadline) {
    if(dsock_slow(len > 0 && !buf)) {errno = EINVAL; return -1;}
    ssize_t sent = 0;
    while(sent < len) {
        ssize_t sz = send(s, ((char*)buf) + sent, len - sent, DSOCK_NOSIGNAL);
        if(sz < 0) {
            if(dsock_slow(errno != EWOULDBLOCK && errno != EAGAIN)) return -1;
            int rc = fdout(s, deadline);
            if(dsock_slow(rc < 0)) return -1;
            continue;
        }
        sent += sz;
    }
    return 0;
}

int dsrecv(int s, void *buf, size_t len, int64_t deadline) {
    if(dsock_slow(len > 0 && !buf)) {errno = EINVAL; return -1;}
    ssize_t received = 0;
    while(1) {
        ssize_t sz = recv(s, ((char*)buf) + received, len - received, 0);
        if(dsock_slow(sz == 0)) {
            errno = ECONNRESET;
            return -1;
        }
        if(sz < 0) {
            if(dsock_slow(errno != EWOULDBLOCK && errno != EAGAIN))
                return -1;
        }
        else {
            received += sz;
            if(received >= len) return 0;
        }
        int rc = fdin(s, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
}

int dsclose(int s) {
    fdclean(s);
    return close(s);
}

