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

#ifndef DSOCK_H_INCLUDED
#define DSOCK_H_INCLUDED

#include <libdill.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DSOCK_VERSION_CURRENT 1

/*  The latest revision of the current interface. */
#define DSOCK_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DSOCK_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if !defined __GNUC__ && !defined __clang__
#error "Unsupported compiler!"
#endif

#if DSOCK_NO_EXPORTS
#define DSOCK_EXPORT
#else
#define DSOCK_EXPORT __attribute__ ((visibility("default")))
#endif

/* Old versions of GCC don't support visibility attribute. */
#if defined __GNUC__ && __GNUC__ < 4
#undef DSOCK_EXPORT
#define DSOCK_EXPORT
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

DSOCK_EXPORT int iplocal(
    ipaddr *addr,
    const char *name,
    int port,
    int mode);
DSOCK_EXPORT int ipremote(
    ipaddr *addr,
    const char *name,
    int port,
    int mode,
    int64_t deadline);
DSOCK_EXPORT const char *ipaddrstr(
    const ipaddr *addr,
    char *ipstr);
DSOCK_EXPORT int ipfamily(
    const ipaddr *addr);
DSOCK_EXPORT const struct sockaddr *ipsockaddr(
    const ipaddr *addr);
DSOCK_EXPORT int iplen(
    const ipaddr *addr);
DSOCK_EXPORT int ipport(
    const ipaddr *addr);
DSOCK_EXPORT void ipsetport(
    ipaddr *addr,
    int port);

/******************************************************************************/
/*  Bytestream sockets                                                        */
/******************************************************************************/

DSOCK_EXPORT int bsend(
    int s,
    const void *buf,
    size_t *len,
    int64_t deadline);
DSOCK_EXPORT int brecv(
    int s,
    void *buf,
    size_t *len,
    int64_t deadline);

/******************************************************************************/
/*  Message sockets                                                           */
/******************************************************************************/

DSOCK_EXPORT int msend(
    int s,
    const void *buf,
    size_t *len,
    int64_t deadline);
DSOCK_EXPORT int mrecv(
    int s,
    void *buf,
    size_t *len,
    int64_t deadline);

/******************************************************************************/
/*  TCP library                                                               */
/******************************************************************************/

DSOCK_EXPORT int tcplisten(
    ipaddr *addr,
    int backlog);
DSOCK_EXPORT int tcpaccept(
    int s,
    ipaddr *addr,
    int64_t deadline);
DSOCK_EXPORT int tcpconnect(
    const ipaddr *addr,
    int64_t deadline);

#define tcpsend bsend
#define tcprecv brecv

/******************************************************************************/
/*  UNIX library                                                              */
/******************************************************************************/

DSOCK_EXPORT int unixlisten(
    const char *addr,
    int backlog);
DSOCK_EXPORT int unixaccept(
    int s,
    int64_t deadline);
DSOCK_EXPORT int unixconnect(
    const char *addr,
    int64_t deadline);
DSOCK_EXPORT int unixpair(
    int s[2]);

#define unixsend bsend
#define unixrecv brecv

/******************************************************************************/
/*  UDP library                                                               */
/******************************************************************************/

DSOCK_EXPORT int udpsocket(
    ipaddr *local,
    const ipaddr *remote);
DSOCK_EXPORT int udpsend(
    int s,
    const ipaddr *addr,
    const void *buf,
    size_t *len);
DSOCK_EXPORT int udprecv(
    int s,
    ipaddr *addr,
    void *buf,
    size_t *len,
    int64_t deadline);

#endif

