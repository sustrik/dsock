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

#include "dillsocks.h"
#include "utils.h"

static const int cons_type_placeholder = 0;
static const void *cons_type = &cons_type_placeholder;

static void cons_close(int s);
static void cons_dump(int s);
static int cons_send(int s, struct iovec *iovs, int niovs,
    const struct sockctrl *inctrl, struct sockctrl *outctrl, int64_t deadline);
static int cons_recv(int s, struct iovec *iovs, int niovs, size_t *outlen,
    const struct sockctrl *inctrl, struct sockctrl *outctrl, int64_t deadline);

static const struct sockvfptrs cons_vfptrs = {
    cons_close,
    cons_dump,
    cons_send,
    cons_recv
};   

struct cons {
    int in;
    int out;
};

int consattach(int in, int out) {
    /* Check whether arguments are sockets. */
    if(in != -1) {
        void *data = sockdata(in, NULL);
        if(dill_slow(!data)) {errno = EINVAL; return -1;}
    }
    if(out != -1) {
        void *data = sockdata(out, NULL);
        if(dill_slow(!data)) {errno = EINVAL; return -1;}
    }
    /* Merge flags from the two underlying sockets. */
    int flags = 0;
    if(in >= 0) {
        flags |= sockflags(in) & (SOCK_IN | SOCK_INMSG | SOCK_INREL |
            SOCK_INORD);
    }
    if(out >= 0) {
        flags |= sockflags(out) & (SOCK_OUT | SOCK_OUTMSG | SOCK_OUTREL |
            SOCK_OUTORD);
    }
    /* Create the object. */
    struct cons *cns = malloc(sizeof(struct cons));
    if(dill_slow(!cns)) {errno = ENOMEM; return -1;}
    cns->in = in;
    cns->out = out;
    int h = sock(cons_type, flags, cns, &cons_vfptrs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(cns);
        errno = err;
        return -1;
    };
    return h;
}

static void cons_close(int s) {
    struct cons *cns = sockdata(s, cons_type);
    dill_assert(cns);
    free(cns);
}

static void cons_dump(int s) {
    struct cons *cns = sockdata(s, cons_type);
    dill_assert(cns);
    fprintf(stderr, "  CONS in:{%d} out:{%d}\n", cns->in, cns->out);
}

static int cons_send(int s, struct iovec *iovs, int niovs,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct cons *cns = sockdata(s, cons_type);
    if(dill_slow(!cns)) return -1;
    if(dill_slow(cns->out < 0)) {errno = ENOTSUP; return -1;}
    return socksendmsg(cns->out, iovs, niovs, inctrl, outctrl, deadline);
}

static int cons_recv(int s, struct iovec *iovs, int niovs, size_t *outlen,
      const struct sockctrl *inctrl, struct sockctrl *outctrl,
      int64_t deadline) {
    struct cons *cns = sockdata(s, cons_type);
    if(dill_slow(!cns)) return -1;
    if(dill_slow(cns->in < 0)) {errno = ENOTSUP; return -1;}
    return sockrecvmsg(cns->in, iovs, niovs, outlen, inctrl, outctrl, deadline);
}

int consdetach(int s, int *in, int *out) {
    struct cons *cns = sockdata(s, cons_type);
    if(dill_slow(!cns)) return -1;
    if(in)
       *in = cns->in;
    if(out)
       *out = cns->out;
    hclose(s);
    return 0;
}

