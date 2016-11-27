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
#include <sys/uio.h>

/******************************************************************************/
/*  ABI versioning support.                                                   */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DSOCK_VERSION_CURRENT 4

/*  The latest revision of the current interface. */
#define DSOCK_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DSOCK_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility.                                                        */
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
/*  IP address resolution.                                                    */
/******************************************************************************/

#define IPADDR_IPV4 1
#define IPADDR_IPV6 2
#define IPADDR_PREF_IPV4 3
#define IPADDR_PREF_IPV6 4
#define IPADDR_MAXSTRLEN 46

typedef struct {char data[32];} ipaddr;

DSOCK_EXPORT int ipaddr_local(
    ipaddr *addr,
    const char *name,
    int port,
    int mode);
DSOCK_EXPORT int ipaddr_remote(
    ipaddr *addr,
    const char *name,
    int port,
    int mode,
    int64_t deadline);
DSOCK_EXPORT const char *ipaddr_str(
    const ipaddr *addr,
    char *ipstr);
DSOCK_EXPORT int ipaddr_family(
    const ipaddr *addr);
DSOCK_EXPORT const struct sockaddr *ipaddr_sockaddr(
    const ipaddr *addr);
DSOCK_EXPORT int ipaddr_len(
    const ipaddr *addr);
DSOCK_EXPORT int ipaddr_port(
    const ipaddr *addr);
DSOCK_EXPORT void ipaddr_setport(
    ipaddr *addr,
    int port);

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

extern const void *bsock_type;

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
DSOCK_EXPORT int bsendv(
    int s,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);
