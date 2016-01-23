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
#include <stdlib.h>
#include <string.h>

#include "buf.h"

void dill_buf_init(struct dill_buf *b) {
    b->data = malloc(4000);
    assert(b->data);
    b->capacity = 4000;
    b->head = 0;
    b->bytes = 0;
}

void dill_buf_term(struct dill_buf *b) {
    free(b->data);
}

size_t dill_buf_datasz(struct dill_buf *b) {
    return b->bytes;
}

int dill_buf_data(struct dill_buf *b, struct iovec *res) {
    if(b->head + b->bytes <= b->capacity) {
        res[0].iov_base = b->data + b->head; 
        res[0].iov_len = b->bytes;
        return 1;
    }
    res[0].iov_base = b->data + b->head;
    res[0].iov_len = b->capacity - b->head;
    res[1].iov_base = b->data;
    res[1].iov_len = b->bytes - res[0].iov_len;
    return 2;
}

size_t dill_buf_emptysz(struct dill_buf *b) {
    return b->capacity - b->bytes;
}

int dill_buf_empty(struct dill_buf *b, struct iovec *res) {
    size_t ehead = (b->head + b->bytes) % b->capacity;
    size_t ebytes = b->capacity - b->bytes;
    if(ehead + ebytes <= b->capacity) {
        res[0].iov_base = b->data + ehead; 
        res[0].iov_len = ebytes;
        return 1;
    }
    res[0].iov_base = b->data + ehead;
    res[0].iov_len = b->capacity - ehead;
    res[1].iov_base = b->data;
    res[1].iov_len = ebytes - res[0].iov_len;
    return 2;
}

void dill_buf_hasread(struct dill_buf *b, size_t sz) {
    assert(sz <= b->bytes);
    b->head = (b->head + sz) % b->capacity;
    b->bytes -= sz;
}

void dill_buf_haswritten(struct dill_buf *b, size_t sz) {
    assert(b->bytes + sz <= b->capacity);
    b->bytes += sz;
}

void dill_buf_resize(struct dill_buf *b, size_t sz) {
    if(sz <= b->capacity)
        return;
    b->data = realloc(b->data, sz);
    assert(b->data);
    size_t hsz = b->capacity - b->head;
    size_t tsz = b->bytes - hsz;
    size_t asz = sz - b->capacity;
    if(tsz <= asz) {
        memcpy(b->data + b->capacity, b->data, tsz);
    }
    else {
        memcpy(b->data + b->capacity, b->data, asz);
        memmove(b->data, b->data + asz, tsz - asz);
    }
    b->capacity = sz;
}

