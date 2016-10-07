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
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "bsock.h"
#include "dsock.h"
#include "fd.h"
#include "utils.h"

static int unixresolve(const char *addr, struct sockaddr_un *su);
static int unixmakeconn(int fd);

/******************************************************************************/
/*  UNIX connection socket                                                    */
/******************************************************************************/

static const int unix_conn_type_placeholder = 0;
static const void *unix_conn_type = &unix_conn_type_placeholder;
static void unix_conn_close(int s);
static int unix_conn_bsendmsg(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);
static int unix_conn_brecvmsg(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);

struct unix_conn {
    struct bsock_vfptrs vfptrs;
    int fd;
    struct fdrxbuf rxbuf;
};

int unix_connect(const char *addr, int64_t deadline) {
    int err;
    /* Create a UNIX address out of the address string. */
    struct sockaddr_un su;
    int rc = unixresolve(addr, &su);
    if(rc < 0) {err = errno; goto error1;}
    /* Open a socket. */
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    rc = fdunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fdconnect(s, (struct sockaddr*)&su, sizeof(su), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = unixmakeconn(s);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fdclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int unix_conn_bsendmsg(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct unix_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == unix_conn_type);
    ssize_t sz = fdsend(obj->fd, iov, iovlen, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int unix_conn_brecvmsg(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct unix_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == unix_conn_type);
    return fdrecv(obj->fd, &obj->rxbuf, iov, iovlen, deadline);
}

static void unix_conn_close(int s) {
    struct unix_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj);
    int rc = fdclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  UNIX listener socket                                                      */
/******************************************************************************/

static const int unix_listener_type_placeholder = 0;
static const void *unix_listener_type = &unix_listener_type_placeholder;
static void unix_listener_close(int s);
static const struct hvfptrs unix_listener_vfptrs = {unix_listener_close};

struct unix_listener {
    struct hvfptrs vfptrs;
    int fd;
};

int unix_listen(const char *addr, int backlog) {
    int err;
    /* Create a UNIX address out of the address string. */
    struct sockaddr_un su;
    int rc = unixresolve(addr, &su);
    if(rc < 0) {err = errno; goto error1;}
    /* Open the listening socket. */
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    rc = fdunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, (struct sockaddr*)&su, sizeof(su));
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the object. */
    struct unix_listener *obj = malloc(sizeof(struct unix_listener));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->fd = s;
    /* Create handle. */
    int h = handle(unix_listener_type, obj, &unix_listener_vfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(obj);
error2:
    close(s);
error1:
    errno = err;
    return -1;
}

int unix_accept(int s, int64_t deadline) {
    int err;
    /* Retrieve the listener object. */
    struct unix_listener *lst = hdata(s, unix_listener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    int as = fdaccept(lst->fd, NULL, NULL, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fdunblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = unixmakeconn(as);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fdclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static void unix_listener_close(int s) {
    struct unix_listener *obj = hdata(s, unix_listener_type);
    dsock_assert(obj);
    int rc = fdclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  UNIX pair                                                                 */
/******************************************************************************/

int unix_pair(int s[2]) {
    int err;
    /* Create the pair. */
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if(rc < 0) {err = errno; goto error1;}
    /* Set the sockets to non-blocking mode. */
    rc = fdunblock(fds[0]);
    if(dsock_slow(rc < 0)) {err = errno; goto error3;}
    rc = fdunblock(fds[1]);
    if(dsock_slow(rc < 0)) {err = errno; goto error3;}
    /* Create the handles. */
    s[0] = unixmakeconn(fds[0]);
    if(dsock_slow(s[0] < 0)) {err = errno; goto error3;}
    s[1] = unixmakeconn(fds[1]);
    if(dsock_slow(s[1] < 0)) {err = errno; goto error4;}
    return 0;
error4:
    rc = hclose(s[0]);
    goto error2;
error3:
    rc = fdclose(fds[0]);
    dsock_assert(rc == 0);
error2:
    rc = fdclose(fds[1]);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int unixresolve(const char *addr, struct sockaddr_un *su) {
    dsock_assert(su);
    if(strlen(addr) >= sizeof(su->sun_path)) {errno = ENAMETOOLONG; return -1;}
    su->sun_family = AF_UNIX;
    strncpy(su->sun_path, addr, sizeof(su->sun_path));
    return 0;
}

static int unixmakeconn(int fd) {
    int err;
    /* Create the object. */
    struct unix_conn *obj = malloc(sizeof(struct unix_conn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = unix_conn_close;
    obj->vfptrs.type = unix_conn_type;
    obj->vfptrs.bsendmsg = unix_conn_bsendmsg;
    obj->vfptrs.brecvmsg = unix_conn_brecvmsg;
    obj->fd = fd;
    fdinitrxbuf(&obj->rxbuf);
    /* Create the handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

