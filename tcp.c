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

#include "bsock.h"
#include "dsock.h"
#include "fd.h"
#include "utils.h"

static int tcpmakeconn(int fd);

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

static const int tcp_conn_type_placeholder = 0;
static const void *tcp_conn_type = &tcp_conn_type_placeholder;
static void tcp_conn_close(int s);
static int tcp_conn_bsendmsg(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);
static int tcp_conn_brecvmsg(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);

struct tcp_conn {
    struct bsock_vfptrs vfptrs;
    int fd;
    struct fdrxbuf rxbuf;
};

int tcp_connect(const ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fdunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fdconnect(s, ipaddr_sockaddr(addr), ipaddr_len(addr), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(s);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fdclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int tcp_conn_bsendmsg(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct tcp_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcp_conn_type);
    ssize_t sz = fdsend(obj->fd, iov, iovlen, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int tcp_conn_brecvmsg(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct tcp_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcp_conn_type);
    return fdrecv(obj->fd, &obj->rxbuf, iov, iovlen, deadline);
}

static void tcp_conn_close(int s) {
    struct tcp_conn *obj = hdata(s, bsock_type);
    dsock_assert(obj);
    int rc = fdclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  TCP listener socket                                                       */
/******************************************************************************/

static const int tcp_listener_type_placeholder = 0;
static const void *tcp_listener_type = &tcp_listener_type_placeholder;
static void tcp_listener_close(int s);
static const struct hvfptrs tcp_listener_vfptrs = {tcp_listener_close};

struct tcp_listener {
    struct hvfptrs vfptrs;
    int fd;
    ipaddr addr;
};

int tcp_listen(ipaddr *addr, int backlog) {
    int err;
    /* Open the listening socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fdunblock(s);
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
    obj->fd = s;
    /* Create handle. */
    int h = handle(tcp_listener_type, obj, &tcp_listener_vfptrs);
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
    struct tcp_listener *lst = hdata(s, tcp_listener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(ipaddr);
    int as = fdaccept(lst->fd, (struct sockaddr*)addr, &addrlen, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fdunblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(as);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fdclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static void tcp_listener_close(int s) {
    struct tcp_listener *obj = hdata(s, tcp_listener_type);
    dsock_assert(obj);
    int rc = fdclose(obj->fd);
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
    obj->vfptrs.hvfptrs.close = tcp_conn_close;
    obj->vfptrs.type = tcp_conn_type;
    obj->vfptrs.bsendmsg = tcp_conn_bsendmsg;
    obj->vfptrs.brecvmsg = tcp_conn_brecvmsg;
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

