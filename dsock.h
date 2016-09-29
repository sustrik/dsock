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
#define DSOCK_VERSION_CURRENT 2

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
    size_t len,
    int64_t deadline);
DSOCK_EXPORT int brecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);

/******************************************************************************/
/*  Message sockets                                                           */
/******************************************************************************/

DSOCK_EXPORT int msend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
DSOCK_EXPORT ssize_t mrecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);

/******************************************************************************/
/*  TCP protocol                                                              */
/******************************************************************************/

DSOCK_EXPORT int tcp_listen(
    ipaddr *addr,
    int backlog);
DSOCK_EXPORT int tcp_accept(
    int s,
    ipaddr *addr,
    int64_t deadline);
DSOCK_EXPORT int tcp_connect(
    const ipaddr *addr,
    int64_t deadline);
DSOCK_EXPORT int tcp_attach(
    int fd);
DSOCK_EXPORT int tcp_detach(
    int s);

#define tcp_send bsend
#define tcp_recv brecv

/******************************************************************************/
/*  UNIX protocol                                                             */
/******************************************************************************/

DSOCK_EXPORT int unix_listen(
    const char *addr,
    int backlog);
DSOCK_EXPORT int unix_accept(
    int s,
    int64_t deadline);
DSOCK_EXPORT int unix_connect(
    const char *addr,
    int64_t deadline);
DSOCK_EXPORT int unix_attach(
    int fd);
DSOCK_EXPORT int unix_detach(
    int s);
DSOCK_EXPORT int unix_pair(
    int s[2]);
DSOCK_EXPORT int unix_sendfd(
    int s,
    int fd,
    int64_t deadline);
DSOCK_EXPORT int unix_recvfd(
    int s,
    int64_t deadline);

#define unix_send bsend
#define unix_recv brecv

/******************************************************************************/
/*  UDP protocol                                                              */
/******************************************************************************/

DSOCK_EXPORT int udp_socket(
    ipaddr *local,
    const ipaddr *remote);
DSOCK_EXPORT int udp_attach(
    int fd);
DSOCK_EXPORT int udp_detach(
    int s);
DSOCK_EXPORT int udp_send(
    int s,
    const ipaddr *addr,
    const void *buf,
    size_t len);
DSOCK_EXPORT ssize_t udp_recv(
    int s,
    ipaddr *addr,
    void *buf,
    size_t len,
    int64_t deadline);

/******************************************************************************/
/*  Bytestream log                                                            */
/*  Logs both inbound and outbound data into stderr.                          */
/******************************************************************************/

DSOCK_EXPORT int blog_attach(
    int s);
DSOCK_EXPORT int blog_detach(
    int s);

#define blog_send bsend
#define blog_recv brecv

/******************************************************************************/
/*  Nagle's algorithm for bytestreams                                         */
/*  Delays small sends until buffer of size 'batch' is full or timeout        */
/*  'interval' expires.                                                       */
/******************************************************************************/

DSOCK_EXPORT int nagle_attach(
    int s,
    size_t batch,
    int64_t interval);
DSOCK_EXPORT int nagle_detach(
    int s);

#define nagle_send bsend
#define nagle_recv brecv

/******************************************************************************/
/*  PFX protocol                                                              */
/*  Messages are prefixed by 8-byte size in network byte order.               */
/******************************************************************************/

DSOCK_EXPORT int pfx_attach(
    int s);
DSOCK_EXPORT int pfx_detach(
    int s);

#define pfx_send msend
#define pfx_recv mrecv

/******************************************************************************/
/*  CRLF library                                                              */
/*  Messages are delimited by CRLF (0x0d 0x0a) sequences.                     */
/******************************************************************************/

DSOCK_EXPORT int crlf_attach(
    int s);
DSOCK_EXPORT int crlf_detach(
    int s);

#define crlf_send msend
#define crlf_recv mrecv

/******************************************************************************/
/*  Bytestream throttler                                                      */
/*  Throttles the outbound bytestream to send_throughput bytes per second.    */
/*  Sending quota is recomputed every send_interval milliseconds.             */
/*  Throttles the inbound bytestream to recv_throughput bytes per second.     */
/*  Receiving quota is recomputed every recv_interval milliseconds.           */
/******************************************************************************/

DSOCK_EXPORT int bthrottler_attach(
    int s,
    uint64_t send_throughput, 
    int64_t send_interval,
    uint64_t recv_throughput,
    int64_t recv_interval);
DSOCK_EXPORT int bthrottler_detach(
    int s);

#define bthrottler_send bsend
#define bthrottler_recv brecv

/******************************************************************************/
/*  Message throttler                                                         */
/*  Throttles send operations to send_throughput messages per second.         */
/*  Sending quota is recomputed every send_interval milliseconds.             */
/*  Throttles receive operations to recv_throughput messages per second.      */
/*  Receiving quota is recomputed every recv_interval milliseconds.           */
/******************************************************************************/

DSOCK_EXPORT int mthrottler_attach(
    int s,
    uint64_t send_throughput, 
    int64_t send_interval,
    uint64_t recv_throughput,
    int64_t recv_interval);
DSOCK_EXPORT int mthrottler_detach(
    int s);

#define mthrottler_send msend
#define mthrottler_recv mrecv

/******************************************************************************/
/*  LZ4 bytestream compressor                                                 */
/*  Compresses data usin LZ4 compression algorithm.                           */
/******************************************************************************/

DSOCK_EXPORT int lz4_attach(
    int s);
DSOCK_EXPORT int lz4_detach(
    int s);

#define lz4_send bsend
#define lz4_recv brecv

#endif

