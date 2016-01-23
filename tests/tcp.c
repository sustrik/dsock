/*

  Copyright (c) 2015 Martin Sustrik

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
#include <libdill.h>
#include <string.h>

#include "../dillsocks.h"

coroutine void client(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    sock cs = tcp_connect(addr, -1);
    assert(cs);

    bsend(cs, "123456", 6, -1);
    assert(errno == 0);
    bflush(cs, -1);
    assert(errno == 0);

    msleep(now() + 100);

    bsend(cs, "ABCDEF", 6, -1);
    assert(errno == 0);
    bflush(cs, -1);
    assert(errno == 0);

    tcp_close(cs);
}

int main(void) {
    /* Create a connection. */
    sock ls = tcp_listen(iplocal(NULL, 5555, 0), 10);
    assert(ls);
    go(client(5555));
    sock as = tcp_accept(ls, -1);

    /* Test retrieving address and port. */
    ipaddr addr = tcp_addr(as);
    char ipstr[IPADDR_MAXSTRLEN];
    assert(strcmp(ipaddrstr(addr, ipstr), "127.0.0.1") == 0);
    assert(tcp_port(as) != 5555);

    /* Test reading data, both with an without the deadline. */
    char buf[10];
    brecv(as, buf, 4, -1);
    assert(errno == 0 && memcmp(buf, "1234", 4) == 0);
    int64_t deadline = now() + 50;
    brecv(as, buf, 4, deadline);
    assert(errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);
    brecv(as, buf, 8, -1);
    assert(errno == 0 && memcmp(buf, "56ABCDEF", 8) == 0);

    tcp_close(as);
    tcp_close(ls);

    return 0;
}
