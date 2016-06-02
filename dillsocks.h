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

#ifndef DILLSOCKS_H_INCLUDED
#define DILLSOCKS_H_INCLUDED

#include <libdill.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/uio.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DILLSOCKS_VERSION_CURRENT 0

/*  The latest revision of the current interface. */
#define DILLSOCKS_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILLSOCKS_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if !defined __GNUC__ && !defined __clang__
#error "Unsupported compiler!"
#endif

#if DILL_NO_EXPORTS
#define DILLSOCKS_EXPORT
#else
#define DILLSOCKS_EXPORT __attribute__ ((visibility("default")))
#endif

/* Old versions of GCC don't support visibility attribute. */
#if defined __GNUC__ && __GNUC__ < 4
#undef DILLSOCKS_EXPORT
#define DILLSOCKS_EXPORT
#endif

/******************************************************************************/
/*  IP address resolution                                                     */
/******************************************************************************/

#define IPADDR_IPV4 1
#define IPADDR_IPV6 2
#define IPADDR_PREF_IPV4 3
#define IPADDR_PREF_IPV6 4
#define IPADDR_MAXSTRLEN 46

typedef struct {char data[32];} ipaddr;

DILLSOCKS_EXPORT int iplocal(ipaddr *addr, const char *name, int port,
    int mode);
DILLSOCKS_EXPORT int ipremote(ipaddr *addr, const char *name, int port,
    int mode, int64_t deadline);
DILLSOCKS_EXPORT const char *ipaddrstr(const ipaddr *addr, char *ipstr);
DILLSOCKS_EXPORT int ipfamily(const ipaddr *addr);
DILLSOCKS_EXPORT const struct sockaddr *ipsockaddr(const ipaddr *addr);
DILLSOCKS_EXPORT int iplen(const ipaddr *addr);
DILLSOCKS_EXPORT int ipport(const ipaddr *addr);

/******************************************************************************/
/*  Basic helper functions                                                    */
/******************************************************************************/

DILLSOCKS_EXPORT int dsunblock(int s);
DILLSOCKS_EXPORT int dsconnect(int s, const struct sockaddr *addr,
    socklen_t addrlen, int64_t deadline);
DILLSOCKS_EXPORT int dsaccept(int s, struct sockaddr *addr, socklen_t *addrlen,
    int64_t deadline);
DILLSOCKS_EXPORT int dssend(int s, const void *buf, size_t *len,
    int64_t deadline);
DILLSOCKS_EXPORT int dsrecv(int s, void *buf, size_t *len, int64_t deadline);
DILLSOCKS_EXPORT int dsclose(int s);

/******************************************************************************/
/*  Virtual bytestream socket                                                 */
/******************************************************************************/

/* For implementors. */

struct bsockvfptrs {
    int (*send)(int s, const void *buf, size_t len, int64_t deadline);
    int (*recv)(int s, void *buf, size_t len, int64_t deadline);
    int (*flush)(int s, int64_t deadline);
    int (*finish)(int s, int64_t deadline);
    void (*close)(int s);
};

DILLSOCKS_EXPORT int bsock(const void *type, void *data,
    const struct bsockvfptrs *vfptrs);
DILLSOCKS_EXPORT void *bsockdata(int s, const void *type);

/* For users. */

DILLSOCKS_EXPORT int bsend(int s, const void *buf, size_t len,
    int64_t deadline);
DILLSOCKS_EXPORT int brecv(int s, void *buf, size_t len, int64_t deadline);
DILLSOCKS_EXPORT int bflush(int s, int64_t deadline);

/******************************************************************************/
/*  Virtual message socket                                                    */
/******************************************************************************/

/* For implementors. */

struct msockvfptrs {
    int (*send)(int s, const void *buf, size_t len, int64_t deadline);
    int (*recv)(int s, void *buf, size_t *len, int64_t deadline);
    int (*finish)(int s, int64_t deadline);
    void (*close)(int s);
};

DILLSOCKS_EXPORT int msock(const void *type, void *data,
    const struct msockvfptrs *vfptrs);
DILLSOCKS_EXPORT void *msockdata(int s, const void *type);

/* For users. */

DILLSOCKS_EXPORT int msend(int s, const void *buf, size_t len,
    int64_t deadline);
DILLSOCKS_EXPORT int mrecv(int s, void *buf, size_t *len, int64_t deadline);

/******************************************************************************/
/*  TCP socket                                                                */
/******************************************************************************/

DILLSOCKS_EXPORT int tcplisten(const ipaddr *addr, int backlog);
DILLSOCKS_EXPORT int tcpaccept(int s, int64_t deadline);
DILLSOCKS_EXPORT int tcpconnect(const ipaddr *addr, int64_t deadline);
DILLSOCKS_EXPORT int tcpport(int s);
DILLSOCKS_EXPORT int tcppeer(int s, ipaddr *addr);

/******************************************************************************/
/*  Simple framing socket                                                     */
/******************************************************************************/

DILLSOCKS_EXPORT int sfattach(int s);
DILLSOCKS_EXPORT int sfdetach(int s, int64_t deadline);

#endif
 
