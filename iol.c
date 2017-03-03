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

#include <string.h>

#include "iol.h"
#include "utils.h"

size_t iol_size(struct iolist *first) {
    size_t sz = 0;
    while(first) {
        sz += first->iol_len;
        first = first->iol_next;
    }
    return sz;
}

void iol_copyallfrom(uint8_t *dst, struct iolist *first) {
    while(first) {
        memcpy(dst, first->iol_base, first->iol_len);
        dst += first->iol_len;
        first = first->iol_next;
    }
}

void iol_slice_init(struct iol_slice *self, struct iolist *first,
      struct iolist *last, size_t offset, size_t len) {
    struct iolist *it = first;
    while(offset >= it->iol_len) {
        offset -= it->iol_len;
        it = it->iol_next;
        dsock_assert(it);
    }
    self->first = *it;
    self->first.iol_base += offset;
    self->first.iol_len -= offset;
    it = &self->first;
    while(len > it->iol_len) {
        len -= it->iol_len;
        it = it->iol_next;
        dsock_assert(it);
    }
    self->oldlast = *it;
    self->last = it;
    it->iol_len = len;
    it->iol_next = NULL;
}

void iol_slice_term(struct iol_slice *self) {
    *self->last = self->oldlast;
}

int iol_deep_copy(struct iolist *dst, struct iolist *src) {
    if(!dst || !src) {errno = EINVAL; return -1;}
    size_t dst_size = iol_size(dst);
    size_t src_size = iol_size(src);
    if(dst_size < src_size) {errno = EINVAL; return -1;}
    size_t remaining = src_size;
    struct iolist *src_it = src;
    size_t src_block_remain = src_it->iol_len;
    struct iolist *dst_it = dst;
    size_t dst_block_remain = dst_it->iol_len;
    while(1) {
        size_t to_copy = MIN(src_block_remain, dst_block_remain);
        memcpy(dst_it->iol_base + dst_it->iol_len - dst_block_remain,
               src_it->iol_base + src_it->iol_len - src_block_remain,
               to_copy);
        dst_block_remain -= to_copy;
        src_block_remain -= to_copy;
        remaining -= to_copy;
        if(remaining == 0)
            break;
        dsock_assert(dst_block_remain == 0 || src_block_remain == 0);
        if(dst_block_remain == 0) {
            dst_it = dst_it->iol_next;
            dst_block_remain = dst_it->iol_len;
        }
        if(src_block_remain == 0) {
            src_it = src_it->iol_next;
            src_block_remain = src_it->iol_len;
        }
    }
    return 0;
}

