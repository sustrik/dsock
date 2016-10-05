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
#include <sys/stat.h>
#include <unistd.h>

#include "../dsock.h"

#define TESTADDR "unix.test"

coroutine void client(void) {
    int cs = unix_connect(TESTADDR, -1);
    assert(cs >= 0);

    int rc = msleep(now() + 100);
    assert(rc == 0);

    char buf[16];
    char buffer[2048];
    rc = unix_recv(cs, buf, 3, -1);
    assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = unix_recv(cs, buf, 2, -1);
    assert(rc == 0);
    rc = unix_recv(cs, buf, 1, -1);
    assert(rc == 0);
    assert(buf[0] == 'F');
    rc = unix_recv(cs, buffer, sizeof(buffer), -1);
    assert(rc == 0);
    rc = unix_send(cs, "456", 3, -1);
    assert(rc == 0);

    rc = hclose(cs);
    assert(rc == 0);
}

coroutine void client2(int s) {
    int rc = msleep(now() + 100);
    assert(rc == 0);
    rc = hclose(s);
    assert(rc == 0);
}

int main() {
    char buf[16];
    char buffer[2048];

    struct stat st;
    if (stat(TESTADDR, &st) == 0) {
        assert(unlink(TESTADDR) == 0);
    }

    int ls = unix_listen(TESTADDR, 10);
    assert(ls >= 0);

    go(client());

    int as = unix_accept(ls, -1);

    /* Test deadline. */
    int64_t deadline = now() + 30;
    int rc = unix_recv(as, buf, sizeof(buf), deadline);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    /* Test simple passing of data. */
    rc = unix_send(as, "ABC", 3, -1);
    assert(rc == 0);
    rc = unix_send(as, "DEF", 3, -1);
    assert(rc == 0);
    rc = unix_send(as, buffer, sizeof(buffer), -1);
    assert(rc == 0);
    rc = unix_recv(as, buf, 3, -1);
    assert(rc == 0);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '6');

    rc = hclose(as);
    assert(rc == 0);
    rc = hclose(ls);
    assert(rc == 0);
    if (stat(TESTADDR, &st) == 0) {
        assert(unlink(TESTADDR) == 0);
    }

    /* Test whether we perform correctly when faced with TCP pushback. */
    int hndls[2];
    rc = unix_pair(hndls);
    go(client2(hndls[1]));
    assert(rc == 0);
    while(1) {
        rc = unix_send(hndls[0], buffer, 2048, -1);
        if(rc == -1 && errno == ECONNRESET)
            break;
        assert(rc == 0);
    }
    rc = hclose(hndls[0]);
    assert(rc == 0);

    return 0;
}

