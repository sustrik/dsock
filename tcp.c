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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "dillsocks.h"
#include "utils.h"

/* Deliberately larger than standard IP packet size. */
#define TCPMINRECVBUF 2048
#define TCPMINSENDBUF 2048

static int tcp_send(sock s, struct iovec *iovs, int niovs,
    const struct sockctrl *inctrl, struct sockctrl *outctrl,
    int64_t deadline);
static int tcp_recv(sock s, struct iovec *iovs, int niovs, size_t *len,
    const struct sockctrl *inctrl, struct sockctrl *outctrl,
    int64_t deadline);

struct tcp_listener {
    struct sockvfptr *vfptr;
    int fd;
    int port;
};

static struct sockvfptr tcp_listener_vfptr = {0};

struct tcp_conn {
    struct sockvfptr *vfptr;
    int fd;

    uint8_t *txbuf;
    size_t txbuf_len;
    size_t txbuf_capacity;
    chan tosender;
    chan fromsender;
    coro sender;

    uint8_t *rxbuf;
    size_t rxbuf_len;
    size_t rxbuf_capacity;

    ipaddr addr;
};

static struct sockvfptr tcp_conn_vfptr = {
    tcp_send,
    tcp_recv
};

static void tcp_tune(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    dill_assert(rc != -1);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    dill_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    dill_assert (rc == 0 || errno == EINVAL);
#endif
}

static coroutine void tcp_sender(struct tcp_conn *conn) {
    while(1) {
        /* Hand the buffer to the main object. */
        int rc = chsend(conn->fromsender, NULL, 0, -1);
        if(dill_slow(rc == -1 && errno == EPIPE)) return;
        if(dill_slow(rc == -1 && errno == ECANCELED)) return;
        dill_assert(rc == 0);
        /* Wait till main object fills the buffer and hands it back. */
        rc = chrecv(conn->tosender, NULL, 0, -1);
        if(dill_slow(rc == -1 && errno == EPIPE)) return;
        if(dill_slow(rc == -1 && errno == ECANCELED)) return;
        dill_assert(rc == 0);
        /* Loop until all data in send buffer are sent. */
        uint8_t *pos = conn->txbuf;
        size_t len = conn->txbuf_len;
        while(len) {
            rc = fdwait(conn->fd, FDW_OUT, -1);
            if(dill_slow(rc == -1 && errno == ECANCELED)) return;
            dill_assert(rc == FDW_OUT);
            ssize_t sz = send(conn->fd, pos, len, 0);
            /* TODO: Handle connection errors. */
            dill_assert(sz >= 0);
            pos += sz;
            len -= sz;
        }
    }
}

static int tcp_conn_init(struct tcp_conn *conn, int fd) {
    /* TODO: Error handling. */
    conn->vfptr = &tcp_conn_vfptr;
    conn->fd = fd;
    conn->rxbuf = NULL;
    conn->rxbuf_len = 0;
    conn->rxbuf_capacity = 0;
    conn->txbuf = NULL;
    conn->txbuf_len = 0;
    conn->txbuf_capacity = 0;
    conn->tosender = channel(0, 0);
    dill_assert(conn->tosender);
    conn->fromsender = channel(0, 0);
    dill_assert(conn->fromsender);
    conn->sender = go(tcp_sender(conn));
    dill_assert(conn->sender);
    return 0;
}

sock tcplisten(const ipaddr *addr, int backlog) {
    if(dill_slow(backlog < 0)) {errno = EINVAL; return NULL;}
    /* Open listening socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(s == -1) return NULL;
    tcp_tune(s);
    /* Start listening. */
    int rc = bind(s, ipsockaddr(addr), iplen(addr));
    if(rc != 0) return NULL;
    rc = listen(s, backlog);
    if(rc != 0) return NULL;
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS now. */
    int port = ipport(addr);
    if(port == 0) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc == -1) {
            int err = errno;
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
        port = ipport(&baddr);
    }
    /* Create the object. */
    struct tcp_listener *l = malloc(sizeof(struct tcp_listener));
    if(dill_slow(!l)) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    l->vfptr = &tcp_listener_vfptr;
    l->fd = s;
    l->port = port;
    return (sock)l;
}

