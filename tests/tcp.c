
#include <assert.h>
#include <stdio.h>
#include <libdill.h>

#include "../dillsocks.h"

coroutine void client(void) {
    ipaddr addr;
    int rc = ipremote(&addr, "127.0.0.1", 5555, 0, -1);
    sock cs = tcpconnect(&addr, -1);
    assert(cs);
    char buf[3];
    ssize_t sz = sockrecv(cs, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    sz = socksend(cs, "DEF", 3, -1);
    assert(sz == 3);
    tcpclose(cs, -1);
}

int main() {
    ipaddr addr;
    int rc = iplocal(&addr, NULL, 5555, 0);
    assert(rc == 0);
    sock ls = tcplisten(&addr, 10);
    assert(ls);
    go(client());
    sock as = tcpaccept(ls, -1);
    assert(as);
    ssize_t sz = socksend(as, "ABC", 3, -1);
    assert(sz == 3);
    char buf[3];
    sz = sockrecv(as, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    tcpclose(as, -1);
    tcpclose(ls, -1);

    return 0;
}

