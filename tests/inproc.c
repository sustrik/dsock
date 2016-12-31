#include <memory.h>
#include <assert.h>

#include "../dsock.h"

coroutine void echo_sink(int s) {
    ssize_t rc;
    char buf[32];
    while(1) {
        rc = mrecv(s, buf, 32, -1);
        assert(rc >= 0);
        if(memcmp(buf, "CONTINUE", 8) == 0)
            break;
        rc = msend(s, buf, (size_t) rc, now() + 100);
        assert(rc >= 0);
    }
    rc = hclose(s);
    assert(rc == 0);
}


int main() {
    ssize_t rc;
    char buf[32];
    int fds[2];
    rc = inproc_pair_start(fds);
    assert(rc >= 0);
    int g = go(echo_sink(fds[1]));
    rc = msend(fds[0], "ABC", 3, now() + 100);
    assert(rc == 0);
    rc = mrecv(fds[0], buf, 32, now() + 100);
    assert(rc >= 0);
    assert(memcmp(buf, "ABC", 3) == 0);

    rc = msend(fds[0], "CONTINUE", 8, now() + 100);
    assert(rc == 0);
    rc = mrecv(fds[0], buf, 32, -1);
    assert(rc == -1);
    assert(errno == EPIPE);
    rc = hclose(fds[0]);
    assert(rc == 0);
    rc = hclose(g);
    assert(rc == 0);
    return 0;
}