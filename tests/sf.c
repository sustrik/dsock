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

coroutine void worker1(int ch) {
    ipaddr addr;
    int rc = ipremote(&addr, "127.0.0.1", 5555, 0, -1);
    assert(rc == 0);
    int cs = tcpconnect(&addr, -1);
    assert(cs >= 0);
    rc = chsend(ch, &cs, sizeof(cs), -1);
    assert(rc == 0);
}

void create_tcp_connection(int s[2]) {
    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    int lst = tcplisten(&addr, 10);
    assert(lst >= 0);
    int ch = channel(sizeof(int), 0);
    assert(ch >= 0);
    int chndl = go(worker1(ch));
    assert(chndl >= 0);
    s[0] = tcpaccept(lst, -1);
    assert(s[0] >= 0);
    rc = chrecv(ch, &s[1], sizeof(int), -1);
    assert(rc == 0);
    rc = hclose(chndl);
    assert(rc == 0);
    rc = hclose(ch);
    assert(rc == 0);
    rc = hclose(lst);
    assert(rc == 0);
}

coroutine void worker2(int s, int ch) {
    int u = sfdetach(s, -1);
    assert(u >= 0);
    int rc = chsend(ch, &u, sizeof(u), -1);
    assert(rc == 0);
}

void detach_sf(int s[2]) {
    int ch = channel(sizeof(int), 0);
    assert(ch >= 0);
    int chndl = go(worker2(s[0], ch));
    assert(chndl >= 0); 
    s[1] = sfdetach(s[1], -1);
    assert(s[1] >= 0);
    int rc = chrecv(ch, &s[0], sizeof(int), -1);
    assert(rc == 0);
    rc = hclose(chndl);
    assert(rc == 0);
    rc = hclose(ch);
    assert(rc == 0);
}

int main(void) {
    int s[2];
    char buf[16];

    /* Test attach & detach. */
    create_tcp_connection(s);
printf("s0=%d s1=%d\n", s[0], s[1]);
    s[0] = sfattach(s[0]);
    assert(s[0] >= 0);
    s[1] = sfattach(s[1]);
    assert(s[1] >= 0);
    int rc = msend(s[0], "ABC", 3, -1);
    assert(rc == 0);
    size_t len = sizeof(buf);
    rc = mrecv(s[1], buf, &len, -1);
    assert(rc == 0);
    assert(len == 3 && memcmp(buf, "ABC", 3) == 0);
    rc = msend(s[0], "DEF", 3, -1);
    assert(rc == 0);
    detach_sf(s);
printf("s0=%d s1=%d\n", s[0], s[1]);
    rc = bsend(s[0], "GHI", 3, -1);
    assert(rc == 0);
    rc = hfinish(s[0], -1);
    assert(rc == 0);
    rc = brecv(s[1], buf, 3, -1);
    assert(rc == 0);
    assert(len == 3 && memcmp(buf, "GHI", 3) == 0);
    rc = hclose(s[1]);
    assert(rc == 0);

    return 0;
}

