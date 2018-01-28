## Copyright (c) 2004-2017  Daniel Elstner  <daniel.kitta@gmail.com>
##
## This file is part of Somato.
##
## Somato is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## Somato is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Somato.  If not, see <http://www.gnu.org/licenses/>.

#serial 20180128

## SOMATO_ARG_ENABLE_VECTOR_SIMD()
##
## Provide the --enable-vector-simd configure argument, set to 'auto'
## by default.
##
AC_DEFUN([SOMATO_ARG_ENABLE_VECTOR_SIMD],
[dnl
AC_CACHE_CHECK([for SSE support], [somato_cv_simd_sse_support],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <xmmintrin.h>
]], [[
__m128 a;
__m128 b;
a = _mm_setr_ps(1.0f, 2.0f, 3.0f, 4.0f);
b = _mm_mul_ps(a, a);
(void) _mm_shuffle_ps(a, b, _MM_SHUFFLE(0,1,2,3));
]])],
  [somato_cv_simd_sse_support=yes],
  [somato_cv_simd_sse_support=no])
])
AC_ARG_ENABLE([vector-simd], [AS_HELP_STRING(
  [--enable-vector-simd=@<:@auto|sse|no@:>@],
  [use SIMD instructions for vector arithmetic @<:@auto@:>@])],
  [somato_enable_vector_simd=$enableval],
  [somato_enable_vector_simd=auto])[]dnl

AC_MSG_CHECKING([which SIMD vector implementation to use])
somato_result=none

AS_IF([test "x$somato_cv_simd_sse_support" = xyes],
      [AS_CASE([$somato_enable_vector_simd],
               [sse|auto|yes], [somato_result=sse])])
AM_CONDITIONAL([SIMD_SSE], [test "x$somato_result" = xsse])
AM_COND_IF([SIMD_SSE], [AC_DEFINE([SOMATO_VECTOR_USE_SSE], [1],
                                  [Define to 1 to enable the SSE vector code.])])
AC_MSG_RESULT([$somato_result])

AS_CASE([$somato_enable_vector_simd], [auto|yes|no],,
        [AS_IF([test "x$somato_result" != "x$somato_enable_vector_simd"], [AC_MSG_FAILURE([[
The requested SIMD target "$somato_enable_vector_simd" is not supported. It might be
necessary to append the appropriate architecture selection options to
your CFLAGS or CXXFLAGS, respectively.
]])])])
])
