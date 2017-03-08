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

#include <assert.h>

#include "../dsock.h"

int main() {

    /* Test whether big chunk gets through. */
    int s[2];
    int rc = ipc_pair(s);
    assert(rc == 0);
    int n = nagle_start(s[0], 5, -1);
    assert(n >= 0);
    rc = bsend(n, "123456789", 9, -1);
    assert(rc == 0);
    char buf[9];
    rc = brecv(s[1], buf, 9, -1);
    assert(rc == 0);
    rc = hclose(s[1]);
    assert(rc == 0);
    rc = hclose(n);
    assert(rc == 0);

    /* Test whether several small chunks get through. */
    rc = ipc_pair(s);
    assert(rc == 0);
    n = nagle_start(s[0], 5, -1);
    assert(n >= 0);
    rc = bsend(n, "12", 2, -1);
    assert(rc == 0);
    rc = bsend(n, "34567", 5, -1);
    assert(rc == 0);
    rc = brecv(s[1], buf, 7, -1);
    assert(rc == 0);
    rc = hclose(s[1]);
    assert(rc == 0);
    rc = hclose(n);
    assert(rc == 0);

    /* Infinite interval: Test that single small chunk doesn't get through. */
    rc = ipc_pair(s);
    assert(rc == 0);
    n = nagle_start(s[0], 5, -1);
    assert(n >= 0);
    rc = bsend(n, "12", 2, -1);
    assert(rc == 0);
    rc = brecv(s[1], buf, 2, now() + 100);
    assert(rc < 0 && errno == ETIMEDOUT);
    rc = hclose(s[1]);
    assert(rc == 0);
    rc = hclose(n);
    assert(rc == 0);

    /* Finite interval: Test that single small chunk does get through. */
    rc = ipc_pair(s);
    assert(rc == 0);
    n = nagle_start(s[0], 5, 50);
    assert(n >= 0);
    rc = bsend(n, "12", 2, -1);
    assert(rc == 0);
    rc = brecv(s[1], buf, 2, -1);
    assert(rc == 0);
    rc = hclose(s[1]);
    assert(rc == 0);
    rc = hclose(n);
    assert(rc == 0);

    return 0;
}

