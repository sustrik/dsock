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
#include "socket_helpers.h"
#include "utils.h"

static int unixresolve(const char *addr, struct sockaddr_un *su);
static int unixmakeconn(int fd);

/******************************************************************************/
/*  UNIX connection socket                                                    */
/******************************************************************************/

static const int unixconn_type_placeholder = 0;
static const void *unixconn_type = &unixconn_type_placeholder;
static void unixconn_close(int s);
static int unixconn_send(int s, const void *buf, size_t len, int64_t deadline);
static int unixconn_recv(int s, void *buf, size_t len, int64_t deadline);

struct unixconn {
    struct bsockvfptrs vfptrs;
    int fd;
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
    rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = dsconnect(s, (struct sockaddr*)&su, sizeof(su), deadline);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = unixmakeconn(s);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int unix_attach(int fd) {
    if(dsock_slow(fd < 0)) {errno = EINVAL; return -1;}
    /* Set the socket to non-blocking mode. */
    int rc = dsunblock(fd);
    if(dsock_slow(rc < 0)) return -1;
    /* Create the handle. */
    int h = unixmakeconn(fd);
    if(dsock_slow(h < 0)) return -1;
    return h;
}

int unix_detach(int s) {
    struct unixconn *obj = hdata(s, bsock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != unixconn_type)) {
        errno = ENOTSUP; return -1;}
    int fd = obj->fd;
    free(obj);
    return fd;
}

static int unixconn_send(int s, const void *buf, size_t len, int64_t deadline) {
    struct unixconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == unixconn_type);
    ssize_t sz = dssend(obj->fd, buf, len, deadline);
    if(dsock_fast(sz >= 0)) return sz;
    if(errno == EPIPE) errno = ECONNRESET;
    return -1;
}

static int unixconn_recv(int s, void *buf, size_t len, int64_t deadline) {
    struct unixconn *obj = hdata(s, bsock_type);
    dsock_assert(obj->vfptrs.type == unixconn_type);
    return dsrecv(obj->fd, buf, len, deadline);
}

int unix_sendfd(int s, int fd, int64_t deadline) {
    struct unixconn *obj = hdata(s, bsock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != unixconn_type)) {
        errno = ENOTSUP; return -1;}
    if(dsock_slow(fd < 0)) {errno = EINVAL; return -1;}
    /* Prepare the message. */
    struct iovec iov;
    unsigned char buf[] = {0};
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    char control[sizeof(struct cmsghdr) + 10];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    struct cmsghdr *cmsg;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    *((int*)CMSG_DATA(cmsg)) = fd;
    msg.msg_controllen = cmsg->cmsg_len;
    /* Try to send it. */
    while(1) {
        ssize_t sz = sendmsg(obj->fd, &msg, DSOCK_NOSIGNAL);
        if(dsock_fast(sz == 1)) return 0;
        dsock_assert(sz < 0);
        if(dsock_slow(errno != EWOULDBLOCK && errno != EAGAIN)) return -1;
        int rc = fdout(obj->fd, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
}

int unix_recvfd(int s, int64_t deadline) {
    struct unixconn *obj = hdata(s, bsock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != unixconn_type)) {
        errno = ENOTSUP; return -1;}
    /* Read one byte along with the ancillary data. */
    char buf[1];
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    unsigned char control[1024];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    while(1) {
        ssize_t sz = recvmsg(obj->fd, &msg, 0);
        if(dsock_fast(sz == 1)) break;
        if(dsock_slow(sz == 0)) {errno = ECONNRESET; return -1;}
        dsock_assert(sz < 0);
        if(dsock_slow(errno != EWOULDBLOCK && errno != EAGAIN)) return -1;
        int rc = fdin(obj->fd, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }    
    /* Loop through the auxiliary data to find the embedded file descriptor. */
    int fd = -1;
    struct cmsghdr *cmsg;
    for(cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type  == SCM_RIGHTS) {
            /* We don't expect multiple fds here and we ignore all of them
               except for the first one. This can result in fd leaks. */
            fd = *(int*)CMSG_DATA(cmsg);
            break;
        }
    }
    return fd;
}

static void unixconn_close(int s) {
    struct unixconn *obj = hdata(s, bsock_type);
    dsock_assert(obj);
    int rc = dsclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  UNIX listener socket                                                      */
/******************************************************************************/

static const int unixlistener_type_placeholder = 0;
static const void *unixlistener_type = &unixlistener_type_placeholder;
static void unixlistener_close(int s);
static const struct hvfptrs unixlistener_vfptrs = {unixlistener_close};

struct unixlistener {
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
    rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, (struct sockaddr*)&su, sizeof(su));
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the object. */
    struct unixlistener *obj = malloc(sizeof(struct unixlistener));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->fd = s;
    /* Create handle. */
    int h = handle(unixlistener_type, obj, &unixlistener_vfptrs);
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
    struct unixlistener *lst = hdata(s, unixlistener_type);
    if(dsock_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    int as = dsaccept(lst->fd, NULL, NULL, deadline);
    if(dsock_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(as);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = unixmakeconn(as);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static void unixlistener_close(int s) {
    struct unixlistener *obj = hdata(s, unixlistener_type);
    dsock_assert(obj);
    int rc = dsclose(obj->fd);
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
    rc = dsunblock(fds[0]);
    if(dsock_slow(rc < 0)) {err = errno; goto error3;}
    rc = dsunblock(fds[1]);
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
    rc = dsclose(fds[0]);
    dsock_assert(rc == 0);
error2:
    rc = dsclose(fds[1]);
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
    struct unixconn *obj = malloc(sizeof(struct unixconn));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = unixconn_close;
    obj->vfptrs.type = unixconn_type;
    obj->vfptrs.bsend = unixconn_send;
    obj->vfptrs.brecv = unixconn_recv;
    obj->fd = fd;
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

