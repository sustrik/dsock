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

#ifndef DILLSOCKS_BUF_INCLUDED
#define DILLSOCKS_BUF_INCLUDED

#include <stdint.h>
#include <sys/uio.h>

/* Cyclic buffer. */
struct dill_buf {
    uint8_t *data;
    size_t head;
    size_t bytes;
    size_t capacity;
};

void dill_buf_init(struct dill_buf *b);
void dill_buf_term(struct dill_buf *b);

size_t dill_buf_datasz(struct dill_buf *b);
int dill_buf_data(struct dill_buf *b, struct iovec *res);
size_t dill_buf_emptysz(struct dill_buf *b);
int dill_buf_empty(struct dill_buf *b, struct iovec *res);

void dill_buf_hasread(struct dill_buf *b, size_t sz);
void dill_buf_haswritten(struct dill_buf *b, size_t sz);

void dill_buf_resize(struct dill_buf *b, size_t sz);

#endif

