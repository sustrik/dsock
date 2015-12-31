/*

  Copyright (c) 2015 Martin Sustrik

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

#ifndef MILLSOCKS_H_INCLUDED
#define MILLSOCKS_H_INCLUDED

#include <libmill.h>
#include <stddef.h>
#include <stdint.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define MILLSOCKS_VERSION_CURRENT 0

/*  The latest revision of the current interface. */
#define MILLSOCKS_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define MILLSOCKS_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if defined MILLSOCKS_NO_EXPORTS
#   define MILLSOCKS_EXPORT
#else
#   if defined _WIN32
#      if defined MILLSOCKS_EXPORTS
#          define MILLSOCKS_EXPORT __declspec(dllexport)
#      else
#          define MILLSOCKS_EXPORT __declspec(dllimport)
#      endif
#   else
#      if defined __SUNPRO_C
#          define MILLSOCKS_EXPORT __global
#      elif (defined __GNUC__ && __GNUC__ >= 4) || \
             defined __INTEL_COMPILER || defined __clang__
#          define MILLSOCKS_EXPORT __attribute__ ((visibility("default")))
#      else
#          define MILLSOCKS_EXPORT
#      endif
#   endif
#endif

/******************************************************************************/
/*  Bytestream.                                                               */
/******************************************************************************/

struct mill_bstream {
    void (*send)(struct mill_bstream **self, const void *buf, size_t len,
        int64_t deadline);
    void (*flush)(struct mill_bstream **self, int64_t deadline);
    void (*recv)(struct mill_bstream **self, void *buf, size_t len,
        int64_t deadline);
};

typedef struct mill_bstream **bstream;

MILLSOCKS_EXPORT void bstream_send(bstream s, const void *buf, size_t len,
    int64_t deadline);
MILLSOCKS_EXPORT void bstream_flush(bstream s, int64_t deadline);
MILLSOCKS_EXPORT void bstream_recv(bstream s, void *buf, size_t len,
    int64_t deadline);

/******************************************************************************/
/*  TCP                                                                       */
/******************************************************************************/

typedef struct mill_tcp_sock *tcp_sock;

MILLSOCKS_EXPORT tcp_sock tcp_listen(ipaddr addr, int backlog);
MILLSOCKS_EXPORT int tcp_port(tcp_sock s);
MILLSOCKS_EXPORT tcp_sock tcp_accept(tcp_sock s, int64_t deadline);
MILLSOCKS_EXPORT ipaddr tcp_addr(tcp_sock s);
MILLSOCKS_EXPORT tcp_sock tcp_connect(ipaddr addr, int64_t deadline);
MILLSOCKS_EXPORT void tcp_send(tcp_sock s, const void *buf, size_t len,
    int64_t deadline);
MILLSOCKS_EXPORT void tcp_flush(tcp_sock s, int64_t deadline);
MILLSOCKS_EXPORT void tcp_recv(tcp_sock s, void *buf, size_t len,
    int64_t deadline);
MILLSOCKS_EXPORT void tcp_close(tcp_sock s);
MILLSOCKS_EXPORT bstream tcp_bstream(tcp_sock s);

#endif

