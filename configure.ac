#
# Copyright (c) 2013 Luca Barbato
# Copyright (c) 2015 Martin Sustrik  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom
# the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

################################################################################
#  Start the configuration phase.                                              #
################################################################################

AC_PREREQ([2.53])

AC_INIT([dsock], [m4_esyscmd([./package_version.sh])],
   [libmill@freelists.org], [dsock], [http://libdill.org/])
AC_CONFIG_SRCDIR([dsock.pc.in])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST

################################################################################
#  Retrieve the versions.                                                      #
################################################################################

AC_PROG_SED
AC_PROG_AWK

DSOCK_ABI_VERSION=m4_esyscmd([./abi_version.sh])
DSOCK_PACKAGE_VERSION=m4_esyscmd([./package_version.sh])
DSOCK_LIBTOOL_VERSION=m4_esyscmd([./abi_version.sh -libtool])

AC_SUBST(DSOCK_ABI_VERSION)
AC_SUBST(DSOCK_PACKAGE_VERSION)
AC_SUBST(DSOCK_LIBTOOL_VERSION)

AC_MSG_NOTICE([dsock package version: $DSOCK_PACKAGE_VERSION])
AC_MSG_NOTICE([dsock ABI version: $DSOCK_ABI_VERSION])

################################################################################
#  Check the compilers.                                                        #
################################################################################

AC_PROG_CC_C99
AM_PROG_CC_C_O

################################################################################
#  --enable-tls                                                                #
################################################################################

# Compatibility checks for libtls integration
CHECK_LIBC_COMPAT
CHECK_CRYPTO_COMPAT

AC_ARG_ENABLE([tls], [AS_HELP_STRING([--enable-tls],
    [Enable TLS sockets [default=no]])])

# TODO: check version
AM_CONDITIONAL([HAVE_TLS], [test "x$enable_tls" = xyes])
if test "x$enable_tls" = "xyes"; then
    AC_DEFINE(HAVE_TLS)
    AC_CHECK_LIB([ssl], [SSL_ctrl ], [],
        AC_MSG_ERROR([libssl not found; install OpenSSL]))
    AC_CHECK_LIB([crypto], [BIO_read], [],
        AC_MSG_ERROR([libcrypto not found; install OpenSSL]))

    # Compatibility checks with OpenSSL implementation
    AC_CHECK_FUNCS([SSL_CTX_set_alpn_protos SSL_get0_alpn_selected], [],
        AC_MSG_ERROR([Incompatible OpenSSL; install OpenSSL>=1.0.2 or LibreSSL]))
fi

################################################################################
#  --enable-debug                                                              #
################################################################################

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    # Override original optimisation level - last option specified wins.
    CFLAGS="$CFLAGS -g -O0"
fi

################################################################################
#  Feature checks.                                                             #
################################################################################

AC_CHECK_LIB([dill], [dill_prologue])
AC_CHECK_FUNCS([dill_prologue])

# SunOS has sockets in a separate library.
AC_CHECK_LIB([socket], [socket])

AC_CHECK_FUNCS([mkstemp])

################################################################################
#  Libtool                                                                     #
################################################################################

LT_INIT

################################################################################
#  Finish the configuration phase.                                             #
################################################################################

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([Makefile dsock.pc])
cp confdefs.h config.h

