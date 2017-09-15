## Copyright (c) 2004-2007  Daniel Elstner  <daniel.kitta@gmail.com>
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
## along with Somato; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

## SOMATO_ARG_ENABLE_VECTOR_SIMD()
##
## Provide the --enable-vector-simd configure argument, set to 'auto'
## by default.
##
AC_DEFUN([SOMATO_ARG_ENABLE_VECTOR_SIMD],
[dnl
AC_CACHE_CHECK([for SSE support], [somato_cv_simd_sse_support],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM(
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
AC_CACHE_CHECK([for SSE2 support], [somato_cv_simd_sse2_support],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[
#include <emmintrin.h>
]], [[
__m128 a;
__m128i b;
a = _mm_setr_ps(1.0f, 2.0f, 3.0f, 4.0f);
b = _mm_cvtps_epi32(a);
(void) _mm_cvtepi32_ps(b);
]])],
  [somato_cv_simd_sse2_support=yes],
  [somato_cv_simd_sse2_support=no])
])
DK_CHECK_FEATURE([_mm_cvtss_f32], [AC_LANG_PROGRAM(
[[
#include <xmmintrin.h>
]], [[
__m128 a;
float b;
a = _mm_set_ss(1.0f);
b = _mm_cvtss_f32(a);
(void) b;
]])
])
AC_CHECK_ALIGNOF([max_align_t])
AS_IF([test "$ac_cv_alignof_max_align_t" -ge 16],
      [somato_align_sufficient=yes], [somato_align_sufficient=no])

DK_CHECK_FEATURE([_mm_malloc], [AC_LANG_PROGRAM(
[[
#include <xmmintrin.h>
]], [[
void* p;
p = _mm_malloc(128, sizeof(__m128));
_mm_free(p);
]])
])
DK_CHECK_FEATURE([posix_memalign], [AC_LANG_PROGRAM(
[[
#ifdef __cplusplus
# include <cstdlib>
using namespace std;
#else
# include <stdlib.h>
#endif
]], [[
void* p = 0;
if (posix_memalign(&p, 16, 128) == 0)
  free(p);
]])
])
AC_ARG_ENABLE([vector-simd], [AS_HELP_STRING(
  [--enable-vector-simd=@<:@auto|sse|sse2|no@:>@],
  [use SIMD instructions for vector arithmetic @<:@auto@:>@])],
  [somato_enable_vector_simd=$enableval],
  [somato_enable_vector_simd=auto])[]dnl

AC_MSG_CHECKING([which SIMD vector implementation to use])
somato_result=none

case $somato_cv_simd_sse_support.$somato_align_sufficient.$SOMATO_FEATURE__MM_MALLOC.$SOMATO_FEATURE_POSIX_MEMALIGN in
  yes.*yes*)
    {
      case $somato_enable_vector_simd.$somato_cv_simd_sse2_support in
        sse2.yes|auto.yes|yes.yes) somato_result=sse2 ;;
        sse2.*|sse.*|auto.*|yes.*) somato_result=sse ;;
      esac
    }
    ;;
esac

case $somato_result in
  sse|sse2)
    {
      AC_DEFINE([SOMATO_VECTOR_USE_SSE], [1],
                [Define to 1 to enable the SSE vector code.])
    }
    ;;
esac

AS_IF([test "x$somato_result" = xsse2],
      [AC_DEFINE([SOMATO_VECTOR_USE_SSE2], [1],
                 [Define to 1 to enable the SSE2 vector code.])
])
AC_MSG_RESULT([$somato_result])

case $somato_enable_vector_simd in
  auto|yes|no)
    ;;
  *)
    {
      AS_IF([test "x$somato_result" != "x$somato_enable_vector_simd"], [AC_MSG_FAILURE([[
The requested SIMD target "$somato_enable_vector_simd" is not available.  It might be
necessary to append the appropriate architecture selection options to
your CFLAGS or CXXFLAGS, respectively.  E.g. if you use GCC, try with
"-march=pentium3" to generate code for Pentium III processors.
]])])
    }
    ;;
esac
])
