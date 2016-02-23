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

#include "../dillsocks.h"

#include "create_connection.inc"

int main(void) {
    int s[2];
    create_tcp_connection(s);
    int sf0 = sfattach(s[0]);
    assert(sf0 >= 0);

    /* Check whether sent message is properly framed. */
    int rc = socksend(sf0, "ABC", 3, -1);
    assert(rc == 0);
    size_t sz;
    char buf[11];
    rc = sockrecv(s[1], buf, 11, &sz, -1);
    assert(rc == 0);
    assert(sz == 11);
    assert(memcmp(buf, "\x00\x00\x00\x00\x00\x00\x00\x03" "ABC", 11) == 0);

    /* Simple sf-based ping-pong. */
    int sf1 = sfattach(s[1]);
    assert(sf1 >= 0);
    rc = socksend(sf1, "DEF", 3, -1);
    assert(rc == 0);
    rc = sockrecv(sf0, buf, 3, &sz, -1);
    assert(rc == 0);
    assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = socksend(sf0, "GHI", 3, -1);
    assert(rc == 0);
    rc = sockrecv(sf1, buf, 3, &sz, -1);
    assert(rc == 0);
    assert(sz == 3);
    assert(buf[0] == 'G' && buf[1] == 'H' && buf[2] == 'I');

    /* Terminate the framing protocol. */
    int u;
    rc = sfdetach(sf0, &u, -1);
    assert(rc == 0);
    assert(u == s[0]);
    rc = sfdetach(sf1, &u, -1);
    assert(rc == 0);
    assert(u == s[1]);
    rc = tcpclose(s[0], -1);
    assert(rc == 0);
    rc = tcpclose(s[1], -1);
    assert(rc == 0);

    return 0;
}
