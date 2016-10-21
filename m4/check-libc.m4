# Copyright (c) 2014-2015 Brent Cook
# Copyright (c) 2016 Tai Chi Minh Ralph Eastwood
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


AC_DEFUN([CHECK_LIBC_COMPAT], [
# Check for libc headers
AC_CHECK_HEADERS([err.h])
# Check for general libc functions
AC_CHECK_FUNCS([asprintf reallocarray])
AC_CHECK_FUNCS([strlcpy strsep])
AC_CHECK_FUNCS([timegm])
AM_CONDITIONAL([HAVE_REALLOCARRAY], [test "x$ac_cv_func_reallocarray" = xyes])
AM_CONDITIONAL([HAVE_STRLCPY], [test "x$ac_cv_func_strlcpy" = xyes])
])

AC_DEFUN([CHECK_CRYPTO_COMPAT], [
# Check crypto-related libc functions and syscalls
AC_CHECK_FUNCS([arc4random arc4random_buf arc4random_uniform])
AC_CHECK_FUNCS([explicit_bzero getauxval getentropy])
AM_CONDITIONAL([HAVE_ARC4RANDOM], [test "x$ac_cv_func_arc4random" = xyes])
AM_CONDITIONAL([HAVE_ARC4RANDOM_BUF], [test "x$ac_cv_func_arc4random_buf" = xyes])
AM_CONDITIONAL([HAVE_ARC4RANDOM_UNIFORM], [test "x$ac_cv_func_arc4random_uniform" = xyes])
AM_CONDITIONAL([HAVE_EXPLICIT_BZERO], [test "x$ac_cv_func_explicit_bzero" = xyes])
AM_CONDITIONAL([HAVE_GETENTROPY], [test "x$ac_cv_func_getentropy" = xyes])

# Override arc4random_buf implementations with known issues
AM_CONDITIONAL([HAVE_ARC4RANDOM_BUF],
	[test "x$USE_BUILTIN_ARC4RANDOM" != xyes \
	   -a "x$ac_cv_func_arc4random_buf" = xyes])

# Check for getentropy fallback dependencies
AC_CHECK_FUNC([getauxval])
AC_SEARCH_LIBS([clock_gettime],[rt posix4])
AC_CHECK_FUNC([clock_gettime])
AC_SEARCH_LIBS([dl_iterate_phdr],[dl])
AC_CHECK_FUNC([dl_iterate_phdr])
])
