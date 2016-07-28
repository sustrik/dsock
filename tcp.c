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
#include "helpers.h"
#include "utils.h"

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

static const int tcpconn_type_placeholder = 0;
static const void *tcpconn_type = &tcpconn_type_placeholder;
static void tcpconn_close(int s);
static ssize_t tcpconn_bsend(int s, const void *buf, size_t len,
    int64_t deadline);
static int tcpconn_bflush(int s, int64_t deadline);
static ssize_t tcpconn_brecv(int s, void *buf, size_t len, int64_t deadline);

struct tcpconn {
    struct bsockvfptrs vfptrs;
    int fd;
};

int tcpconnect(const ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = dsconnect(s, ipsockaddr(addr), iplen(addr), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the object. */
    struct tcpconn *obj = malloc(sizeof(struct tcpconn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->vfptrs.hvfptrs.close = tcpconn_close;
    obj->vfptrs.type = tcpconn_type;
    obj->vfptrs.bsend = tcpconn_bsend;
    obj->vfptrs.bflush = tcpconn_bflush;
    obj->vfptrs.brecv = tcpconn_brecv;
    obj->fd = s;
    /* Create the handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(obj);
error2:
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static ssize_t tcpconn_bsend(int s, const void *buf, size_t len,
      int64_t deadline) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcpconn_type);
    ssize_t sz = dssend(obj->fd, buf, len, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int tcpconn_bflush(int s, int64_t deadline) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcpconn_type);
    return 0;
}

static ssize_t tcpconn_brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct tcpconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == tcpconn_type);
    return dsrecv(obj->fd, buf, len, deadline);
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
    struct bsockvfptrs vfptrs;
    int fd;
    ipaddr addr;
};

int tcplisten(const ipaddr *addr, int backlog) {
    int err;
    /* Open the listening socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(dsock_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, ipsockaddr(addr), iplen(addr));
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    int port = ipport(addr);
    if(!port) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc < 0) {err = errno; goto error2;}
        port = ipport(&baddr);
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

int tcpaccept(int s, int64_t deadline) {
    int err;
    /* Retrieve the listener object. */
    struct tcplistener *lst = hdata(s, tcplistener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    ipaddr addr;
    socklen_t addrlen;
    int as = dsaccept(lst->fd, (struct sockaddr*)&addr, &addrlen, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the object. */
    struct tcpconn *obj = malloc(sizeof(struct tcpconn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->vfptrs.hvfptrs.close = tcpconn_close;
    obj->vfptrs.type = tcpconn_type;
    obj->vfptrs.bsend = tcpconn_bsend;
    obj->vfptrs.bflush = tcpconn_bflush;
    obj->vfptrs.brecv = tcpconn_brecv;
    obj->fd = as;
    /* Create handle. */
    int h = handle(bsock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(obj);
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
/*  Function shared by TCP connection and listener sockets                    */
/******************************************************************************/

int tcpaddr(int s, ipaddr *addr) {
    dsock_assert(0);
}

