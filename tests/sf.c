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
    ipaddr addr;
    int rc = ipremote(&addr, "127.0.0.1", 5555, 0, -1);
    assert(rc == 0);
    int s = tcpconnect(&addr, -1);
    assert(s >= 0);
    int sf = sfattach(s);
    assert(sf >= 0);

    rc = msend(sf, "ABC", 3, -1);
    assert(rc == 0);

    rc = hclose(sf);
    assert(rc == 0);
}

int main(void) {
    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    int ls = tcplisten(&addr, 10);
    assert(ls >= 0);

    go(client());

    int as = tcpaccept(ls, -1);
    assert(as >= 0);
    int sf = sfattach(as);
    assert(sf >= 0);

    char buf[16];
    size_t len = sizeof(buf);
    rc = mrecv(sf, buf, &len, -1);
    assert(rc == 0);
    assert(len == 3 && memcmp(buf, "ABC", 3) == 0);

    rc = hclose(sf);
    assert(rc == 0);
    rc = hclose(ls);
    assert(rc == 0);

    return 0;
}

