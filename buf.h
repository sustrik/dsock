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

#ifndef MILLSOCKS_BUF_INCLUDED
#define MILLSOCKS_BUF_INCLUDED

#include <stdint.h>
#include <sys/uio.h>

/* Cyclic buffer. */
struct mill_buf {
    uint8_t *data;
    size_t head;
    size_t bytes;
    size_t capacity;
};

void mill_buf_init(struct mill_buf *b);
void mill_buf_term(struct mill_buf *b);

size_t mill_buf_datasz(struct mill_buf *b);
int mill_buf_data(struct mill_buf *b, struct iovec *res);
size_t mill_buf_emptysz(struct mill_buf *b);
int mill_buf_empty(struct mill_buf *b, struct iovec *res);

void mill_buf_hasread(struct mill_buf *b, size_t sz);
void mill_buf_haswritten(struct mill_buf *b, size_t sz);

void mill_buf_resize(struct mill_buf *b, size_t sz);

#endif

