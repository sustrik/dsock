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

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "buf.h"
#include "debug.h"
#include "ip.h"
#include "dillsocks.h"

static void dill_tcp_brecv(sock s, void *buf, size_t len,
    int64_t deadline);
static void dill_tcp_bsend(sock s, const void *buf, size_t len,
    int64_t deadline);
static void dill_tcp_bflush(sock s, int64_t deadline);

static struct dill_sock_vfptr dill_tcp_listener_vfptr = {
    NULL,
    NULL,
    NULL};

static struct dill_sock_vfptr dill_tcp_conn_vfptr = {
    dill_tcp_brecv,
    dill_tcp_bsend,
    dill_tcp_bflush};

struct dill_tcp_listener {
    struct dill_sock sock;
    int fd;
    int port;
};

struct dill_tcp_conn {
    struct dill_sock sock;
    int fd;
    ipaddr addr;
    struct dill_buf ibuf;
    struct dill_buf obuf;
};

static void dill_tcp_tune(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    assert (rc == 0 || errno == EINVAL);
#endif
}

static void dill_tcp_conn_init(struct dill_tcp_conn *c, int fd) {
    c->sock.vfptr = &dill_tcp_conn_vfptr;
    c->fd = fd;
    dill_buf_init(&c->ibuf);
    dill_buf_init(&c->obuf);
}

sock tcp_listen(ipaddr addr, int backlog) {
    /* Open the listening socket. */
    int s = socket(dill_ipfamily(&addr), SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    dill_tcp_tune(s);

    /* Start listening. */
    int rc = bind(s, dill_ipsockaddr(&addr), dill_iplen(&addr));
    if(rc != 0)
        return NULL;
    rc = listen(s, backlog);
    if(rc != 0)
        return NULL;

    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS now. */
    int port = dill_ipport(&addr);
    if(!port == 0) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, dill_ipsockaddr(&baddr), &len);
        if(rc == -1) {
            int err = errno;
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
        port = dill_ipport(&baddr);
    }

    /* Create the object. */
    struct dill_tcp_listener *l = malloc(sizeof(struct dill_tcp_listener));
    if(!l) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    l->sock.vfptr = &dill_tcp_listener_vfptr;
    l->fd = s;
    l->port = port;
    errno = 0;
    return &l->sock;
}

ipaddr tcp_addr(sock s) {
    if(s->vfptr != &dill_tcp_conn_vfptr) {
        assert(0);
    }  
    struct dill_tcp_conn *l = (struct dill_tcp_conn*)s;
    return l->addr;
}

int tcp_port(sock s) {
    if(s->vfptr == &dill_tcp_conn_vfptr) {
        struct dill_tcp_conn *c = (struct dill_tcp_conn*)s;
        return dill_ipport(&c->addr);
    }
    else if(s->vfptr == &dill_tcp_listener_vfptr) {
        struct dill_tcp_listener *l = (struct dill_tcp_listener*)s;
        return l->port;
    }
    errno = EPROTOTYPE;
    return -1;
}

