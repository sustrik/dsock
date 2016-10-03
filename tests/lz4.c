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

#include <assert.h>
#include <string.h>

#include "../dsock.h"

int main() {

    int s[2];
    int rc = unix_pair(s);
    assert(rc == 0);
    int pfx0 = pfx_start(s[0]);
    assert(pfx0 >= 0);
    int pfx1 = pfx_start(s[1]);
    assert(pfx1 >= 0);
    int mlog0 = mlog_start(pfx0);
    assert(mlog0 >= 0);
    int mlog1 = mlog_start(pfx1);
    assert(mlog0 >= 0);
    int lz0 = lz4_start(mlog0);
    assert(lz0 >= 0);
    int lz1 = lz4_start(mlog1);
    assert(lz1 >= 0);
    
    rc = msend(lz0, "123456789012345678901234567890", 30, -1);
    assert(rc == 0);
    uint8_t buf[30];
    size_t sz = mrecv(lz1, buf, 30, -1);
    assert(sz == 30);
    assert(memcmp(buf, "123456789012345678901234567890", 30) == 0);

    rc = hclose(lz1);
    assert(rc == 0);
    rc = hclose(lz0);
    assert(rc == 0);

    return 0;
}