sock tcpaccept(sock s, int64_t deadline) {
    if(dill_slow(*s != &tcp_listener_vfptr)) {errno = EPROTOTYPE; return NULL;}
    struct tcp_listener *l = (struct tcp_listener*)s;
    while(1) {
        /* Try to get new connection (non-blocking). */
        socklen_t addrlen;
        ipaddr addr;
        addrlen = sizeof(addr);
        int as = accept(l->fd, (struct sockaddr*)&addr, &addrlen);
        if(as >= 0) {
            tcp_tune(as);
            struct tcp_conn *c = malloc(sizeof(struct tcp_conn));
            if(dill_slow(!c)) {
                fdclean(as);
                close(as);
                errno = ENOMEM;
                return NULL;
            }
            int rc = tcp_conn_init(c, as);
            if(dill_slow(rc < 0)) {
                int err = errno;
                fdclean(as);
                close(as);
                free(c);
                errno = err;
                return NULL;
            }
            c->addr = addr;
            return (sock)c;
        }
        dill_assert(as == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK) return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(l->fd, FDW_IN, deadline);
        if(rc < 0) return NULL;
        dill_assert(rc == FDW_IN);
    }
}

sock tcpconnect(const ipaddr *addr, int64_t deadline) {
    /* Open a socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(s < 0) return NULL;
    tcp_tune(s);
    /* Connect to the remote endpoint. */
    int rc = connect(s, ipsockaddr(addr), iplen(addr));
    if(rc != 0) {
        dill_assert(rc == -1);
        if(errno != EINPROGRESS) return NULL;
        rc = fdwait(s, FDW_OUT, deadline);
        if(rc < 0) return NULL;
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
    struct tcp_conn *c = malloc(sizeof(struct tcp_conn));
    if(!c) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    rc = tcp_conn_init(c, s);
    if(dill_slow(rc < 0)) {
        int err = errno;
        fdclean(s);
        close(s);
        free(c);
        errno = err;
        return NULL;
    }
    return (sock)c;
}

int tcpport(sock s) {
    if(*s == &tcp_conn_vfptr) {
        struct tcp_conn *c = (struct tcp_conn*)s;
        return ipport(&c->addr);
    }
    if(*s == &tcp_listener_vfptr) {
        struct tcp_listener *l = (struct tcp_listener*)s;
        return l->port;
    }
    errno == EPROTOTYPE;
    return -1;
}

int tcppeer(sock s, ipaddr *addr) {
    if(dill_slow(*s != &tcp_conn_vfptr)) {errno = EPROTOTYPE; return -1;}
    struct tcp_conn *c = (struct tcp_conn*)s;
    if(dill_fast(addr))
        *addr = c->addr;
    return 0;
}

static int tcp_send(sock s, struct iovec *iovs, int niovs,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    if(dill_slow(*s != &tcp_conn_vfptr)) {errno = EPROTOTYPE; return -1;}
    if(dill_slow(!s || niovs < 0 || (niovs && !iovs))) {
        errno == EINVAL; return -1;}
    /* This protocol doesn't use control data. */
    if(dill_slow(inctrl || outctrl)) {errno == EINVAL; return -1;}
    struct tcp_conn *conn = (struct tcp_conn*)s;
    /* Wait till sender coroutine hands us the send buffer. */
    int rc = chrecv(conn->fromsender, NULL, 0, deadline);
    if(dill_slow(rc < 0))
        return -1;
    /* Resize the send buffer so that the data fit it. */
    size_t len = 0;
    int i;
    for(i = 0; i != niovs; ++i)
        len += iovs[i].iov_len;
    if(dill_slow(conn->txbuf_capacity < len)) {
        void *newbuf = realloc(conn->txbuf, len);
        if(dill_slow(!newbuf)) {
            /* TODO: Eek! Now we own the buffer but the next invocation of 
                     tcp_send() won't know about it. */
            errno = ENOMEM;
            return -1;
        }
        conn->txbuf = newbuf;
        conn->txbuf_capacity = len;
    }
    /* Copy the data to the buffer. */
    uint8_t *pos = conn->txbuf;
    for(i = 0; i != niovs; ++i) {
        memcpy(pos, iovs[i].iov_base, iovs[i].iov_len);
        pos += iovs[i].iov_len;
    }
    conn->txbuf_len = len;
    /* Hand the buffer to the sender coroutine. */
    rc = chsend(conn->tosender, NULL, 0, -1);
    dill_assert(rc == 0); // ECANCELED ?
    return 0;
}

static int tcp_recv(sock s, struct iovec *iovs, int niovs, size_t *len,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    if(dill_slow(*s != &tcp_conn_vfptr)) {errno = EPROTOTYPE; return -1;}
    if(dill_slow(!s || niovs < 0 || (niovs && !iovs))) {
        errno == EINVAL; return -1;}
    /* This protocol doesn't use control data. */
    if(dill_slow(inctrl || outctrl)) {errno == EINVAL; return -1;}
    struct tcp_conn *conn = (struct tcp_conn*)s;
    /* Compute total size of the data requested. */
    size_t sz = 0;
    int i;
    for(i = 0; i != niovs; ++i)
        sz += iovs[i].iov_len;
    /* If there's not enough data in the buffer try to read them from
       the socket. */
    if(sz > conn->rxbuf_len) {
        /* Resize the buffer to be able to hold all the data. */
        if(dill_slow(sz > conn->rxbuf_capacity)) {
            uint8_t *newbuf = realloc(conn->rxbuf, sz);
            if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
            conn->rxbuf = newbuf;
            conn->rxbuf_capacity = sz;
        }
        while(conn->rxbuf_len < sz) {
            int rc = fdwait(conn->fd, FDW_IN, deadline);
            if(dill_slow(rc < 0)) return -1;
            dill_assert(rc == FDW_IN);
            ssize_t nbytes = recv(conn->fd, conn->rxbuf + conn->rxbuf_len,
                sz - conn->rxbuf_len, 0);
            /* TODO: Handle connection errors. */
            dill_assert(nbytes != 0);
            if(dill_slow(nbytes < 0)) return -1;
            conn->rxbuf_len += nbytes;
        }
    }
    /* Copy the data from rx buffer to user-supplied buffer(s). */
    uint8_t *pos = conn->rxbuf;
    for(i = 0; i != niovs; ++i) {
        memcpy(iovs[i].iov_base, pos, iovs[i].iov_len);
        pos += iovs[i].iov_len;
    }
    /* Shift remaining data in the buffer to the beginning. */
    conn->rxbuf_len = conn->rxbuf_len - (pos - conn->rxbuf);
    memmove(conn->rxbuf, pos, conn->rxbuf_len);
    if(len)
        *len = sz;
    return 0;
}

int tcpclose(sock s, int64_t deadline) {
    if(*s == &tcp_conn_vfptr) {
        struct tcp_conn *c = (struct tcp_conn*)s;
        int rc = chdone(c->tosender);
        dill_assert(rc == 0);
        rc = chdone(c->fromsender);
        dill_assert(rc == 0);
        gocancel(&c->sender, 1, deadline);
        chclose(c->tosender);
        chclose(c->fromsender);
        free(c->txbuf);
        free(c->rxbuf);
        fdclean(c->fd);
        rc = close(c->fd);
        dill_assert(rc == 0);
        free(c);
        return 0;
    }
    if(*s == &tcp_listener_vfptr) {
        struct tcp_listener *l = (struct tcp_listener*)s;
        fdclean(l->fd);
        int rc = close(l->fd);
        dill_assert(rc == 0);
        free(l);
        return 0;
    }
    errno == EPROTOTYPE;
    return -1;
}

