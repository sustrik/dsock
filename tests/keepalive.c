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

#include "../dsock.h"

static void keepalive_pair(int h[2], int both) {
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
    assert(mlog1 >= 0);
    h[0] = keepalive_start(mlog0, 50, 150, "KEEPALIVE", 9);
    assert(h[0] >= 0);
    h[1] = mlog1;
    if(both) {
        h[1] = keepalive_start(mlog1, 50, 150, "KEEPALIVE", 9);
        assert(h[1] >= 0);
    }
}

static void keepalive_pair_close(int h[2]) {
    int rc = hclose(h[1]);
    assert(rc == 0);
    rc = hclose(h[0]);
    assert(rc == 0);
}

int main() {
    int rc;
    ssize_t sz;
    int h[2];
    char buf[32];
    int64_t start;
    int64_t elapsed;

    /* Check whether keepalives are being sent. */
    keepalive_pair(h, 0);
    start = now();
    sz = mrecv(h[1], buf, sizeof(buf), -1);
    assert(sz == 9);
    elapsed = now() - start;
    assert(elapsed > 40 && elapsed < 60);
    keepalive_pair_close(h);

    /* Check whether keepalives are filtered out. */
    keepalive_pair(h, 1);
    start = now();
    sz = mrecv(h[1], buf, sizeof(buf), now() + 300);
    assert(sz < 0 && errno == ETIMEDOUT);
    elapsed = now() - start;
    assert(elapsed > 280 && elapsed < 320);
    keepalive_pair_close(h);

    return 0;
}

