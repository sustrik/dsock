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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "dillsocks.h"
#include "utils.h"

/*TODO: Copying the buffers each time some data are sent or received is
  atrocious. Replace that with cyclical buffers or something similar. */

/* This number is deliberatly larger than typical MTU (1500B) so  that the
   library tries not to send/recv small sub-optimal data chunks. */
#define BATCHSIZE 2048

static const int tcplistener_type_placeholder = 0;
static const void *tcplistener_type = &tcplistener_type_placeholder;
static int tcplistener_finish(int s, int64_t deadline);
static void tcplistener_close(int s);
static const struct hvfptrs tcplistener_vfptrs = {
    tcplistener_finish,
    tcplistener_close
};

struct tcplistener {
    int fd;
    int port;
};

static const int tcpconn_type_placeholder = 0;
static const void *tcpconn_type = &tcpconn_type_placeholder;
static int tcpconn_send(int s, const void *buf, size_t len, int64_t deadline);
static int tcpconn_recv(int s, void *buf, size_t len, int64_t deadline);
static int tcpconn_flush(int s, int64_t deadline);
static int tcpconn_finish(int s, int64_t deadline);
static void tcpconn_close(int s);

static const struct bsockvfptrs tcpconn_vfptrs = {
    tcpconn_send,
    tcpconn_recv,
    tcpconn_flush,
    tcpconn_finish,
    tcpconn_close
};

struct tcpconn {
    int fd;
    ipaddr addr;
    char *obuf;
    size_t olen;
    size_t ocap;
    char *ibuf;
    size_t ilen;
    size_t icap;
};

/******************************************************************************/
/*  Helper fuctions                                                           */
/******************************************************************************/

static struct tcpconn *tcpconn_create(void) {
    int err;
    struct tcpconn *conn = malloc(sizeof(struct tcpconn));
    if(dill_slow(!conn)) {err = ENOMEM; goto error1;}
    conn->fd = -1;
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->obuf = malloc(BATCHSIZE);
    if(dill_slow(!conn->obuf)) {errno = ENOMEM; goto error2;}
    conn->olen = 0;
    conn->ocap = BATCHSIZE;
    conn->ibuf = malloc(BATCHSIZE);
    if(dill_slow(!conn->ibuf)) {errno = ENOMEM; goto error3;}
    conn->ilen = 0;
    conn->icap = BATCHSIZE;
    return conn;
error3:
    free(conn->obuf);
error2:
    free(conn);
error1:
    errno = err;
    return NULL;
}

static void tcpconn_destroy(struct tcpconn *conn) {
    free(conn->ibuf);
    free(conn->obuf);
    free(conn);
}

