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

#include "../dillsocks.h"

#include "create_connection.inc"

int main(void) {
    int conn1[2];
    create_tcp_connection(conn1);
    int conn2[2];
    create_tcp_connection(conn2);

    int cons = consattach(conn1[0], conn2[0]);
    assert(cons >= 0);

    int rc = socksend(cons, "ABC", 3, -1);
    assert(rc == 0);
    char buf[3];
    rc = sockrecv(conn2[1], buf, 3, NULL, -1);
    assert(rc == 0 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    rc = socksend(conn1[1], "DEF", 3, -1);
    assert(rc == 0);
    rc = sockrecv(cons, buf, 3, NULL, -1);
    assert(rc == 0 && buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');

    int s1, s2;
    rc = consdetach(cons, &s1, &s2);
    assert(rc == 0);
    assert(s1 == conn1[0]);
    assert(s2 == conn2[0]);

    hclose(conn1[0]);
    hclose(conn1[1]);
    hclose(conn2[0]);
    hclose(conn2[1]);

    return 0;
}
