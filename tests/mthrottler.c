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

    /* Send-throttling. */
    int rc = unixpair(s);
    assert(rc == 0);
    int pfx0 = pfxattach(s[0]);
    assert(pfx0 >= 0);
    int pfx1 = pfxattach(s[1]);
    assert(pfx1 >= 0);
    int thr = mthrottlerattach(pfx0, 1000, 10, 0, 0);
    assert(thr >= 0);
    int64_t start = now();
    int i;
    for(i = 0; i != 95; ++i) {
        rc = msend(thr, "ABC", 3, -1);
        assert(rc == 0);
    }
    int64_t elapsed = now() - start;
    assert(elapsed > 80 && elapsed < 100);
    char buf[3];
    for(i = 0; i != 95; ++i) {
        ssize_t sz = mrecv(pfx1, buf, sizeof(buf), -1);
        assert(sz == 3);
    }
    hclose(thr);
    hclose(pfx1);

    /* Recv-throttling. */
    rc = unixpair(s);
    assert(rc == 0);
    int crlf0 = crlfattach(s[0]);
    assert(pfx0 >= 0);
    int crlf1 = crlfattach(s[1]);
    assert(pfx1 >= 0);
    thr = mthrottlerattach(crlf0, 0, 0, 1000, 10);
    assert(thr >= 0);
    for(i = 0; i != 95; ++i) {
        rc = msend(crlf1, "ABC", 3, -1);
        assert(rc == 0);
    }
    start = now();
    for(i = 0; i != 95; ++i) {
        rc = mrecv(thr, buf, sizeof(buf), -1);
        assert(rc == 0);
    }
    elapsed = now() - start;
    assert(elapsed > 80 && elapsed < 100);
    hclose(thr);
    hclose(crlf1);

    return 0;
}

