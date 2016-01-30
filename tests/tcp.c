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
#include <stdlib.h>

#include "../dillsocks.h"

coroutine void client(int port) {
    ipaddr addr;
    int rc = ipremote(&addr, "127.0.0.1", port, 0, -1);
    assert(rc == 0);
    sock cs = tcpconnect(&addr, -1);
    assert(cs);

    char ipstr[16] = {0};
    const char *str = ipaddrstr(&addr, ipstr);
    assert(str);
    assert(strcmp(ipstr, "127.0.0.1") == 0);

    msleep(now() + 100);

    char buf[16];
    rc = brecv(cs, buf, 3, -1);
    assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(cs, buf, 2, -1);
    assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E');
    rc = brecv(cs, buf, 4, -1);
    assert(rc == 0);
    assert(buf[0] == 'F' && buf[1] == 'G' && buf[2] == 'H' && buf[3] == 'I');

    rc = bsend(cs, "JKL", 3, -1);
    assert(rc == 0);
    rc = bflush(cs, -1);
    assert(rc == 0);

    tcpclose(cs);
}

int main(void) {
    char buf[16];

    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    sock ls = tcplisten(&addr, 10);
    assert(ls);

    coro cr = go(client(5555));

    sock as = tcpaccept(ls, -1);

    /* Test retrieving address and port. */
    rc = tcppeer(as, &addr);
    assert(rc == 0);
    char ipstr[IPADDR_MAXSTRLEN];
    assert(strcmp(ipaddrstr(&addr, ipstr), "127.0.0.1") == 0);
    assert(tcpport(as) != 5555);

    /* Test deadline. */
    int64_t deadline = now() + 30;
    rc = brecv(as, buf, sizeof(buf), deadline);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    rc = bsend(as, "ABC", 3, -1);
    assert(rc == 0);
    rc = bflush(as, -1);
    assert(rc == 0);
    rc = bsend(as, "DEF", 3, -1);
    assert(rc == 0);
    rc = bsend(as, "GHI", 3, -1);
    assert(rc == 0);
    rc = bflush(as, -1);
    assert(rc == 0);

    rc = brecv(as, buf, 3, -1);
    assert(rc == 0);
    assert(buf[0] == 'J' && buf[1] == 'K' && buf[2] == 'L');

    tcpclose(as);
    tcpclose(ls);

    gocancel(&cr, 1, -1);

    return 0;
}

