#
# Copyright (c) 2015 Martin Sustrik  All rights reserved.
# Copyright (c) 2013 Luca Barbato
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

ACLOCAL_AMFLAGS = -I m4

################################################################################
#  dsock library                                                               #
################################################################################

dsockincludedir = $(includedir)
dsockinclude_HEADERS = dsock.h

lib_LTLIBRARIES = libdsock.la

libdsock_la_SOURCES = \
    bsock.c \
    bthrottler.c \
    btrace.c \
    crlf.c \
    fd.h \
    fd.c \
    http.c \
    iov.h \
    iov.c \
    ipaddr.c \
    keepalive.c \
    lz4.c \
    msock.c \
    mthrottler.c \
    mtrace.c \
    nacl.c \
    nagle.c \
    pfx.c \
    tcp.h \
    tcp.c \
    udp.c \
    unix.c \
    utils.h \
    utils.c \
    websock.c \
    dns/dns.h \
    dns/dns.c \
    lz4/lz4.h \
    lz4/lz4.c \
    lz4/lz4frame.h \
    lz4/lz4frame.c \
    lz4/lz4frame_static.h \
    lz4/lz4hc.h \
    lz4/lz4hc.c \
    lz4/xxhash.h \
    lz4/xxhash.c \
    tweetnacl/tweetnacl.h \
    tweetnacl/tweetnacl.c

if HAVE_TLS
libdsock_la_SOURCES += \
    btls.c \
    tls/tls.h \
    tls/tls.c \
    tls/tls_client.c \
    tls/tls_config.c \
    tls/tls_conninfo.c \
    tls/tls_internal.h \
    tls/tls_peer.c \
    tls/tls_server.c \
    tls/tls_util.c \
    tls/tls_verify.c
endif

libdsock_la_LDFLAGS = -no-undefined -version-info \
    @DSOCK_LIBTOOL_VERSION@

libdsock_la_CFLAGS = \
    -fvisibility=hidden\
    -DDSOCK_EXPORTS \
    -I$(top_srcdir)/tls

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dsock.pc

################################################################################
#  tutorial                                                                    #
################################################################################

noinst_PROGRAMS = \
    tutorial/step1 \
    tutorial/step2 \
    tutorial/step3 \
    tutorial/step4 \
    tutorial/step5

################################################################################
#  compatibility functions for internal libtls                                 #
################################################################################

if HAVE_TLS

libdsock_la_SOURCES += tls/compat/openssl.c

if !HAVE_EXPLICIT_BZERO
libdsock_la_SOURCES += tls/compat/explicit_bzero.c
endif

if !HAVE_STRLCPY
libdsock_la_SOURCES += tls/compat/strlcpy.c
endif

if !HAVE_REALLOCARRAY
libdsock_la_SOURCES += tls/compat/reallocarray.c
endif

if !HAVE_ARC4RANDOM_BUF
libdsock_la_SOURCES += tls/compat/arc4random.c
libdsock_la_SOURCES += tls/compat/getentropy.c
libdsock_la_SOURCES += tls/compat/arc4random_uniform.c
endif

noinst_HEADERS =
	tls/tls.h \
	tls/tls_internal.h \
	tls/compat/arc4random.h \
	tls/compat/arc4random_freebsd.h \
	tls/compat/arc4random_linux.h \
	tls/compat/arc4random_osx.h \
	tls/compat/chacha_private.h

endif

################################################################################
#  automated tests                                                             #
################################################################################

check_PROGRAMS = \
    tests/tcp \
    tests/udp \
    tests/unix \
    tests/pfx \
    tests/crlf \
    tests/bthrottler \
    tests/mthrottler \
    tests/nagle \
    tests/lz4 \
    tests/btrace \
    tests/mtrace \
    tests/nacl \
    tests/keepalive \
    tests/fullstack \
    tests/iovec \
    tests/http \
    tests/websock

if HAVE_TLS

check_PROGRAMS += \
    tests/btls

endif

LDADD = libdsock.la

TESTS = $(check_PROGRAMS)

################################################################################
#  additional packaging-related stuff                                          #
################################################################################

#  Generate ChangeLog file from git.
#  Also, there's no git availabe when building from the source package and
#  thus no way to obtain package version. Therefore, package version is
#  saved into a file when building a source package.
dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog; \
		cd "$(top_srcdir)"; \
		(echo '# Generated by Makefile. Do not edit.'; echo; \
		  ./missing --run git log --decorate ) > ChangeLog.tmp; \
		mv -f ChangeLog.tmp $(top_distdir)/ChangeLog; \
		rm -f ChangeLog.tmp; \
    else \
        cp -f ChangeLog $(top_distdir)/ChangeLog || \
        echo Failed to generate ChangeLog >&2; \
	fi; \
	$(srcdir)/package_version.sh > $(distdir)/.version

EXTRA_DIST = \
    ./abi_version.sh \
    ./package_version.sh

distclean-local:
	-rm -f config.h