static void tcptune(int s) {
    /* Make the socket non-blocking. */
    int rc = dsunblock(s);
    dill_assert(rc == 0);
    /*  Allow re-using the same local address rapidly. */
    int opt = 1;
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

/******************************************************************************/
/*  Connection establishment                                                  */
/******************************************************************************/

int tcplisten(const ipaddr *addr, int backlog) {
    int err;
    if(dill_slow(backlog < 0)) {errno = EINVAL; return -1;}
    /* Open listening socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) return -1;
    tcptune(s);
    /* Start listening. */
    int rc = bind(s, ipsockaddr(addr), iplen(addr));
    if(dill_slow(rc != 0)) return -1;
    rc = listen(s, backlog);
    if(dill_slow(rc != 0)) return -1;
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS now. */
    int port = ipport(addr);
    if(port == 0) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(dill_slow(rc < 0)) {err = errno; goto error1;}
        port = ipport(&baddr);
    }
    /* Create the object. */
    struct tcplistener *lst = malloc(sizeof(struct tcplistener));
    if(dill_slow(!lst)) {errno = ENOMEM; goto error1;}
    lst->fd = s;
    lst->port = port;
    /* Bind the object to a handle. */
    int h = handle(tcplistener_type, lst, &tcplistener_vfptrs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(lst);
error1:
    rc = dsclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

int tcpaccept(int s, int64_t deadline) {
    int err;
    struct tcplistener *lst = hdata(s, tcplistener_type);
    if(dill_slow(!lst)) return -1;
    /* Try to get new connection in a non-blocking way. */
    ipaddr addr;
    socklen_t addrlen;
    int as = dsaccept(lst->fd, (struct sockaddr*)&addr, &addrlen, deadline);
    if(dill_slow(as < 0)) return -1;
    tcptune(as);
    /* Create the object. */
    struct tcpconn *conn = tcpconn_create();
    if(dill_slow(!conn)) {err = errno; goto error1;}
    conn->fd = as;
    conn->addr = addr;
    /* Bind the object to a handle. */
    int hndl = bsock(tcpconn_type, conn, &tcpconn_vfptrs);
    if(dill_slow(hndl < 0)) {err = errno; goto error2;}
    return hndl;
error2:
    tcpconn_destroy(conn);
error1:;
    int rc = dsclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

int tcpconnect(const ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipfamily(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) return -1;
    tcptune(s);
    /* Connect to the remote endpoint. */
    int rc = dsconnect(s, ipsockaddr(addr), iplen(addr), deadline);
    if(dill_slow(rc < 0)) return -1;
    /* Create the object. */
    struct tcpconn *conn = tcpconn_create();
    if(dill_slow(!conn)) {err = errno; goto error1;}
    conn->fd = s;
    conn->addr = *addr;
    /* Bind the object to a sock handle. */
    int bs = bsock(tcpconn_type, conn, &tcpconn_vfptrs);
    if(dill_slow(bs < 0)) {err = errno; goto error2;}
    return bs;
error2:
    tcpconn_destroy(conn);
error1:
    rc = dsclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

int tcpport(int s) {
    struct tcplistener *lst = hdata(s, tcplistener_type);
    if(dill_slow(!lst)) return -1;
    return lst->port;
}

int tcppeer(int s, ipaddr *addr) {
    struct tcpconn *conn = bsockdata(s, tcpconn_type);
    if(dill_slow(!conn)) return -1;
    if(dill_fast(addr))
        *addr = conn->addr;
    return 0;
}

static int tcplistener_finish(int s, int64_t deadline) {
    tcplistener_close(s);
    return 0;
}

static void tcplistener_close(int s) {
    struct tcplistener *lst = hdata(s, tcplistener_type);
    dill_assert(lst);
    int rc = dsclose(lst->fd);
    dill_assert(rc == 0);
    free(lst);
}

/******************************************************************************/
/*  Sending and receiving data                                                */
/******************************************************************************/

static int tcpconn_send(int s, const void *buf, size_t len, int64_t deadline) {
    struct tcpconn *conn = bsockdata(s, tcpconn_type);
    if(dill_slow(!conn)) return -1;
    /* If the data fit into the buffer without exceeding the BATCHSIZE
       we can store it and be done with it. That way sending a lot of little
       messages won't result in lot of system calls. */
    if(dill_fast(conn->olen + len <= BATCHSIZE)) {
        memcpy(conn->obuf + conn->olen, buf, len);
        conn->olen += len;
        return 0;
    }
    /* Message won't fit into buffer, so let's try to flush the buffer first. */
    int rc = tcpconn_flush(s, deadline);
    if(dill_slow(rc < 0)) return -1;
    /* Try to fit the message into buffer again. */
    if(dill_fast(len <= BATCHSIZE)) {
        dill_assert(conn->olen == 0);
        memcpy(conn->obuf, buf, len);
        conn->olen = len;
        return 0;
    }
    /* Resize the buffer so that it can hold the message if needed. By doing
       this in advance we won't be faced with ugly non-atomic case when only
       part of the message is sent to the network but there's no buffer space
       to buffer the remaining data. */
    if(dill_slow(len > conn->olen)) {
        char *newbuf = realloc(conn->obuf, len);
        if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
        conn->obuf = newbuf;
        conn->ocap = len;
    }
    /* Try to send the message directly from the user's buffer. */
    size_t sent = len;
    rc = dssend(conn->fd, buf, &sent, deadline);
    if(dill_fast(rc >= 0)) return 0;
    int err = errno;
    /* Store the remainder of the message into the output buffer. */
    dill_assert(conn->olen == 0);
    memcpy(conn->obuf, ((char*)buf) + sent, len - sent);
    conn->olen = len - sent;
    /* ETIMEDOUT can be ignored. The send operation have already completed
       successfully so there's no point in reporting the error. */
    if(dill_slow(err == ETIMEDOUT)) return 0;
    /* Other errors, such as ECONNRESET or ECANCELED are fatal so we can report
       them straight away even though message data were buffered. */
    errno = err;
    return -1;
}

static int tcpconn_recv(int s, void *buf, size_t len, int64_t deadline) {
    struct tcpconn *conn = bsockdata(s, tcpconn_type);
    if(dill_slow(!conn)) return -1;
    /* If there's enough data in the buffer use it. */
    if(dill_fast(len <= conn->ilen)) {
        memcpy(buf, conn->ibuf, len);
        memmove(conn->ibuf, conn->ibuf + len, conn->ilen - len);
        conn->ilen -= len;
        return 0;
    }
    /* If there's not enough data in the buffer, yet the message is small,
       read whole chunk of data to avoid excessive system calls. */
    if(len <= BATCHSIZE) {
        /* First try a non-blocking batch read. */
        size_t recvd = BATCHSIZE - conn->ilen;
        /* TODO: Make sure that deadline of 0 works as intended! */
        dsrecv(conn->fd, conn->ibuf + conn->ilen, &recvd, 0);
        conn->ilen += recvd;
        if(dill_fast(len <= conn->ilen)) {
            memcpy(buf, conn->ibuf, len);
            memmove(conn->ibuf, conn->ibuf + len, conn->ilen - len);
            conn->ilen -= len;
            return 0;
        }
    }
    /* Either message is big or we weren't able to read it in non-blocking
       fashion. In both cases let's try to read the missing data directly
       to the user's buffer. If the operation fails we'll copy the data that
       was already read to the input buffer. To make sure that the operation
       is atomic even in face of memory shortage let's resize the buffer first
       do receiving second. */
    if(dill_slow(len > conn->ilen)) {
        char *newbuf = realloc(conn->ibuf, len);
        if(dill_slow(!newbuf)) {errno = ENOMEM; return -1;}
        conn->ibuf = newbuf;
        conn->icap = len;
    }
    /* Read the missing part directly into user's buffer. */
    size_t recvd = len - conn->ilen;
    int rc = dsrecv(conn->fd, ((char*)buf) + conn->ilen, &recvd, deadline);
    /* If succesfull copy the first part of the message from input buffer. */
    if(dill_fast(rc == 0)) {
        memcpy(buf, conn->ibuf, conn->ilen);
        conn->ilen = 0;
        return 0;
    }
    /* If not successful, store the aready received data into input buffer. */
    int err = errno;
    memcpy(conn->ibuf + conn->ilen, ((char*)buf) + conn->ilen, recvd);
    conn->ibuf += recvd;
    errno = err;
    return -1;
}

static int tcpconn_flush(int s, int64_t deadline) {
    struct tcpconn *conn = bsockdata(s, tcpconn_type);
    if(dill_slow(!conn)) return -1;
    /* Try to send the buffered data. */
    if(conn->olen == 0) return 0;
    size_t sent = conn->olen;
    int rc = dssend(conn->fd, conn->obuf, &sent, deadline);
    if(dill_fast(rc == 0)) {
        conn->olen = 0;
        return 0;
    }
    /* Drop sent data from the buffer. */
    int err = errno;
    memmove(conn->obuf, conn->obuf + sent, conn->olen - sent);
    conn->olen -= sent;
    errno = err;
    return -1;
}

/******************************************************************************/
/*  Connection teardown                                                       */
/******************************************************************************/

static int tcpconn_finish(int s, int64_t deadline) {
    int rc;
    int err = 0;
    struct tcpconn *conn = bsockdata(s, tcpconn_type);
    if(dill_slow(!conn)) return -1;
    /* First, let's try to flush remaining outbound data. */
    if(deadline != 0) {
        rc = tcpconn_flush(s, deadline);
        if(dill_slow(rc < 0)) err = errno;
    }
    /* Close the underlying socket. */
    rc = dsclose(conn->fd);
    dill_assert(rc == 0);
    /* Deallocate the object. */
    tcpconn_destroy(conn);
    errno = err;
    return err ? -1 : 0;
}

static void tcpconn_close(int s) {
    int rc = tcpconn_finish(s, 0);
    dill_assert(rc == 0);
}

