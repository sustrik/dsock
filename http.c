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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iov.h"
#include "dsockimpl.h"
#include "utils.h"

dsock_unique_id(http_type);

static void *http_hquery(struct hvfs *hvfs, const void *type);
static void http_hclose(struct hvfs *hvfs);

struct http_sock {
    struct hvfs hvfs;
    /* Underlying CRLF socket. */
    int s;
    int rxerr;
    char rxbuf[1024];
};

static void *http_hquery(struct hvfs *hvfs, const void *type) {
    struct http_sock *obj = (struct http_sock*)hvfs;
    if(type == http_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int http_start(int s) {
    int err;
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hquery(s, bsock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct http_sock *obj = malloc(sizeof(struct http_sock));
    if(dsock_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->hvfs.query = http_hquery;
    obj->hvfs.close = http_hclose;
    obj->s = -1;
    obj->rxerr = 0;
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    /* Make a private copy of the underlying socket. */
    int tmp = hdup(s);
    if(dsock_slow(tmp < 0)) {err = errno; goto error3;}
    /* Wrap the underlying socket into CRLF protocol. */
    obj->s = crlf_start(tmp);
    if(dsock_slow(obj->s < 0)) {err = errno; goto error4;}
    /* Function succeeded. We can now close original undelying handle. */
    int rc = hclose(s);
    dsock_assert(rc == 0);
    return h;
error4:
    rc = hclose(tmp);
    dsock_assert(rc == 0);
error3:;
    rc = hclose(h);
    dsock_assert(rc == 0);
error2:
    free(obj);
error1:
    errno = err;
    return -1;

}

int http_done(int s, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    return crlf_done(obj->s, deadline);
}

int http_stop(int s, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    int u = crlf_stop(obj->s, deadline);
    /* TODO: Handle errors. */
    free(obj);
    return u;
}

int http_sendrequest(int s, const char *command, const char *resource,
      int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    /* TODO: command and resource should contain no spaces! */
    struct iovec iov[4];
    iov[0].iov_base = (void*)command;
    iov[0].iov_len = strlen(command);
    iov[1].iov_base = (void*)" ";
    iov[1].iov_len = 1;
    iov[2].iov_base = (void*)resource;
    iov[2].iov_len = strlen(resource);
    iov[3].iov_base = (void*)" HTTP/1.1";
    iov[3].iov_len = 9;
    return msendv(obj->s, iov, 4, deadline);
}

int http_recvrequest(int s, char *command, size_t commandlen,
      char *resource, size_t resourcelen, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->rxerr)) {errno = obj->rxerr; return -1;}
    ssize_t sz = mrecv(obj->s, obj->rxbuf, sizeof(obj->rxbuf) - 1, deadline);
    if(dsock_slow(sz < 0)) return -1;
    obj->rxbuf[sz] = 0;
    size_t pos = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Command. */
    size_t start = pos;
    while(obj->rxbuf[pos] != 0 && obj->rxbuf[pos] != ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dsock_slow(pos - start > commandlen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(command, obj->rxbuf + start, pos - start);
    command[pos - start] = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Resource. */
    start = pos;
    while(obj->rxbuf[pos] != 0 && obj->rxbuf[pos] != ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dsock_slow(pos - start > resourcelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(resource, obj->rxbuf + start, pos - start);
    resource[pos - start] = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Protocol. */
    start = pos;
    while(obj->rxbuf[pos] != 0 && obj->rxbuf[pos] != ' ') ++pos;
    if(dsock_slow(pos - start != 8 &&
          memcmp(obj->rxbuf + start, "HTTP/1.1", 8) != 0)) {
        errno = EPROTO; return -1;}
    while(obj->rxbuf[pos] == ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] != 0)) {errno = EPROTO; return -1;}
    return 0;
}

int http_sendstatus(int s, int status, const char *reason, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(status < 100 || status > 599)) {errno = EINVAL; return -1;}
    char buf[4];
    buf[0] = (status / 100) + '0';
    status %= 100;
    buf[1] = (status / 10) + '0';
    status %= 10;
    buf[2] = status + '0';
    buf[3] = ' ';
    struct iovec iov[3];
    iov[0].iov_base = (void*)"HTTP/1.1 ";
    iov[0].iov_len = 9;
    iov[1].iov_base = buf;
    iov[1].iov_len = 4;
    iov[2].iov_base = (void*)reason;
    iov[2].iov_len = strlen(reason);
    return msendv(obj->s, iov, 3, deadline);
}

int http_recvstatus(int s, char *reason, size_t reasonlen, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->rxerr)) {errno = obj->rxerr; return -1;}
    ssize_t sz = mrecv(obj->s, obj->rxbuf, sizeof(obj->rxbuf) - 1, deadline);
    if(dsock_slow(sz < 0)) return -1;
    obj->rxbuf[sz] = 0;
    size_t pos = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Protocol. */
    size_t start = pos;
    while(obj->rxbuf[pos] != 0 && obj->rxbuf[pos] != ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dsock_slow(pos - start != 8 &&
          memcmp(obj->rxbuf + start, "HTTP/1.1", 8) != 0)) {
        errno = EPROTO; return -1;}
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Status code. */
    start = pos;
    while(obj->rxbuf[pos] != 0 && obj->rxbuf[pos] != ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dsock_slow(pos - start != 3)) {errno = EPROTO; return -1;}
    if(dsock_slow(obj->rxbuf[start] < '0' || obj->rxbuf[start] > '9' ||
          obj->rxbuf[start + 1] < '0' || obj->rxbuf[start + 1] > '9' ||
          obj->rxbuf[start + 2] < '0' || obj->rxbuf[start + 2] > '9')) {
        errno = EPROTO; return -1;}
    int status = (obj->rxbuf[start] - '0') * 100 +
        (obj->rxbuf[start + 1] - '0') * 10 +
        (obj->rxbuf[start + 2] - '0');
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Reason. */
    if(sz - pos > reasonlen - 1) {errno = EMSGSIZE; return -1;}
    memcpy(reason, obj->rxbuf + pos, sz - pos);
    reason[sz - pos] = 0;
    return status;
}

int http_sendfield(int s, const char *name, const char *value,
      int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    /* TODO: Check whether name contains only valid characters! */
    if (strpbrk(name, "(),/:;<=>?@[\\]{}\" \t") != NULL) {errno = EPROTO; return -1;}
    if (strlen(value) == 0) {errno = EPROTO; return -1;}
    struct iovec iov[3];
    iov[0].iov_base = (void*)name;
    iov[0].iov_len = strlen(name);
    iov[1].iov_base = (void*)": ";
    iov[1].iov_len = 2;
    const char *start = dsock_lstrip(value, ' ');
    const char *end = dsock_rstrip(start, ' ');
    dsock_assert(start < end);
    iov[2].iov_base = (void*)start;
    iov[2].iov_len = end - start;
    return msendv(obj->s, iov, 3, deadline);
}

int http_recvfield(int s, char *name, size_t namelen,
      char *value, size_t valuelen, int64_t deadline) {
    struct http_sock *obj = hquery(s, http_type);
    if(dsock_slow(!obj)) return -1;
    if(dsock_slow(obj->rxerr)) {errno = obj->rxerr; return -1;}
    ssize_t sz = mrecv(obj->s, obj->rxbuf, sizeof(obj->rxbuf) - 1, deadline);
    if(dsock_slow(sz < 0)) return -1;
    obj->rxbuf[sz] = 0;
    size_t pos = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Name. */
    size_t start = pos;
    while(obj->rxbuf[pos] != 0 &&
          obj->rxbuf[pos] != ' ' &&
          obj->rxbuf[pos] != ':')
        ++pos;
    if(dsock_slow(obj->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dsock_slow(pos - start > namelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(name, obj->rxbuf + start, pos - start);
    name[pos - start] = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] != ':')) {errno = EPROTO; return -1;}
    ++pos;
    while(obj->rxbuf[pos] == ' ') ++pos;
    /* Value. */
    start = pos;
    pos = dsock_rstrip(obj->rxbuf + sz, ' ') - obj->rxbuf;
    if(dsock_slow(pos - start > valuelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(value, obj->rxbuf + start, pos - start);
    value[pos - start] = 0;
    while(obj->rxbuf[pos] == ' ') ++pos;
    if(dsock_slow(obj->rxbuf[pos] != 0)) {errno = EPROTO; return -1;}
    return 0;
}

static void http_hclose(struct hvfs *hvfs) {
    struct http_sock *obj = (struct http_sock*)hvfs;
    if(dsock_fast(obj->s >= 0)) {
        int rc = hclose(obj->s);
        dsock_assert(rc == 0);
    }
    free(obj);
}

