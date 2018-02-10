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

#if 1

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../dsock.h"

coroutine void client(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    assert(cs >= 0);
    int tcs = btls_attach_client(cs,
            DSOCK_BTLS_DEFAULT |
            DSOCK_BTLS_NO_VERIFY_NAME |
            DSOCK_BTLS_NO_VERIFY_CERT |
            DSOCK_BTLS_NO_VERIFY_TIME,
            0, NULL, NULL, NULL);
    assert(tcs >= 0);

    rc = msleep(now() + 100);
    assert(rc == 0);

    char buf[3];
    printf("recving\n");
    rc = brecv(tcs, buf, sizeof(buf), -1);
    assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    rc = bsend(tcs, "456", 3, -1);
    assert(rc == 0);

    rc = hclose(tcs);
    assert(rc == 0);
}

coroutine void client2(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    assert(cs >= 0);
    int tcs = btls_attach_client(cs,
            DSOCK_BTLS_DEFAULT |
            DSOCK_BTLS_NO_VERIFY_NAME |
            DSOCK_BTLS_NO_VERIFY_CERT |
            DSOCK_BTLS_NO_VERIFY_TIME,
            0, NULL, NULL, NULL);
    assert(tcs >= 0);
    rc = msleep(now() + 100);
    assert(rc == 0);
    rc = hclose(tcs);
    assert(rc == 0);
}


int main() {
    char buf[16];

    /* Create a tcp/tls socket */
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    assert(ls >= 0);

    /* Initialise a key pair */
    struct btls_kp kp;
    size_t certlen, keylen;
    uint8_t *cert = btls_loadfile("./tests/cert.pem", &certlen, NULL);
    uint8_t *key = btls_loadfile("./tests/key.pem", &keylen, NULL);
    rc = btls_kp(&kp, cert, certlen, key, keylen);
    assert(rc == 0);

    /* Start the tls server listener */
    int tls = btls_attach_server(ls, DSOCK_BTLS_DEFAULT, 0, &kp, 1, NULL, NULL);
    assert(tls >= 0);

    go(client(5555));

    int as = tcp_accept(ls, NULL, -1);
    assert(as >= 0);
    int tas = btls_attach_accept(as, tls);
    assert(tas >= 0);

    /* Test deadline. */
    int64_t deadline = now() + 30;
    rc = brecv(tas, buf, sizeof(buf), deadline);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    printf("sending\n");
    rc = bsend(tas, "ABC", 3, -1);
    assert(rc == 0);
    rc = brecv(tas, buf, 2, -1);
    assert(rc == 0);
    rc = brecv(tas, buf, sizeof(buf), -1);
    assert(rc == -1 && errno == ECONNRESET);

    rc = hclose(tas);
    assert(rc == 0);
    rc = hclose(tls);
    assert(rc == 0);

    /* Test whether we perform correctly when faced with TCP pushback. */
    ls = tcp_listen(&addr, 10);
    tls = btls_attach_server(ls, DSOCK_BTLS_DEFAULT, 0, &kp, 1, NULL, NULL);
    go(client2(5555));
    as = tcp_accept(ls, NULL, -1);
    assert(as >= 0);
    tas = btls_attach_accept(as, tls);
    assert(tas >= 0);
    char buffer[2048];
    while(1) {
        rc = bsend(tas, buffer, 2048, -1);
        if(rc == -1 && errno == ECONNRESET)
            break;
        printf("%d %d\n", rc, errno);
        assert(rc == 0);
    }
    rc = hclose(as);
    assert(rc == 0);
    rc = hclose(ls);
    assert(rc == 0);

    /* Free the key pairs */
    free(cert);
    free(key);

    return 0;
}

#else

int main(void) {
    return 0;
}

#endif

