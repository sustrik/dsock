
#include <assert.h>
#include <stdio.h>
#include <libdill.h>

#include "../dillsocks.h"

static int sec;

coroutine static void
handler(sock as) {
    char buf[16];
    for (;;) {
        int rc = brecv(as, buf, sizeof(buf), -1);
        if (rc && errno == ECANCELED) {
            break;
        }
        printf("text: %s\n", buf);
    }
    tcpclose(as);
}

int
main(int argc, char *argv[]) {
    sock as = NULL;
    coro cr = NULL;
    ipaddr addr;

    iplocal(&addr, NULL, 5555, 0);
    sock ls = tcplisten(&addr, 10);

    for (;;) {
        uint64_t deadline = now() + 1000;
        if (!as) {
            as = tcpaccept(ls, -1);
        } else {
            msleep(deadline);
        }
        if (sec++ >= 2 && cr) {
            gocancel(&cr, 1, 0);
            break;
        }
        if (!cr && as) {
            cr = go(handler(as));
        }
    }

    tcpclose(ls);

    return 0;
}

