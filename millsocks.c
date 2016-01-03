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

#include "millsocks.h"

void brecv(sock s, void *buf, size_t len, int64_t deadline) {
    if(!s) {errno = EINVAL; return;}
    if(!s->vfptr || !s->vfptr->brecv) {errno = ENOTSUP; return;}
    s->vfptr->brecv(s, buf, len, deadline);
}

void bsend(sock s, const void *buf, size_t len, int64_t deadline) {
    if(!s) {errno = EINVAL; return;}
    if(!s->vfptr || !s->vfptr->bsend) {errno = ENOTSUP; return;}
    s->vfptr->bsend(s, buf, len, deadline);
}

void bflush(sock s, int64_t deadline) {
    if(!s) {errno = EINVAL; return;}
    if(!s->vfptr || !s->vfptr->bflush) {errno = ENOTSUP; return;}
    s->vfptr->bflush(s, deadline);
}

