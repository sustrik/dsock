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

#include "dillsocks.h"
#include "utils.h"

int bcanrecv(sock s) {
    return (*s)->brecv ? 1 : 0;
}

int bcansend(sock s) {
    return (*s)->bsend && (*s)->bflush ? 1 : 0;
}

int mcanrecv(sock s) {
    return (*s)->mrecv ? 1 : 0;
}

int mcansend(sock s) {
    return (*s)->msend && (*s)->mflush ? 1 : 0;
}

int brecv(sock s, void *buf, size_t len, int64_t deadline) {
    if(dill_slow(!bcanrecv(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->brecv(s, buf, len, deadline);
}

int bsend(sock s, const void *buf, size_t len, int64_t deadline) {
    if(dill_slow(!bcansend(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->bsend(s, buf, len, deadline);
}

int bflush(sock s, int64_t deadline) {
    if(dill_slow(!bcansend(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->bflush(s, deadline);
}

int mrecv(sock s, void *buf, size_t len, int64_t deadline) {
    if(dill_slow(!mcanrecv(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->mrecv(s, buf, len, deadline);
}

int msend(sock s, const void *buf, size_t len, int64_t deadline) {
    if(dill_slow(!mcansend(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->msend(s, buf, len, deadline);
}

int mflush(sock s, int64_t deadline) {
    if(dill_slow(!mcansend(s))) {errno = EOPNOTSUPP; return -1;}
    return (*s)->mflush(s, deadline);
}

