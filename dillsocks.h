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

#if defined DILLSOCKS_NO_EXPORTS
#   define DILLSOCKS_EXPORT
#else
#   if defined _WIN32
#      if defined DILLSOCKS_EXPORTS
#          define DILLSOCKS_EXPORT __declspec(dllexport)
#      else
#          define DILLSOCKS_EXPORT __declspec(dllimport)
#      endif
#   else
#      if defined __SUNPRO_C
#          define DILLSOCKS_EXPORT __global
#      elif (defined __GNUC__ && __GNUC__ >= 4) || \
             defined __INTEL_COMPILER || defined __clang__
#          define DILLSOCKS_EXPORT __attribute__ ((visibility("default")))
#      else
#          define DILLSOCKS_EXPORT
#      endif
#   endif
#endif

/******************************************************************************/
/*  Generic socket.                                                           */
/******************************************************************************/

typedef struct dill_sock *sock;

struct dill_sock_vfptr {
    void (*brecv)(sock s, void *buf, size_t len, int64_t deadline);
    void (*bsend)(sock s, const void *buf, size_t len, int64_t deadline);
    void (*bflush)(sock s, int64_t deadline);
};

struct dill_sock {
    struct dill_sock_vfptr *vfptr;
};

DILLSOCKS_EXPORT void brecv(sock s, void *buf, size_t len,
    int64_t deadline);
DILLSOCKS_EXPORT void bsend(sock s, const void *buf, size_t len,
    int64_t deadline);
DILLSOCKS_EXPORT void bflush(sock s, int64_t deadline);

/* TODO: msend, mrecv, mflush */

/******************************************************************************/
/*  TCP                                                                       */
/******************************************************************************/

DILLSOCKS_EXPORT sock tcp_listen(ipaddr addr, int backlog);
DILLSOCKS_EXPORT int tcp_port(sock s);
DILLSOCKS_EXPORT sock tcp_accept(sock s, int64_t deadline);
DILLSOCKS_EXPORT ipaddr tcp_addr(sock s);
DILLSOCKS_EXPORT sock tcp_connect(ipaddr addr, int64_t deadline);
DILLSOCKS_EXPORT void tcp_close(sock s);

#endif