sock tcp_accept(sock s, int64_t deadline) {
    if(s->vfptr != &dill_tcp_listener_vfptr) {errno = EPROTOTYPE; return NULL;}
    struct dill_tcp_listener *l = (struct dill_tcp_listener*)s;
    socklen_t addrlen;
    ipaddr addr;
    while(1) {
        /* Try to get new connection (non-blocking). */
        addrlen = sizeof(addr);
        int as = accept(l->fd, dill_ipsockaddr(&addr), &addrlen);
        if (as >= 0) {
            dill_tcp_tune(as);
            struct dill_tcp_conn *conn = malloc(sizeof(struct dill_tcp_conn));
            if(!conn) {
                fdclean(as);
                close(as);
                errno = ENOMEM;
                return NULL;
            }
            dill_tcp_conn_init(conn, as);
            conn->addr = addr;
            errno = 0;
            return &conn->sock;
        }
        assert(as == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(l->fd, FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return NULL;
        }
        assert(rc == FDW_IN);
    }
}

sock tcp_connect(ipaddr addr, int64_t deadline) {
    /* Open a socket. */
    int s = socket(dill_ipfamily(&addr), SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    dill_tcp_tune(s);

    /* Connect to the remote endpoint. */
    int rc = connect(s, dill_ipsockaddr(&addr), dill_iplen(&addr));
    if(rc != 0) {
        assert(rc == -1);
        if(errno != EINPROGRESS)
            return NULL;
        rc = fdwait(s, FDW_OUT, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return NULL;
        }
        int err;
        socklen_t errsz = sizeof(err);
        rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
        if(rc != 0) {
            err = errno;
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
        if(err != 0) {
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
    }

    /* Create the object. */
    struct dill_tcp_conn *conn = malloc(sizeof(struct dill_tcp_conn));
    if(!conn) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    dill_tcp_conn_init(conn, s);
    errno = 0;
    return &conn->sock;
}

static void dill_tcp_bsend(sock s, const void *buf, size_t len, int64_t deadline) {
    struct dill_tcp_conn *c = (struct dill_tcp_conn*)s;
    /* If needed, grow the buffer to fit 'len' bytes. */
    dill_buf_resize(&c->obuf, len);
    /* If there's still not enough free space in the buffer, flush the
       pending data. */
    if(dill_buf_emptysz(&c->obuf) < len) {
        dill_tcp_bflush(s, deadline);
        if(errno != 0)
            return;
    }
    /* At this point there is enough data to fill in user's buffer. */
    struct iovec iov[2];
    int nvecs = dill_buf_empty(&c->obuf, iov);
    int i = 0;
    size_t todo = len;
    while(todo) {
        size_t tocopy = todo < iov[i].iov_len ? todo : iov[i].iov_len;
        memcpy(iov[i].iov_base, buf, tocopy);
        buf = (uint8_t*)buf + tocopy;
        todo -= tocopy;
        ++i;
        assert(i <= nvecs);
    }
    assert(todo == 0);
    dill_buf_haswritten(&c->obuf, len);
    /* Success. */
    errno = 0;
}

static void dill_tcp_bflush(sock s, int64_t deadline) {
    struct dill_tcp_conn *c = (struct dill_tcp_conn*)s;
    struct iovec iov[2];
    int nvecs;
    while(dill_buf_datasz(&c->obuf)) {
        nvecs = dill_buf_data(&c->obuf, iov);
        struct msghdr hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.msg_iov = iov;
        hdr.msg_iovlen = nvecs;
        ssize_t nbytes = sendmsg(c->fd, &hdr, 0);
        if(nbytes < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN)
                return;
            if(!fdwait(c->fd, FDW_OUT, deadline)) {
                errno = ETIMEDOUT;
                return;
            }
            continue;
        }
        dill_buf_hasread(&c->obuf, nbytes);
    }
    /* Success. */
    errno = 0;
}

static void dill_tcp_brecv(sock s, void *buf, size_t len, int64_t deadline) {
    struct dill_tcp_conn *c = (struct dill_tcp_conn*)s;
    int nvecs;
    struct iovec iov[2];
    /* Read more data until we have enough to fill in caller's buffer. */
    dill_buf_resize(&c->ibuf, len);
    while(dill_buf_datasz(&c->ibuf) < len) {
        nvecs = dill_buf_empty(&c->ibuf, iov);
        struct msghdr hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.msg_iov = iov;
        hdr.msg_iovlen = nvecs;
        ssize_t nbytes = recvmsg(c->fd, &hdr, 0);
        if(nbytes == 0) {
            errno = ECONNRESET;
            return;
        }
        if(nbytes < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN)
                return;
            if(!fdwait(c->fd, FDW_IN, deadline)) {
                errno = ETIMEDOUT;
                return;
            }
            continue;
        }
        dill_buf_haswritten(&c->ibuf, nbytes);
    }
    /* We have enough data now. Let's return it to the caller. */
    nvecs = dill_buf_data(&c->ibuf, iov);
    int i;
    size_t todo = len;
    for(i = 0; i != nvecs; ++i) {
        size_t tocopy = todo < iov[i].iov_len ? todo : iov[i].iov_len;
        memcpy(buf, iov[i].iov_base, tocopy);
        buf = (uint8_t*)buf + tocopy;
        todo -= tocopy;
    }
    assert(todo == 0);
    dill_buf_hasread(&c->ibuf, len);
    /* Success. */
    errno = 0;
}

void tcp_close(sock s) {
    if(s->vfptr == &dill_tcp_listener_vfptr) {
        struct dill_tcp_listener *l = (struct dill_tcp_listener*)s;
        fdclean(l->fd);
        int rc = close(l->fd);
        assert(rc == 0);
        free(l);
        errno = 0;
        return;
    }
    if(s->vfptr == &dill_tcp_conn_vfptr) {
        struct dill_tcp_conn *c = (struct dill_tcp_conn*)s;
        fdclean(c->fd);
        int rc = close(c->fd);
        assert(rc == 0);
        dill_buf_term(&c->ibuf);
        dill_buf_term(&c->obuf);
        free(c);
        errno = 0;
        return;
    }
    errno = EPROTOTYPE;
}

