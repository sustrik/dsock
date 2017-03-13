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
#include <string.h>

#include "../dsock.h"

int main() {
    int h[2];
    int rc = ipc_pair(h);
    assert(rc == 0);
    int s0 = websock_attach(h[0], 1);
    assert(s0 >= 0);
    int s1 = websock_attach(h[1], 0);

    rc = msend(s0, "ABC", 3, -1);
    assert(rc == 0);
    char buf[16];
    ssize_t sz = mrecv(s1, buf, sizeof(buf), -1);
    assert(sz == 3 && memcmp(buf, "ABC", 3) == 0);
    rc = msend(s1, "DEF", 3, -1);
    assert(rc == 0);
    sz = mrecv(s0, buf, sizeof(buf), -1);
    assert(sz == 3 && memcmp(buf, "DEF", 3) == 0);

    rc = hclose(s0);
    assert(rc == 0);
    rc = hclose(s1);
    assert(rc == 0);
    return 0;
}

