/*

  Copyright (c) 2017 Maximilian Pudelko

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

#if 0

#include <memory.h>
#include <assert.h>

#include "../dsock.h"

coroutine void echo_sink(int s) {
    ssize_t rc;
    char buf[32];
    while(1) {
        rc = mrecv(s, buf, 32, -1);
        assert(rc >= 0);
        if(memcmp(buf, "CONTINUE", 8) == 0)
            break;
        rc = msend(s, buf, (size_t) rc, now() + 100);
        assert(rc >= 0);
    }
    rc = hclose(s);
    assert(rc == 0);
}

int main() {
    ssize_t rc;
    char buf[32];
    int fds[2];
    rc = inproc_pair(fds);
    assert(rc >= 0);
    int g = go(echo_sink(fds[1]));
    rc = msend(fds[0], "ABC", 3, now() + 100);
    assert(rc == 0);
    rc = mrecv(fds[0], buf, 32, now() + 100);
    assert(rc >= 0);
    assert(memcmp(buf, "ABC", 3) == 0);

    rc = msend(fds[0], "CONTINUE", 8, now() + 100);
    assert(rc == 0);
    rc = mrecv(fds[0], buf, 32, -1);
    assert(rc == -1);
    assert(errno == EPIPE);
    rc = hclose(fds[0]);
    assert(rc == 0);
    rc = hclose(g);
    assert(rc == 0);
    return 0;
}

#endif

int main(void) {
    return 0;
}

