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
    int rc;
    ssize_t sz;
    char buf[200];
    const char key[] = "01234567890123456789012345678901";

    int h1[2];
    rc = unix_pair(h1);
    assert(rc == 0);
    int h2_0 = blog_start(h1[0]);
    assert(h2_0 >= 0);
    int h2_1 = blog_start(h1[1]);
    assert(h2_1 >= 0);
    int h3_0 = bthrottler_start(h2_0, 1000, 10, 1000, 10);
    assert(h3_0 >= 0);
    int h3_1 = bthrottler_start(h2_1, 1000, 10, 1000, 10);
    assert(h3_1 >= 0);
    int h4_0 = nagle_start(h3_0, 2000, 100);
    assert(h4_0 >= 0);
    int h4_1 = nagle_start(h3_1, 2000, 100);
    assert(h4_0 >= 0);
    int h5_0 = pfx_start(h4_0);
    assert(h5_0 >= 0);
    int h5_1 = pfx_start(h4_1);
    assert(h5_1 >= 0);
    int h6_0 = keepalive_start(h5_0, 50, 150);
    assert(h6_0 >= 0);
    int h6_1 = keepalive_start(h5_1, 50, 150);
    assert(h6_0 >= 0);
    int h7_0 = nacl_start(h6_0, key, 32, -1);
    assert(h7_0 >= 0);
    int h7_1 = nacl_start(h6_1, key, 32, -1);
    assert(h7_1 >= 0);
    int h8_0 = lz4_start(h7_0);
    assert(h8_0 >= 0);
    int h8_1 = lz4_start(h7_1);
    assert(h8_1 >= 0);

    rc = msend(h8_0, "ABC", 3, -1);
    assert(rc == 0);
    rc = msend(h8_0, "DEF", 3, -1);
    assert(rc == 0);
    sz = mrecv(h8_1, buf, 3, -1);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    sz = mrecv(h8_1, buf, 3, -1);
    assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = msend(h8_1, "GHI", 3, -1);
    assert(rc == 0);
    /* Allow some keepalives to be sent. */
    rc = msleep(500);
    assert(rc == 0);
    sz = mrecv(h8_0, buf, 3, -1);
    assert(sz == 3);
    assert(buf[0] == 'G' && buf[1] == 'H' && buf[2] == 'I');


    rc = hclose(h8_1);
    assert(rc == 0);
    rc = hclose(h8_0);
    assert(rc == 0);

    return 0;
}

