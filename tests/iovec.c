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
#include "../iov.h"

int deep_copy_test() {
    int rc;
    struct iovec src_vec[2];
    src_vec[0].iov_base = "AB";
    src_vec[0].iov_len = 2;
    src_vec[1].iov_base = "CDE";
    src_vec[1].iov_len = 3;

    /* Basic check */
    char buf[5];
    struct iovec dst_vec1[1] = {{.iov_base = buf, .iov_len = 5}};
    rc = iov_deep_copy(dst_vec1, 1, src_vec, 2);
    assert(rc == 0);
    rc = memcmp(buf, "ABCDE", 5);
    assert(rc == 0);

    /* Dst too small */
    struct iovec dst_vec2[1] = {{.iov_base = buf, .iov_len = 4}};
    rc = iov_deep_copy(dst_vec2, 1, src_vec, 2);
    assert(rc == -1);

    /* Zero length edge case: empty dst */
    struct iovec dst_vec3[1] = {{.iov_base = buf, .iov_len = 0}};
    rc = iov_deep_copy(dst_vec3, 1, src_vec, 2);
    assert(rc == -1);
    
    /* Zero length edge case: empty src */
    struct iovec src_vec2[1] = {{.iov_base = "NNNNNNN", .iov_len = 0}};
    memset(buf, 0, 5);
    rc = iov_deep_copy(dst_vec1, 1, src_vec2, 1);
    assert(rc == 0);
    assert(buf[0] != 'N');

    return 0;
}

int main() {

    int h[2];
    int rc = unix_pair(h);
    struct iovec iov[3];
    iov[0].iov_base = "AB";
    iov[0].iov_len = 2;
    iov[1].iov_base = "CD";
    iov[1].iov_len = 2;
    iov[2].iov_base = "EF";
    iov[2].iov_len = 2;
    rc = bsendv(h[0], iov, 3, -1);
    assert(rc == 0);
    iov[0].iov_base = "GH";
    iov[0].iov_len = 2;
    iov[1].iov_base = "IJ";
    iov[1].iov_len = 2;
    rc = bsendv(h[0], iov, 2, -1);
    assert(rc == 0);

    char buf[10];
    iov[0].iov_base = buf;
    iov[0].iov_len = 2;
    iov[1].iov_base = buf + 2;
    iov[1].iov_len = 3;
    rc = brecvv(h[1], iov, 2, -1);
    assert(rc == 0);
    iov[0].iov_base = buf + 5;
    iov[0].iov_len = 4;
    iov[1].iov_base = buf + 9;
    iov[1].iov_len = 1;
    rc = brecvv(h[1], iov, 2, -1);
    assert(rc == 0);
    assert(memcmp(buf, "ABCDEFGHIJ", 10) == 0);

    rc = hclose(h[0]);
    assert(rc == 0);
    rc = hclose(h[1]);
    assert(rc == 0);

    rc = deep_copy_test();
    assert(rc == 0);

    return 0;
}

