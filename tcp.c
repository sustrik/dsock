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
#include <unistd.h>

#include "dsockimpl.h"
#include "fd.h"
#include "utils.h"
#include "tcp.h"

static int tcpmakeconn(int fd);

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

dsock_unique_id(tcp_type);

static void *tcp_hquery(struct hvfs *hvfs, const void *type);
static void tcp_hclose(struct hvfs *hvfs);
static int tcp_bsendv(struct bsock_vfs *bvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static int tcp_brecvv(struct bsock_vfs *bvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

struct tcp_conn {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    int fd;
    struct fd_rxbuf rxbuf;
};

static void *tcp_hquery(struct hvfs *hvfs, const void *type) {
    struct tcp_conn *obj = (struct tcp_conn*)hvfs;
    if(type == bsock_type) return &obj->bvfs;
    if(type == tcp_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int tcp_connect(const ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fd_connect(s, ipaddr_sockaddr(addr), ipaddr_len(addr), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(s);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fd_close(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int tcp_bsendv(struct bsock_vfs *bvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct tcp_conn *obj = dsock_cont(bvfs, struct tcp_conn, bvfs);
    ssize_t sz = fd_send(obj->fd, iov, iovlen, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int tcp_brecvv(struct bsock_vfs *bvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct tcp_conn *obj = dsock_cont(bvfs, struct tcp_conn, bvfs);
    return fd_recv(obj->fd, &obj->rxbuf, iov, iovlen, deadline);
}

int tcp_done(int s, int64_t deadline) {
    /* Deadline in in the prototype because flushing TCP data is a potentially
       blocking operation. However, POSIX flushes the data in asynchronous
       manner, thus deadline in never used. It may become handy in user-space
       implementations of TCP though. */
    struct tcp_conn *obj = hquery(s, tcp_type);
    if(dsock_slow(!obj)) return -1;
    int rc = shutdown(obj->fd, SHUT_WR);
    dsock_assert(rc == 0);
    return 0;
}

static void tcp_hclose(struct hvfs *hvfs) {
    struct tcp_conn *obj = (struct tcp_conn*)hvfs;
    int rc = fd_close(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  TCP listener socket                                                       */
/******************************************************************************/

dsock_unique_id(tcp_listener_type);

static void *tcp_listener_hquery(struct hvfs *hvfs, const void *type);
static void tcp_listener_hclose(struct hvfs *hvfs);

struct tcp_listener {
    struct hvfs hvfs;
    int fd;
    ipaddr addr;
};

static void *tcp_listener_hquery(struct hvfs *hvfs, const void *type) {
    struct tcp_listener *obj = (struct tcp_listener*)hvfs;
    if(type == tcp_listener_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int tcp_listen(ipaddr *addr, int backlog) {
    int err;
    /* Open the listening socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, ipaddr_sockaddr(addr), ipaddr_len(addr));
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    if(ipaddr_port(addr) == 0) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc < 0) {err = errno; goto error2;}
        ipaddr_setport(addr, ipaddr_port(&baddr));
    }
    /* Create the object. */
    struct tcp_listener *obj = malloc(sizeof(struct tcp_listener));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->hvfs.query = tcp_listener_hquery;
    obj->hvfs.close = tcp_listener_hclose;
    obj->fd = s;
    /* Create handle. */
    int h = hmake(&obj->hvfs);
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

int tcp_accept(int s, ipaddr *addr, int64_t deadline) {
    int err;
    /* Retrieve the listener object. */
    struct tcp_listener *lst = hquery(s, tcp_listener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(ipaddr);
    int as = fd_accept(lst->fd, (struct sockaddr*)addr, &addrlen, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(as);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fd_close(as);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int tcp_fd(int s) {
    struct tcp_listener *lst = hquery(s, tcp_listener_type);
    if(lst) return lst->fd;
    struct tcp_conn *conn = hquery(s, tcp_type);
    if(conn) return conn->fd;
    return -1;
}

static void tcp_listener_hclose(struct hvfs *hvfs) {
    struct tcp_listener *obj = (struct tcp_listener*)hvfs;
    int rc = fd_close(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int tcpmakeconn(int fd) {
    int err;
    /* Create the object. */
    struct tcp_conn *obj = malloc(sizeof(struct tcp_conn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->hvfs.query = tcp_hquery;
    obj->hvfs.close = tcp_hclose;
    obj->bvfs.bsendv = tcp_bsendv;
    obj->bvfs.brecvv = tcp_brecvv;
    obj->fd = fd;
    fd_initrxbuf(&obj->rxbuf);
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

