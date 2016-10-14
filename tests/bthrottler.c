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

int main() {
    int s[2];

    /* send-throttling: One big batch split into multiple bursts. */
    int rc = unix_pair(s);
    assert(rc == 0);
    int thr = bthrottler_start(s[0], 1000, 10, 0, 0);
    assert(thr >= 0);
    char buf[200];
    int64_t start = now();
    rc = bsend(thr, buf, 95, -1);
    assert(rc == 0);
    int64_t elapsed = now() - start;
    assert(elapsed > 80 && elapsed < 110);
    rc = brecv(s[1], buf, 95, -1);
    assert(rc == 0);
    hclose(thr);
    hclose(s[1]);

    /* send-throttling: Multiple small batches in two bursts. */
    rc = unix_pair(s);
    assert(rc == 0);
    thr = bthrottler_start(s[0], 1000, 10, 0, 0);
    assert(thr >= 0);
    start = now();
    int i;
    for(i = 0; i != 50; ++i) {
        rc = bsend(thr, buf, 3, -1);
        assert(rc == 0);
    }
    elapsed = now() - start;
    assert(elapsed > 130 && elapsed < 150);
    rc = brecv(s[1], buf, 150, -1);
    assert(rc == 0);
    hclose(thr);
    hclose(s[1]);

    /* recv-throttling: One big batch split into multiple bursts. */
    rc = unix_pair(s);
    assert(rc == 0);
    thr = bthrottler_start(s[0], 0, 0, 1000, 10);
    assert(thr >= 0);
    rc = bsend(s[1], buf, 95, -1);
    assert(rc == 0);
    start = now();
    rc = brecv(thr, buf, 95, -1);
    assert(rc == 0);
    elapsed = now() - start;
    assert(elapsed > 80 && elapsed < 100);
    hclose(thr);
    hclose(s[1]);

    /* recv-throttling: Multiple small batches in two bursts. */
    rc = unix_pair(s);
    assert(rc == 0);
    thr = bthrottler_start(s[0], 0, 0, 1000, 10);
    assert(thr >= 0);
    rc = bsend(s[1], buf, 150, -1);
    assert(rc == 0);
    start = now();
    for(i = 0; i != 50; ++i) {
        rc = brecv(thr, buf, 3, -1);
        assert(rc == 0);
    }
    elapsed = now() - start;
    assert(elapsed > 130 && elapsed < 150);
    hclose(thr);
    hclose(s[1]);

    return 0;
}

