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
#include <stdint.h>
#include <stdlib.h>

#include "dillsocks.h"
#include "utils.h"

static const int sf_type_placeholder = 0;
static const void *sf_type = &sf_type_placeholder;

struct sfconn {
    int u;
    uint64_t rxmsgsz; /* 0 means that header was not yet read. */
};

static void sf_stop_fn(int s) {
    dill_assert(0);
}

static int sf_send_fn(int s, struct iovec *iovs, int niovs,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    const void *type = socktype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != sf_type)) {errno = EPROTOTYPE; return -1;}
    if(dill_slow(niovs < 0 || (niovs && !iovs))) {errno == EINVAL; return -1;}
    /* This protocol doesn't use control data. */
    if(dill_slow(inctrl || outctrl)) {errno == EINVAL; return -1;}
    struct sfconn *conn = sockdata(s);
    dill_assert(conn);
    /* Prepare new iovec array with additional item for message header. */
    struct iovec *iov = malloc(sizeof(struct iovec) * (niovs + 1));
    if(dill_slow(!iov)) {errno = ENOMEM; return -1;}
    /* Copy the iovecs to the new array. Along the way, get total size of
       the message. */
    size_t len = 0;
    int i;
    for(i = 0; i != niovs; ++i) {
        len += iovs[i].iov_len;
        iov[i + 1] = iovs[i];
    }
    /* Fill in the message header. */
    uint8_t hdr[8];
    dill_putll(hdr, len);
    iov[0].iov_base = hdr;
    iov[0].iov_len = sizeof(hdr);
    /* Send it to the underlying socket. */
    int rc = socksendv(conn->u, iov, niovs + 1, deadline);
    int err = errno;
    free(iov);
    errno = err;
    return rc;
}

static int sf_recv_fn(int s, struct iovec *iovs, int niovs, size_t *outlen,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    const void *type = socktype(s);
    if(dill_slow(!type)) return -1;
    if(dill_slow(type != sf_type)) {errno = EPROTOTYPE; return -1;}
    if(dill_slow(!s || niovs < 0 || (niovs && !iovs))) {
        errno == EINVAL; return -1;}
    /* This protocol doesn't use control data. */
    if(dill_slow(inctrl || outctrl)) {errno == EINVAL; return -1;}
    struct sfconn *conn = sockdata(s);
    dill_assert(conn);
    /* If the header was not yet read, read it now. */
    if(!conn->rxmsgsz) {
        uint8_t hdr[8];
        int rc = sockrecv(conn->u, hdr, sizeof(hdr), NULL, deadline);
        if(dill_slow(rc < 0)) return -1;
        conn->rxmsgsz = dill_getll(hdr);
        /* There's nothing more to do for 0-byte messages. */
        if(!conn->rxmsgsz) {
            if(outlen)
                *outlen = 0;
            return 0;
        }
    }
    /* To read the message body we need a properly sized gather array.
       Given that we can't modify user's array, let's create a copy. */
    struct iovec *iov = malloc(sizeof(struct iovec) * (niovs + 1));
    if(dill_slow(!iov)) {errno = ENOMEM; return -1;}
    int i;
    size_t bufsz = 0;
    for(i = 0; i != niovs; ++i) {
        if(bufsz == conn->rxmsgsz)
            break;
        if(bufsz + iovs[i].iov_len < conn->rxmsgsz) {
            iov[i].iov_base = iovs[i].iov_base;
            iov[i].iov_len = conn->rxmsgsz - bufsz;
            bufsz += iov[i].iov_len;
            break;
        }
        bufsz += iovs[i].iov_len;
        iov[i] = iovs[i];
    }
    /* If message is larger than the supplied buffer keep it and report
       the size to the user. */
    if(bufsz < conn->rxmsgsz) {
       free(iov);
       if(outlen)
           *outlen = conn->rxmsgsz;
       errno = EMSGSIZE;
       return -1;
    }
    /* Read message body. */
    int rc = sockrecvv(conn->u, iov, i, outlen, deadline);
    int err = errno;
    free(iov);
    errno = err;
    return rc;
}

int sfattach(int s) {
    int err;
    /* Check whether s is a socket. */
    const void *type = socktype(s);
    if(dill_slow(!type)) return -1;
    /* Create the object. */
    struct sfconn *conn = malloc(sizeof(struct sfconn));
    if(dill_slow(!conn)) {errno = ENOMEM; return -1;}
    conn->u = s;
    conn->rxmsgsz = 0;
    /* Bind the object a socket handle. */
    int hndl = sock(sf_type, conn, sf_stop_fn, sf_send_fn, sf_recv_fn);
    if(dill_slow(hndl < 0)) {err = errno; goto error1;}
    return hndl;
error1:
    free(conn);
    errno = err;
    return -1;
}
