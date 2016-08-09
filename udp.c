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

#include "msock.h"
#include "dsock.h"
#include "helpers.h"
#include "utils.h"

static const int udp_type_placeholder = 0;
static const void *udp_type = &udp_type_placeholder;
static void udp_close(int s);
static int udp_msend(int s, const void *buf, size_t *len, int64_t deadline);
static int udp_mrecv(int s, void *buf, size_t *len, int64_t deadline);

struct udpsock {
    struct msockvfptrs vfptrs;
    int fd;
    int hasremote;
    ipaddr remote;
};

int udpsocket(ipaddr *local, const ipaddr *remote) {
    int err;
    /* Sanity checking. */
    if(dsock_slow(local && remote && ipfamily(local) != ipfamily(remote))) {
        err = EINVAL; goto error1;}
    /* Open the listening socket. */
    int family = AF_INET;
    if(local) family = ipfamily(local);
    if(remote) family = ipfamily(remote);
    int s = socket(family, SOCK_DGRAM, 0);
    if(s < 0) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dsunblock(s);
    if(dsock_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening. */
    if(local) {
        rc = bind(s, ipsockaddr(local), iplen(local));
        if(s < 0) {err = errno; goto error2;}
        /* Get the ephemeral port number. */
        if(ipport(local) == 0) {
            ipaddr baddr;
            socklen_t len = sizeof(ipaddr);
            rc = getsockname(s, (struct sockaddr*)&baddr, &len);
            if(dsock_slow(rc < 0)) {err = errno; goto error2;}
            ipsetport(local, ipport(&baddr));
        }
    }
    /* Create the object. */
    struct udpsock *obj = malloc(sizeof(struct udpsock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->vfptrs.hvfptrs.close = udp_close;
    obj->vfptrs.type = udp_type;
    obj->vfptrs.msend = udp_msend;
    obj->vfptrs.mrecv = udp_mrecv;
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
    rc = dsclose(s);
    dsock_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int udpsend(int s, const ipaddr *addr, const void *buf, size_t *len) {
    struct udpsock *obj = hdata(s, msock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != udp_type)) {
        errno = EOPNOTSUPP; return -1;}
    if(dsock_slow(!len || (*len > 0 && !buf))) {errno = EINVAL; return -1;}
    /* If no destination IP address is provided, fall back to the stored one. */
    const ipaddr *dstaddr = addr;
    if(!dstaddr) {
        if(dsock_slow(!obj->hasremote)) {errno = EINVAL; return -1;}
        dstaddr = &obj->remote;
    }
    ssize_t sz = sendto(obj->fd, buf, *len, 0,
        (struct sockaddr*)ipsockaddr(dstaddr), iplen(dstaddr));
    if(dsock_fast(sz == *len)) return 0;
    dsock_assert(sz < 0);
    if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

int udprecv(int s, ipaddr *addr, void *buf, size_t *len, int64_t deadline) {
    struct udpsock *obj = hdata(s, msock_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->vfptrs.type != udp_type)) {
        errno = EOPNOTSUPP; return -1;}
    if(dsock_slow(!len || (*len > 0 && !buf))) {errno = EINVAL; return -1;}
    ssize_t sz;
    while(1) {
        socklen_t slen = sizeof(ipaddr);
        sz = recvfrom(obj->fd, buf, *len, 0, (struct sockaddr*)addr, &slen);
        if(sz >= 0) {
            *len = sz;
            return 0;
        }
        if(errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        int rc = fdin(obj->fd, deadline);
        if(dsock_slow(rc < 0)) return -1;
    }
}

static int udp_msend(int s, const void *buf, size_t *len, int64_t deadline) {
    return udpsend(s, NULL, buf, len);
}

static int udp_mrecv(int s, void *buf, size_t *len, int64_t deadline) {
    return udprecv(s, NULL, buf, len, deadline);
}

static void udp_close(int s) {
    struct udpsock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == udp_type);
    int rc = dsclose(obj->fd);
    dsock_assert(rc == 0);
    free(obj);
}

