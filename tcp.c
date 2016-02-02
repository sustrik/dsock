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

static int tcprecv(sock s, void *buf, size_t len, int64_t deadline);
static int tcpsend(sock s, const void *buf, size_t len, int64_t deadline);
static int tcpflush(sock s, int64_t deadline);

struct tcplistener {
    struct sock_vfptr *vfptr;
    int fd;
    int port;
};

static struct sock_vfptr tcplistener_vfptr = {0};

struct tcpconn {
    struct sock_vfptr *vfptr;
    int fd;
    uint8_t *rbuf;
    size_t rbufsz;
    size_t rcapacity;
    uint8_t *sbuf;
    size_t sbufsz;
    size_t scapacity;
    ipaddr addr;
};

static struct sock_vfptr tcpconn_vfptr = {
    tcprecv,
    tcpsend,
    tcpflush,
    NULL,
    NULL,
    NULL
};

static void tcptune(int s) {
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

static int tcpconn_init(struct tcpconn *c, int fd) {
    c->vfptr = &tcpconn_vfptr;
    c->fd = fd;
    c->rbuf = malloc(TCPMINRECVBUF);
    if(dill_slow(!c->rbuf)) {errno = ENOMEM; return -1;}
    c->rbufsz = 0;
    c->rcapacity = TCPMINRECVBUF;
    c->sbuf = malloc(TCPMINSENDBUF);
    if(dill_slow(!c->sbuf)) {
        free(c->rbuf);
        errno = ENOMEM;
        return -1;
    }
    c->sbufsz = 0;
    c->scapacity = TCPMINSENDBUF;
    return 0;
}

sock tcplisten(const ipaddr *addr, int backlog) {
    if(dill_slow(backlog < 0)) {errno = EINVAL; return NULL;}
    /* Open listening socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(s == -1) return NULL;
    tcptune(s);
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
    struct tcplistener *l = malloc(sizeof(struct tcplistener));
    if(dill_slow(!l)) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    l->vfptr = &tcplistener_vfptr;
    l->fd = s;
    l->port = port;
    return (sock)l;
}

sock tcpaccept(sock s, int64_t deadline) {
    if(dill_slow(*s != &tcplistener_vfptr)) {errno = EPROTOTYPE; return NULL;}
    struct tcplistener *l = (struct tcplistener*)s;
    while(1) {
        /* Try to get new connection (non-blocking). */
        socklen_t addrlen;
        ipaddr addr;
        addrlen = sizeof(addr);
        int as = accept(l->fd, (struct sockaddr*)&addr, &addrlen);
        if(as >= 0) {
            tcptune(as);
            struct tcpconn *c = malloc(sizeof(struct tcpconn));
            if(dill_slow(!c)) {
                fdclean(as);
                close(as);
                errno = ENOMEM;
                return NULL;
            }
            int rc = tcpconn_init(c, as);
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
    tcptune(s);
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
    struct tcpconn *c = malloc(sizeof(struct tcpconn));
    if(!c) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    rc = tcpconn_init(c, s);
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
    if(*s == &tcpconn_vfptr) {
        struct tcpconn *c = (struct tcpconn*)s;
        return ipport(&c->addr);
    }
    if(*s == &tcplistener_vfptr) {
        struct tcplistener *l = (struct tcplistener*)s;
        return l->port;
    }
    errno == EPROTOTYPE;
    return -1;
}

int tcppeer(sock s, ipaddr *addr) {
    if(dill_slow(*s != &tcpconn_vfptr)) {errno = EPROTOTYPE; return -1;}
    struct tcpconn *c = (struct tcpconn*)s;
    if(dill_fast(addr))
        *addr = c->addr;
    return 0;
}

static int tcprecv(sock s, void *buf, size_t len, int64_t deadline) {
    dill_assert(*s == &tcpconn_vfptr);
    struct tcpconn *c = (struct tcpconn*)s;
    /* If there's enough data in the buffer return straight away. */
    if(len <= c->rbufsz) {
        memcpy(buf, c->rbuf, len);
        memmove(c->rbuf, c->rbuf + len, c->rbufsz - len);
        c->rbufsz -= len;
        return 0;
    }
    /* If needed, resize the buffer so that it can accept all the data. */
    if(dill_slow(len > c->rcapacity)) {
        /* TODO: Align the size to a multiple of TCPMINRECVBUF. */
        uint8_t *newbuf = realloc(c->rbuf, len);
        if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
        c->rbuf = newbuf;
        c->rcapacity = len;
    }
    /* Read the data. */
    while(1) {
        ssize_t nbytes = recv(c->fd, c->rbuf + c->rbufsz,
                c->rcapacity - c->rbufsz, 0);
        /* Connection closed by peer. */
        if(dill_slow(!nbytes)) {errno = ECONNRESET; return -1;}
        /* Wait for more data. */
        if(nbytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            int rc = fdwait(c->fd, FDW_IN, deadline);
            if(dill_slow(rc < 0)) return -1;
            continue;
        }
        /* Other errors. */
        if(dill_slow(nbytes < 0)) return -1;
        /* At least some data arrived. */
        c->rbufsz += nbytes;
        /* Enough data arrived to satisfy the request. */
        if(c->rbufsz >= len) {
            memcpy(buf, c->rbuf, len);
            memmove(c->rbuf, c->rbuf + len, c->rbufsz - len);
            c->rbufsz -= len;
            return 0;
        }
    }
}

static int tcpsend(sock s, const void *buf, size_t len, int64_t deadline) {
    dill_assert(*s == &tcpconn_vfptr);
    struct tcpconn *c = (struct tcpconn*)s;
    /* If data fit into the buffer store them and return straight ahead. */
    if(len <= c->scapacity - c->sbufsz) {
        memcpy(c->sbuf + c->sbufsz, buf, len);
        c->sbufsz += len;
        return 0;
    }
    /* Flush all remaining data in the buffer. */
    if(c->sbufsz) {
        int rc = tcpflush(s, deadline);
        if(rc < 0) return -1;
    }
    /* If needed, resize the buffer to fit the data. */
    if(dill_slow(len > c->scapacity)) {
        /* TODO: Align the size to a multiple of TCPMINSENDBUF. */
        uint8_t *newbuf = realloc(c->sbuf, len);
        if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
        c->sbuf = newbuf;
        c->scapacity = len;
    }
    /* Store the data for later sending. */
    memcpy(c->sbuf, buf, len);
    return 0;
}

static int tcpflush(sock s, int64_t deadline) {
    dill_assert(*s == &tcpconn_vfptr);
    struct tcpconn *c = (struct tcpconn*)s;
    if(c->sbufsz == 0) return 0;
    size_t pos = 0;
    while(1) {
        ssize_t nbytes = send(c->fd, c->sbuf + pos, c->sbufsz - pos, 0);
        /* Wait till more data can be send. */
        if(nbytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            int rc = fdwait(c->fd, FDW_OUT, deadline);
            if(dill_slow(rc < 0)) {
                if(pos) {
                    memmove(c->sbuf, c->sbuf + pos, c->sbufsz - pos);
                    c->sbufsz = c->sbufsz - pos;
                }
                return -1;
            }
            continue;
        }
        /* Socket error. */
        if(dill_slow(nbytes < 0)) {
            if(pos) {
                memmove(c->sbuf, c->sbuf + pos, c->sbufsz - pos);
                c->sbufsz = c->sbufsz - pos;
            }
            return -1;
        }
        if(nbytes == c->sbufsz - pos) {
            c->sbufsz = 0;
            return 0;
        }
    }
}

int tcpclose(sock s) {
    if(*s == &tcpconn_vfptr) {
        struct tcpconn *c = (struct tcpconn*)s;
        fdclean(c->fd);
        int rc = close(c->fd);
        dill_assert(rc == 0);
        free(c->sbuf);
        free(c->rbuf);
        free(c);
        return 0;
    }
    if(*s == &tcplistener_vfptr) {
        struct tcplistener *l = (struct tcplistener*)s;
        fdclean(l->fd);
        int rc = close(l->fd);
        dill_assert(rc == 0);
        free(l);
        return 0;
    }
    errno == EPROTOTYPE;
    return -1;
}

