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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#include "dillsocks.h"
#include "utils.h"

static const int inproc_type_placeholder = 0;
static const void *inproc_type = &inproc_type_placeholder;

static void inproc_close(int s);
static void inproc_dump(int s);
static int inproc_send(int s, struct iovec *iovs, int niovs,
    const struct sockctrl *inctrl, struct sockctrl *outctrl, int64_t deadline);
static int inproc_recv(int s, struct iovec *iovs, int niovs, size_t *outlen,
    const struct sockctrl *inctrl, struct sockctrl *outctrl, int64_t deadline);

static const struct sockvfptrs inproc_vfptrs = {
    inproc_close,
    inproc_dump,
    inproc_send,
    inproc_recv
};

struct inproc {
    int chin;
    int chout;
    struct iovec iov;
};

int inprocpair(int s[2]) {
    int err;
    int ch1 = channel(sizeof(struct iovec), 100);
    if(dill_slow(!ch1)) {err = errno; goto error1;}
    int ch2 = channel(sizeof(struct iovec), 100);
    if(dill_slow(!ch2)) {err = errno; goto error2;}
    struct inproc *ep1 = malloc(sizeof(struct inproc));
    if(dill_slow(!ep1)) {err = ENOMEM; goto error3;}
    struct inproc *ep2 = malloc(sizeof(struct inproc));
    if(dill_slow(!ep2)) {err = ENOMEM; goto error4;}
    ep1->chin = ch1;
    ep1->chout = ch2;
    ep1->iov.iov_base = NULL;
    ep1->iov.iov_len = 0;
    ep2->chin = hdup(ch2);
    dill_assert(ep2->chin >= 0);
    ep2->chout = hdup(ch1);
    dill_assert(ep2->chout >= 0);
    ep2->iov.iov_base = NULL;
    ep2->iov.iov_len = 0;
    s[0] = sock(inproc_type, SOCK_IN | SOCK_OUT, ep1, &inproc_vfptrs);
    if(dill_slow(s[0] < 0)) {err = errno; goto error5;}
    s[1] = sock(inproc_type, SOCK_IN | SOCK_OUT, ep2, &inproc_vfptrs);
    if(dill_slow(s[1] < 0)) {err = errno; goto error6;}
    return 0;
error6:
    hclose(s[0]); /* TODO: Check this error mode. */
error5:
    free(ep2);
error4:
    free(ep1);
error3:
    hclose(ch2);
error2:
    hclose(ch1);
error1:
    errno = err;
    return -1;
}

int inprocclose(int s) {
    struct inproc *ip = sockdata(s, inproc_type);
    dill_assert(ip);
    dill_assert(0);
}

static void inproc_close(int s) {
    struct inproc *ip = sockdata(s, inproc_type);
    dill_assert(ip);
    dill_assert(0);
}

static void inproc_dump(int s) {
    struct inproc *ip = sockdata(s, inproc_type);
    dill_assert(ip);
    dill_assert(0);
}

static int inproc_send(int s, struct iovec *iovs, int niovs,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct inproc *ip = sockdata(s, inproc_type);
    dill_assert(ip);
    /* Compute total size of the message. */
    size_t sz = 0;
    int i;
    for(i = 0; i != niovs; ++i)
        sz += iovs[i].iov_len;
    /* Allocate buffer for the data. */
    struct iovec iov;
    iov.iov_base = malloc(sz);
    if(dill_slow(!iov.iov_base)) {errno = ENOMEM; return -1;}
    iov.iov_len = sz;
    /* Copy the data into the buffer. */
    uint8_t *pos = iov.iov_base;
    for(i = 0; i != niovs; ++i) {
        memcpy(pos, iovs[i].iov_base, iovs[i].iov_len);
        pos += iovs[i].iov_len;
    }
    /* Send the buffer to the peer. */
    int rc = chsend(ip->chout, &iov, sizeof(iov), deadline);
    if(dill_slow(rc < 0)) {
        int err = errno;
        free(iov.iov_base);
        errno = err;
    }
    return rc;
}
static int inproc_recv(int s, struct iovec *iovs, int niovs, size_t *outlen,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct inproc *ip = sockdata(s, inproc_type);
    dill_assert(0);
}

