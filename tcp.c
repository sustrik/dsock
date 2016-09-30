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
#include "fdhelpers.h"
#include "utils.h"

static int tcpmakeconn(int fd);

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

static const int tcpconn_type_placeholder = 0;
static const void *tcpconn_type = &tcpconn_type_placeholder;
static void tcpconn_close(int s);
static int tcpconn_bsend(int s, const void *buf, size_t len, int64_t deadline);
static int tcpconn_brecv(int s, void *buf, size_t len, int64_t deadline);

struct tcpconn {
    struct bsockvfptrs vfptrs;
    int fd;
    struct dsrxbuf rxbuf;
};

int tcp_connect(const ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = dsconnect(s, ipaddr_sockaddr(addr), ipaddr_len(addr), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(s);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int tcpconn_bsend(int s, const void *buf, size_t len, int64_t deadline) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcpconn_type);
    ssize_t sz = dssend(obj->fd, buf, len, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int tcpconn_brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcpconn_type);
    return dsrecv(obj->fd, &obj->rxbuf, buf, len, deadline);
}

static void tcpconn_close(int s) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj);
    int rc = dsclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  TCP listener socket                                                       */
/******************************************************************************/

static const int tcplistener_type_placeholder = 0;
static const void *tcplistener_type = &tcplistener_type_placeholder;
static void tcplistener_close(int s);
static const struct hvfptrs tcplistener_vfptrs = {tcplistener_close};

struct tcplistener {
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
    int rc = dsunblock(s);
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
    struct tcplistener *obj = malloc(sizeof(struct tcplistener));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->fd = s;
    /* Create handle. */
    int h = handle(tcplistener_type, obj, &tcplistener_vfptrs);
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
    struct tcplistener *lst = hdata(s, tcplistener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(ipaddr);
    int as = dsaccept(lst->fd, (struct sockaddr*)addr, &addrlen, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcpmakeconn(as);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static void tcplistener_close(int s) {
    struct tcplistener *obj = hdata(s, tcplistener_type);
    dsock_assert(obj);
    int rc = dsclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int tcpmakeconn(int fd) {
    int err;
    /* Create the object. */
    struct tcpconn *obj = malloc(sizeof(struct tcpconn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = tcpconn_close;
    obj->vfptrs.type = tcpconn_type;
    obj->vfptrs.bsend = tcpconn_bsend;
    obj->vfptrs.brecv = tcpconn_brecv;
    obj->fd = fd;
    dsinitrxbuf(&obj->rxbuf);
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