DSOCK_EXPORT int brecvv(
    int s,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

extern const void *msock_type;

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
DSOCK_EXPORT int msendv(
    int s,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);
DSOCK_EXPORT ssize_t mrecvv(
    int s,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);

/******************************************************************************/
/*  TCP protocol.                                                             */
/******************************************************************************/

extern const void *tcp_type;
extern const void *tcp_listener_type;

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
DSOCK_EXPORT int tcp_done(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  UNIX protocol.                                                            */
/******************************************************************************/

extern const void *unix_type;
extern const void *unix_listener_type;

DSOCK_EXPORT int unix_listen(
    const char *addr,
    int backlog);
DSOCK_EXPORT int unix_accept(
    int s,
    int64_t deadline);
DSOCK_EXPORT int unix_connect(
    const char *addr,
    int64_t deadline);
DSOCK_EXPORT int unix_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int unix_pair(
    int s[2]);

/******************************************************************************/
/*  UDP protocol.                                                             */
/******************************************************************************/

extern const void *udp_type;

DSOCK_EXPORT int udp_socket(
    ipaddr *local,
    const ipaddr *remote);
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
DSOCK_EXPORT int udp_sendv(
    int s,
    const ipaddr *addr,
    const struct iovec *iov,
    size_t iovlen);
DSOCK_EXPORT ssize_t udp_recvv(
    int s,
    ipaddr *addr,
    const struct iovec *iov,
    size_t iovlen,
    int64_t deadline);

/******************************************************************************/
/*  PFX protocol.                                                             */
/*  Messages are prefixed by 8-byte size in network byte order.               */
/******************************************************************************/

extern const void *pfx_type;

DSOCK_EXPORT int pfx_start(
    int s);
DSOCK_EXPORT int pfx_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int pfx_stop(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  CRLF protocol.                                                            */
/*  Messages are delimited by CRLF (0x0d 0x0a) sequences.                     */
/******************************************************************************/

extern const void *crlf_type;

DSOCK_EXPORT int crlf_start(
    int s);
DSOCK_EXPORT int crlf_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int crlf_stop(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  HTTP                                                                      */
/******************************************************************************/

extern const void *http_type;

DSOCK_EXPORT int http_start(
    int s);
DSOCK_EXPORT int http_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int http_stop(
    int s,
    int64_t deadline);
DSOCK_EXPORT int http_sendrequest(
    int s,
    const char *command,
    const char *resource,
    int64_t deadline);
DSOCK_EXPORT int http_recvrequest(
    int s,
    char *command,
    size_t commandlen,
    char *resource,
    size_t resourcelen,
    int64_t deadline);
DSOCK_EXPORT int http_sendstatus(
    int s,
    int status,
    const char *reason,
    int64_t deadline);
DSOCK_EXPORT int http_recvstatus(
    int s,
    char *reason,
    size_t reasonlen,
    int64_t deadline);
DSOCK_EXPORT int http_sendfield(
    int s,
    const char *name,
    const char *value,
    int64_t deadline);
DSOCK_EXPORT int http_recvfield(
    int s,
    char *name,
    size_t namelen,
    char *value,
    size_t valuelen,
    int64_t deadline);

/******************************************************************************/
/*  WebSocket protocol.                                                       */
/******************************************************************************/

extern const void *websock_type;

DSOCK_EXPORT int websock_client(
    int s);
DSOCK_EXPORT int websock_server(
    int s);
DSOCK_EXPORT int websock_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int websock_stop(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  NaCl encryption and authentication protocol.                              */
/*  Uses crypto_secretbox_xsalsa20poly1305 algorithm. Key is 32B long.        */
/******************************************************************************/

extern const void *nacl_type;

DSOCK_EXPORT int nacl_start(
    int s,
    const void *key,
    size_t keylen,
    int64_t deadline);
DSOCK_EXPORT int nacl_done(
    int s);
DSOCK_EXPORT int nacl_stop(
    int s);

/******************************************************************************/
/*  LZ4 bytestream compression protocol.                                      */
/*  Compresses data using LZ4 compression algorithm.                          */
/******************************************************************************/

extern const void *lz4_type;

DSOCK_EXPORT int lz4_start(
    int s);
DSOCK_EXPORT int lz4_done(
    int s);
DSOCK_EXPORT int lz4_stop(
    int s);

/******************************************************************************/
/*  Bytestream tracing.                                                       */
/*  Logs both inbound and outbound data into stderr.                          */
/******************************************************************************/

extern const void *btrace_type;

DSOCK_EXPORT int btrace_start(
    int s);
DSOCK_EXPORT int btrace_done(
    int s);
DSOCK_EXPORT int btrace_stop(
    int s);

/******************************************************************************/
/*  Message tracing.                                                          */
/*  Logs both inbound and outbound messages into stderr.                      */
/******************************************************************************/

extern const void *mtrace_type;

DSOCK_EXPORT int mtrace_start(
    int s);
DSOCK_EXPORT int mtrace_done(
    int s);
DSOCK_EXPORT int mtrace_stop(
    int s);

/******************************************************************************/
/*  Nagle's algorithm for bytestreams.                                        */
/*  Delays small sends until buffer of size 'batch' is full or timeout        */
/*  'interval' expires.                                                       */
/******************************************************************************/

extern const void *nagle_type;

DSOCK_EXPORT int nagle_start(
    int s,
    size_t batch,
    int64_t interval);
DSOCK_EXPORT int nagle_done(
    int s,
    int64_t deadline);
DSOCK_EXPORT int nagle_stop(
    int s, int64_t deadline);

/******************************************************************************/
/*  Bytestream throttler.                                                     */
/*  Throttles the outbound bytestream to send_throughput bytes per second.    */
/*  Sending quota is recomputed every send_interval milliseconds.             */
/*  Throttles the inbound bytestream to recv_throughput bytes per second.     */
/*  Receiving quota is recomputed every recv_interval milliseconds.           */
/******************************************************************************/

extern const void *bthrottler_type;

DSOCK_EXPORT int bthrottler_start(
    int s,
    uint64_t send_throughput,
    int64_t send_interval,
    uint64_t recv_throughput,
    int64_t recv_interval);
DSOCK_EXPORT int bthrottler_done(
    int s);
DSOCK_EXPORT int bthrottler_stop(
    int s);

/******************************************************************************/
/*  Message throttler.                                                        */
/*  Throttles send operations to send_throughput messages per second.         */
/*  Sending quota is recomputed every send_interval milliseconds.             */
/*  Throttles receive operations to recv_throughput messages per second.      */
/*  Receiving quota is recomputed every recv_interval milliseconds.           */
/******************************************************************************/

extern const void *mthrottler_type;

DSOCK_EXPORT int mthrottler_start(
    int s,
    uint64_t send_throughput,
    int64_t send_interval,
    uint64_t recv_throughput,
    int64_t recv_interval);
DSOCK_EXPORT int mthrottler_done(
    int s);
DSOCK_EXPORT int mthrottler_stop(
    int s);

/******************************************************************************/
/*  Keep-alives.                                                              */
/*  If there's no messages being sent a keep-alive is sent once every         */
/*  send_interval milliseconds. If no message or keep-alive is received for   */
/*  recv_interval milliseconds an error is reported.                          */
/******************************************************************************/

extern const void *keepalive_type;

DSOCK_EXPORT int keepalive_start(
    int s,
    int64_t send_interval,
    int64_t recv_interval);
DSOCK_EXPORT int keepalive_done(
    int s);
DSOCK_EXPORT int keepalive_stop(
    int s);

/******************************************************************************/
/*  TLS sockets                                                               */
/******************************************************************************/

extern const void *btls_conn_type;
extern const void *btls_listener_type;

#define DSOCK_BTLS_PROTO_BTLSV1_0         (1 << 1)
#define DSOCK_BTLS_PROTO_BTLSV1_1         (1 << 2)
#define DSOCK_BTLS_PROTO_BTLSV1_2         (1 << 3)
#define DSOCK_BTLS_PROTO_BTLSV1 \
	(DSOCK_BTLS_PROTO_BTLSV1_0|DSOCK_BTLS_PROTO_BTLSV1_1|DSOCK_BTLS_PROTO_BTLSV1_2)

#define DSOCK_BTLS_PROTO_ALL     DSOCK_BTLS_PROTO_BTLSV1
#define DSOCK_BTLS_PROTO_DEFAULT DSOCK_BTLS_PROTO_BTLSV1_2
#define DSOCK_BTLS_PROTO_VALUE(x)         (x & 0xf)

#define DSOCK_BTLS_FLAGS_RESERVED_0       (0 << 4) /* new TLS 1.3 */
#define DSOCK_BTLS_FLAGS_RESERVED_1       (0 << 5) /* new TLS ver? */

#define DSOCK_BTLS_PREFER_CIPHERS_CLIENT  (0 << 6) /* default */
#define DSOCK_BTLS_PREFER_CIPHERS_SERVER  (1 << 6)
#define DSOCK_BTLS_NO_VERIFY_CERT         (1 << 7)
#define DSOCK_BTLS_NO_VERIFY_NAME         (1 << 8)
#define DSOCK_BTLS_NO_VERIFY_TIME         (1 << 9)
#define DSOCK_BTLS_VERIFY_CLIENT          (1 << 10)
#define DSOCK_BTLS_VERIFY_CLIENT_OPTIONAL (1 << 11)
#define DSOCK_BTLS_CLEAR_KEYS             (1 << 12)

#define DSOCK_BTLS_DHEPARAMS_NONE         (0 << 13) /* default */
#define DSOCK_BTLS_DHEPARAMS_AUTO         (1 << 13)
#define DSOCK_BTLS_DHEPARAMS_LEGACY       (2 << 13)
#define DSOCK_BTLS_DHEPARAMS_VALUE(x)     ((x >> 13) & 0x3)

#define DSOCK_BTLS_ECDHECURVE_NONE        (1 << 15)
#define DSOCK_BTLS_ECDHECURVE_AUTO        (0 << 15) /* default */
#define DSOCK_BTLS_ECDHECURVE_SECP192R1   (2 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP224R1   (3 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP224K1   (4 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP256R1   (5 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP256K1   (6 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP384R1   (7 << 15)
#define DSOCK_BTLS_ECDHECURVE_SECP521R1   (8 << 15)
#define DSOCK_BTLS_ECDHECURVE_VALUE(x)    ((x >> 15) & 0xf)

#define DSOCK_BTLS_CIPHERS_DEFAULT        (1 << 19)
#define DSOCK_BTLS_CIPHERS_SECURE         (1 << 19) /* default */
#define DSOCK_BTLS_CIPHERS_COMPAT         (2 << 19)
#define DSOCK_BTLS_CIPHERS_LEGACY         (3 << 19)
#define DSOCK_BTLS_CIPHERS_INSECURE       (4 << 19)
#define DSOCK_BTLS_CIPHERS_SPECIFIC       (5 << 19) /* see list below */
#define DSOCK_BTLS_CIPHERS_VALUE(x)       ((x >> 19) & 0x7)

#define DSOCK_BTLS_VERIFY_DEPTH_DEFAULT   (6 << 22)
#define DSOCK_BTLS_VERIFY_DEPTH(X)        (X << 22)
#define DSOCK_BTLS_VERIFY_DEPTH_MAX       (1 << 27)
#define DSOCK_BTLS_VERIFY_VALUE(x)        ((x >> 22) & 0x1f)

/* BTLS v1.2 ciphers */
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_AES256_GCM_SHA384       (1ull <<  0)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_AES256_GCM_SHA384     (1ull <<  1)
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_AES256_SHA384           (1ull <<  2)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_AES256_SHA384         (1ull <<  3)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_AES256_GCM_SHA384         (1ull <<  4)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_AES256_GCM_SHA384         (1ull <<  5)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_AES256_SHA256             (1ull <<  6)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_AES256_SHA256             (1ull <<  7)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_CHACHA20_POLY1305     (1ull <<  8)
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_CHACHA20_POLY1305       (1ull <<  9)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_CHACHA20_POLY1305         (1ull << 10)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_CHACHA20_POLY1305_OLD (1ull << 11)
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_CHACHA20_POLY1305_OLD   (1ull << 12)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_CHACHA20_POLY1305_OLD     (1ull << 13)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_CAMELLIA256_SHA256        (1ull << 14)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_CAMELLIA256_SHA256        (1ull << 15)
#define DSOCK_BTLS_CIPHERS_ECDH_RSA_AES256_GCM_SHA384        (1ull << 16)
#define DSOCK_BTLS_CIPHERS_ECDH_ECDSA_AES256_GCM_SHA384      (1ull << 17)
#define DSOCK_BTLS_CIPHERS_ECDH_RSA_AES256_SHA384            (1ull << 18)
#define DSOCK_BTLS_CIPHERS_ECDH_ECDSA_AES256_SHA384          (1ull << 18)
#define DSOCK_BTLS_CIPHERS_AES256_GCM_SHA384                 (1ull << 19)
#define DSOCK_BTLS_CIPHERS_AES256_SHA256                     (1ull << 20)
#define DSOCK_BTLS_CIPHERS_CAMELLIA256_SHA256                (1ull << 21)
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_AES128_GCM_SHA256       (1ull << 22)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_AES128_GCM_SHA256     (1ull << 23)
#define DSOCK_BTLS_CIPHERS_ECDHE_RSA_AES128_SHA256           (1ull << 24)
#define DSOCK_BTLS_CIPHERS_ECDHE_ECDSA_AES128_SHA256         (1ull << 25)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_AES128_GCM_SHA256         (1ull << 26)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_AES128_GCM_SHA256         (1ull << 27)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_AES128_SHA256             (1ull << 28)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_AES128_SHA256             (1ull << 29)
#define DSOCK_BTLS_CIPHERS_DHE_RSA_CAMELLIA128_SHA256        (1ull << 30)
#define DSOCK_BTLS_CIPHERS_DHE_DSS_CAMELLIA128_SHA256        (1ull << 31)
#define DSOCK_BTLS_CIPHERS_ECDH_RSA_AES128_GCM_SHA256        (1ull << 32)
#define DSOCK_BTLS_CIPHERS_ECDH_ECDSA_AES128_GCM_SHA256      (1ull << 33)
#define DSOCK_BTLS_CIPHERS_ECDH_RSA_AES128_SHA256            (1ull << 34)
#define DSOCK_BTLS_CIPHERS_ECDH_ECDSA_AES128_SHA256          (1ull << 35)
#define DSOCK_BTLS_CIPHERS_AES128_GCM_SHA256                 (1ull << 36)
#define DSOCK_BTLS_CIPHERS_AES128_SHA256                     (1ull << 37)
#define DSOCK_BTLS_CIPHERS_CAMELLIA128_SHA256                (1ull << 38)

#define DSOCK_BTLS_DEFAULT \
    (DSOCK_BTLS_PROTO_DEFAULT| \
     DSOCK_BTLS_DHEPARAMS_NONE| \
     DSOCK_BTLS_ECDHECURVE_AUTO| \
     DSOCK_BTLS_CIPHERS_DEFAULT| \
     DSOCK_BTLS_VERIFY_DEPTH_DEFAULT| \
     DSOCK_BTLS_PREFER_CIPHERS_SERVER| \
     DSOCK_BTLS_CLEAR_KEYS)

struct btls_kp {
	const uint8_t *certmem;
	size_t certlen;
	const uint8_t *keymem;
	size_t keylen;
};

struct btls_ca {
    const char *path, *file;
    const uint8_t *mem;
    size_t len;
};

DSOCK_EXPORT uint8_t *btls_loadfile(
    const char *file,
    size_t *len,
    char *password);
DSOCK_EXPORT int btls_ca(
    struct btls_ca *c,
    const char *file,
    const char *path,
    const uint8_t *mem,
    size_t len);
DSOCK_EXPORT int btls_kp(
    struct btls_kp *kp,
    const uint8_t *cert,
    size_t certlen,
    const uint8_t *key,
    size_t keylen);
DSOCK_EXPORT const char *btls_error(
    int s);
DSOCK_EXPORT int btls_start_server(
    int s,
    uint64_t flags,
    uint64_t ciphers,
    struct btls_kp *kp,
    size_t kplen,
    struct btls_ca *ca,
    const char *alpn);
DSOCK_EXPORT int btls_start_accept(
    int s,
    int l);
DSOCK_EXPORT int btls_start_client(
    int s,
    uint64_t flags,
    uint64_t ciphers,
    struct btls_ca *ca,
    const char *alpn,
    const char *servername);
DSOCK_EXPORT int btls_start_client_kp(
    int s,
    uint64_t flags,
    uint64_t ciphers,
    struct btls_kp *kp,
    size_t kplen,
    struct btls_ca *ca,
    const char *alpn,
    const char *servername);
DSOCK_EXPORT int btls_stop(
    int s,
    int64_t deadline);
DSOCK_EXPORT void btls_reset(
    int s);
DSOCK_EXPORT int btls_handshake(
    int s,
    int64_t deadline);
DSOCK_EXPORT int btls_peercertprovided(
    int s);
DSOCK_EXPORT int btls_peercertcontainsname(
    int s,
    const char *name);
DSOCK_EXPORT const char *btls_peercerthash(
    int s);
DSOCK_EXPORT const char *btls_peercertissuer(
    int s);
DSOCK_EXPORT const char *btls_peercertsubject(
    int s);
DSOCK_EXPORT time_t btls_peercertnotbefore(
    int s);
DSOCK_EXPORT time_t btls_peercertnotafter(
    int s);
DSOCK_EXPORT const char *btls_connalpnselected(
    int s);
DSOCK_EXPORT const char *btls_conncipher(
    int s);
DSOCK_EXPORT const char *btls_connservername(
    int s);
DSOCK_EXPORT const char *btls_connversion(
    int s);

#endif

