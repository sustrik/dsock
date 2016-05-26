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

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>

#include "../dillsocks.h"

coroutine void client(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    assert(s >= 0);
    int rc = dsunblock(s);
    assert(rc == 0);
    ipaddr addr;
    rc = ipremote(&addr, "127.0.0.1", 5555, 0, -1);
    assert(rc == 0);
    rc = dsconnect(s, (struct sockaddr*)&addr, iplen(&addr), -1);
    assert(rc == 0);
    size_t len = 3;
    rc = dssend(s, "ABC", &len, -1);
    assert(rc == 0);
    assert(len == 3);
    fdclean(s);
    dsclose(s);
}

int main(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    assert(s >= 0);

    /* This allows the test to run multiple times in short intervals. */
    int opt = 1;
    int rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    assert(rc == 0);

    rc = dsunblock(s);
    assert(rc == 0);
    ipaddr addr;
    rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    rc = bind(s, (struct sockaddr*)&addr, iplen(&addr));
    assert(rc == 0);
    rc = listen(s, 10);
    assert(rc == 0);
    go(client());
    int as = dsaccept(s, NULL, NULL, -1);
    assert(as >= 0);
    char buf[3];
    size_t len = 3;
    rc = dsrecv(as, buf, &len, -1);
    assert(rc == 0);
    assert(len == 3);
    assert(memcmp(buf, "ABC", 3) == 0);
    dsclose(as);
    dsclose(s);

    return 0;
}

