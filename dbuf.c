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

#include <stdlib.h>
#include <string.h>

#include "dillsocks.h"
#include "utils.h"

#define MAX_EMBEDDED 56

int dbufalloc(struct dbuf *buf, size_t len) {
    buf->len = len;
    if(len <= MAX_EMBEDDED) return 0;
    void *chunk = malloc(len);
    if(dill_slow(!chunk) {errno = ENOMEM; return -1;}
    *(void**)buf->data = chunk;
    return 0;
}

void *dbufdata(struct dbuf *buf) {
    if(buf->len <= MAX_EMBEDDED) return (void*)buf->data;
    return *(void**)buf->data; 
}

size_t dbuflen(struct dbuf *buf) {
    return buf->len;
}

void dbuffree(struct dbuf *buf) {
    if(buf->len <= MAX_EMBEDDED) return;
    free(*(void**)buf->data);
}

