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

#include "tweetnacl/tweetnacl.h"

#include "msock.h"
#include "dsock.h"
#include "utils.h"

static const int nacl_type_placeholder = 0;
static const void *nacl_type = &nacl_type_placeholder;
static void nacl_close(int s);
static int nacl_msend(int s, const void *buf, size_t len, int64_t deadline);
static ssize_t nacl_mrecv(int s, void *buf, size_t len, int64_t deadline);

struct nacl_sock {
    struct msock_vfptrs vfptrs;
    int s;
    size_t buflen;
    uint8_t *buf1;
    uint8_t *buf2;
    uint8_t key[crypto_secretbox_KEYBYTES];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
};

int nacl_start(int s, const void *key, size_t keylen) {
    int err;
    /* Check whether underlying socket is message-based. */
    if(dsock_slow(!hdata(s, msock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct nacl_sock *obj = malloc(sizeof(struct nacl_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; goto error1;}
    obj->vfptrs.hvfptrs.close = nacl_close;
    obj->vfptrs.type = nacl_type;
    obj->vfptrs.msend = nacl_msend;
    obj->vfptrs.mrecv = nacl_mrecv;
    obj->s = s;
    obj->buflen = 0;
    obj->buf1 = NULL;
    obj->buf2 = NULL;
    /* Store the key. */
    if(dsock_slow(!key || keylen != sizeof(obj->key))) {
        err = EINVAL; goto error2;}
    memcpy(obj->key, key, keylen);
    /* Generate random nonce. */
    FILE *f = fopen("/dev/urandom", "r");
    if(dsock_slow(!f)) {err = errno; goto error2;}
    size_t sz = fread(obj->nonce, 1, sizeof(obj->nonce), f);
    if(dsock_slow(sz < sizeof(obj->nonce))) {err = ENOENT; goto error2;}
    int rc = fclose(f);
    if(dsock_slow(rc != 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = handle(msock_type, obj, &obj->vfptrs.hvfptrs);
    if(dsock_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int nacl_stop(int s) {
    struct nacl_sock *obj = hdata(s, msock_type);
    if(dsock_slow(obj && obj->vfptrs.type != nacl_type)) {
        errno = ENOTSUP; return -1;}
    free(obj->buf1);
    free(obj->buf2);
    int u = obj->s;
    free(obj);
    return u;
}

static int nacl_resizebufs(struct nacl_sock *obj, size_t len) {
   if(dsock_slow(!obj->buf1 || obj->buflen < len)) {
        obj->buflen = len;
        obj->buf1 = realloc(obj->buf1, len);
        if(dsock_slow(!obj->buf1)) {errno = ENOMEM; return -1;}
        obj->buf2 = realloc(obj->buf2, len);
        if(dsock_slow(!obj->buf2)) {errno = ENOMEM; return -1;}
    }
    return 0;
}

static int nacl_msend(int s, const void *buf, size_t len, int64_t deadline) {
    struct nacl_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == nacl_type);
    /* If needed, adjust the buffers. */
    int rc = nacl_resizebufs(obj, crypto_secretbox_NONCEBYTES +
        crypto_secretbox_ZEROBYTES + len);
    if(dsock_slow(rc < 0)) return -1;
    /* Increase nonce. */
    int i;
    for(i = 0; i != sizeof(obj->nonce); ++i) {
        obj->nonce[i]++;
        if(obj->nonce[i]) break;
    }
    /* Encrypt and authenticate the message. */
    memset(obj->buf1, 0, crypto_secretbox_ZEROBYTES);
    memcpy(obj->buf1 + crypto_secretbox_ZEROBYTES, buf, len);
    crypto_secretbox(obj->buf2 + crypto_secretbox_NONCEBYTES,
        obj->buf1, crypto_secretbox_ZEROBYTES + len, obj->nonce, obj->key);
    /* Send the nonce and the encrypted message. */
    uint8_t *msg = obj->buf2 + crypto_secretbox_ZEROBYTES;
    memcpy(msg, obj->nonce, crypto_secretbox_NONCEBYTES);
    return msend(obj->s, msg, sizeof(obj->nonce) + len, deadline);
}

static ssize_t nacl_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    struct nacl_sock *obj = hdata(s, msock_type);
    dsock_assert(obj->vfptrs.type == nacl_type);
    /* If needed, adjust the buffers. */
    int rc = nacl_resizebufs(obj, crypto_secretbox_NONCEBYTES +
        crypto_secretbox_ZEROBYTES + len);
    /* Read the encrypted message. */
    ssize_t sz = mrecv(s, obj->buf1 + crypto_secretbox_ZEROBYTES,
        crypto_secretbox_NONCEBYTES + len, deadline);
    if(dsock_slow(sz < 0)) return -1;
    if(dsock_slow(sz > crypto_secretbox_NONCEBYTES + len)) {
        errno = EMSGSIZE; return -1;}
    /* Decrypt and authenticate the message. */
    memmove(obj->buf1, obj->buf1 + crypto_secretbox_ZEROBYTES,
        crypto_secretbox_NONCEBYTES);
    memset(obj->buf1 + crypto_secretbox_NONCEBYTES, 0,
        crypto_secretbox_ZEROBYTES);
    rc = crypto_secretbox_open(obj->buf2, obj->buf1 +
        crypto_secretbox_NONCEBYTES, sz - crypto_secretbox_NONCEBYTES +
        crypto_secretbox_ZEROBYTES, obj->buf1, obj->key);
    if(dsock_slow(rc < 0)) {errno = EACCES; return -1;}
    /* Copy the message into user's buffer. */
    memcpy(buf, obj->buf2 + crypto_secretbox_ZEROBYTES, sz);
    return 0;
} 

static void nacl_close(int s) {
    struct nacl_sock *obj = hdata(s, msock_type);
    dsock_assert(obj && obj->vfptrs.type == nacl_type);
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

