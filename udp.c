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
#include <unistd.h>

#include "msock.h"
#include "dsock.h"
#include "fd.h"
#include "utils.h"

DSOCK_UNIQUE_ID(udp_type);

static void udp_close(int s);
static int udp_msendv(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);
static ssize_t udp_mrecvv(int s, const struct iovec *iov, size_t iovlen,
    int64_t deadline);

struct udp_sock {
    struct msock_vfptrs vfptrs;
    int fd;
    int hasremote;
    ipaddr remote;
};

int udp_socket(ipaddr *local, const ipaddr *remote) {
    int err;
    /* Sanity checking. */
    if(dsock_slow(local && remote &&
          ipaddr_family(local) != ipaddr_family(remote))) {
        err = EINVAL; goto error1;}
    /* Open the listening socket. */
    int family = AF_INET;
    if(local) family = ipaddr_family(local);
    if(remote) family = ipaddr_family(remote);
    int s = socket(family, SOCK_DGRAM, 0);
    if(s < 0) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening. */
    if(local) {
        rc = bind(s, ipaddr_sockaddr(local), ipaddr_len(local));
        if(s < 0) {err = errno; goto error2;}
        /* Get the ephemeral port number. */
        if(ipaddr_port(local) == 0) {
            ipaddr baddr;
            socklen_t len = sizeof(ipaddr);
            rc = getsockname(s, (struct sockaddr*)&baddr, &len);
            if(dsock_slow(rc < 0)) {err = errno; goto error2;}
            ipaddr_setport(local, ipaddr_port(&baddr));
        }
    }
    /* Create the object. */
    struct udp_sock *obj = malloc(sizeof(struct udp_sock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->vfptrs.hvfptrs.close = udp_close;
    obj->vfptrs.type = udp_type;
    obj->vfptrs.msendv = udp_msendv;
    obj->vfptrs.mrecvv = udp_mrecvv;
    obj->fd = s;
    obj->hasremote = remote ? 1 : 0;
    if(remote) obj->remote = *remote;
    /* Create the handle. */
    int h = handle(msock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(obj);
error2:
    rc = fd_close(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int udp_sendv(int s, const ipaddr *addr, const struct iovec *iov,
      size_t iovlen) {
    struct udp_sock *obj = hdata(s, msock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != udp_type)) {errno = ENOTSUP; return -1;}
    /* If no destination IP address is provided, fall back to the stored one. */
    const ipaddr *dstaddr = addr;
    if(!dstaddr) {
        if(dsock_slow(!obj->hasremote)) {errno = EINVAL; return -1;}
        dstaddr = &obj->remote;
    }
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_name = (void*)ipaddr_sockaddr(dstaddr);
    hdr.msg_namelen = ipaddr_len(dstaddr);
    hdr.msg_iov = (struct iovec*)iov;
    hdr.msg_iovlen = iovlen;
    ssize_t sz = sendmsg(obj->fd, &hdr, 0);
    if(dsock_fast(sz >= 0)) return 0;
    if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

ssize_t udp_recvv(int s, ipaddr *addr, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    struct udp_sock *obj = hdata(s, msock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != udp_type)) {errno = ENOTSUP; return -1;}
    while(1) {
        struct msghdr hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.msg_name = (void*)addr;
        hdr.msg_namelen = sizeof(ipaddr);
        hdr.msg_iov = (struct iovec*)iov;
        hdr.msg_iovlen = iovlen;
        ssize_t sz = recvmsg(obj->fd, &hdr, 0);
        if(sz >= 0) return sz;
        if(errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        int rc = fdin(obj->fd, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
}

int udp_send(int s, const ipaddr *addr, const void *buf, size_t len) {
    struct iovec iov = {(void*)buf, len};
    return udp_sendv(s, addr, &iov, 1);
}

ssize_t udp_recv(int s, ipaddr *addr, void *buf, size_t len, int64_t deadline) {
    struct iovec iov = {(void*)buf, len};
    return udp_recvv(s, addr, &iov, 1, deadline);
}

static int udp_msendv(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    return udp_sendv(s, NULL, iov, iovlen);
}

static ssize_t udp_mrecvv(int s, const struct iovec *iov, size_t iovlen,
      int64_t deadline) {
    return udp_recvv(s, NULL, iov, iovlen, deadline);
}

static void udp_close(int s) {
    struct udp_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == udp_type);
    int rc = fd_close(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

