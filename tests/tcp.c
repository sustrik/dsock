
#include <assert.h>
#include <stdio.h>
#include <libdill.h>

#include "../dillsocks.h"

coroutine void client(chan ch) {
    ipaddr addr;
    int rc = ipremote(&addr, "127.0.0.1", 5555, 0, -1);
    assert(rc == 0);
    int cs = tcpconnect(&addr, -1);
    assert(cs);
    rc = chsend(ch, &cs, sizeof(cs), -1);
    assert(rc == 0);
}

void create_tcp_connection(int s[2]) {
    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    int lst = tcplisten(&addr, 10);
    assert(lst);
    chan ch = channel(sizeof(int), 0);
    assert(ch);
    int chndl = go(client(ch));
    assert(chndl >= 0);
    s[0] = tcpaccept(lst, -1);
    assert(s[0]);
    rc = chrecv(ch, &s[1], sizeof(int), -1);
    assert(rc == 0);
    rc = stop(&chndl, 1, -1);
    assert(rc == 0);
    chclose(ch);
    rc = stop(&lst, 1, 0);
    assert(rc == 0);
}

int main() {

    /* Open and close listening socket. */
    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    int lst = tcplisten(&addr, 10);
    assert(lst);
    rc = stop(&lst, 1, 0);
    assert(rc == 0);

    /* Create a connection. */
    int s[2];
    create_tcp_connection(s);
    /* Ping. */
    rc = socksend(s[0], "ABC", 3, -1);
    assert(rc == 0);
    char buf[3];
    size_t sz = 3;
    rc = sockrecv(s[1], buf, &sz, -1);
    assert(rc == 0);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    /* Pong. */
    rc = socksend(s[1], "DEF", 3, -1);
    assert(rc == 0);
    sz = 3;
    rc = sockrecv(s[0], buf, &sz, -1);
    assert(rc == 0);
    assert(sz == 3 && buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    /* Shut it down. */
    rc = stop(s, 2, 0);
    assert(rc == 0);

    return 0;
}

