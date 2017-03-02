/*

  Copyright (c) 2017 Martin Sustrik

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

#ifndef DSOCK_IOL_H_INCLUDED
#define DSOCK_IOL_H_INCLUDED

#include <dsock.h>

#define iol_to_iov(iolarg, iovarg) \
    size_t n##iovarg = 0;\
    struct iolist *it_##iovarg = iolarg;\
    while(it_##iovarg) {\
        n##iovarg++;\
        it_##iovarg = it_##iovarg->iol_next;\
    }\
    struct iovec iovarg[n##iovarg];\
    it_##iovarg = iolarg;\
    int idx_##iovarg = 0;\
    while(it_##iovarg) {\
        iovarg[idx_##iovarg].iov_base = it_##iovarg->iol_base;\
        iovarg[idx_##iovarg].iov_len = it_##iovarg->iol_len;\
        idx_##iovarg++;\
        it_##iovarg = it_##iovarg->iol_next;\
    }

size_t iol_size(struct iolist *first);

struct iol_slice {
    struct iolist first;
    struct iolist *last;
    struct iolist oldlast;
};

void iol_slice_init(struct iol_slice *self, struct iolist *first,
    struct iolist *last, size_t offset, size_t len);
void iol_slice_term(struct iol_slice *self);

#endif

