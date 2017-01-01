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
#ifndef DSOCK_IOV_H_INCLUDED
#define DSOCK_IOV_H_INCLUDED

#include <stddef.h>
#include <sys/uio.h>

size_t iov_size(const struct iovec *iov, size_t iovlen);

void iov_copyfrom(void *dst, const struct iovec *src, size_t srclen,
    size_t offset, size_t bytes);

void iov_copyto(const struct iovec *dst, size_t dstlen, const void *src,
    size_t offset, size_t bytes);

void iov_copyallfrom(void *dst, const struct iovec *src, size_t srclen);

void iov_copyallto(const struct iovec *dst, size_t dstlen, const void *src);

void iov_copy(struct iovec *dst, const struct iovec *src, size_t len);

size_t iov_cut(struct iovec *dst, const struct iovec *src, size_t iovlen,
      size_t offset, size_t bytes);

/* Copies the data from vector src into the vector dst.
   dst must be large enough to hold the data */
int iov_deep_copy(const struct iovec *dst, size_t dst_iovlen,
      const struct iovec *src, size_t src_iovlen);
#endif

