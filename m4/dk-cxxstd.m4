## Copyright (c) 2018  Daniel Elstner  <daniel.kitta@gmail.com>
##
## This file is part of danielk's Autostuff.
##
## danielk's Autostuff is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published
## by the Free Software Foundation; either version 2 of the License, or (at
## your option) any later version.
##
## danielk's Autostuff is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with danielk's Autostuff.  If not, see <http://www.gnu.org/licenses/>.

#serial 20180128

## _DK_PROG_CXX_STD_TEST(version, flags, [version-2], [flags-2], ...)
##
m4_define([_DK_PROG_CXX_STD_TEST],
[dnl
for dk_flag in '' $2
do
CXXFLAGS="$dk_save_cxxflags $dk_flag"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if __cplusplus < 20]$1[00
# error "version test failed"
#endif
]], [])], [dk_cxx_std_flag=$dk_flag; break], [dk_cxx_std_flag=failed])
done
AS_IF([test "x$dk_cxx_std_flag" = xfailed],
      [m4_ifval([$3], [_DK_PROG_CXX_STD_TEST(m4_shift2($@))], [dk_cxx_std_version=no])],
      [dk_cxx_std_version=C++$1])
])

## _DK_PROG_CXX_STD_CLAMP(opt-version, version, flags, [version-2], [flags-2], ...)
##
m4_define([_DK_PROG_CXX_STD_CLAMP],
[dnl
m4_if([$3],, [m4_fatal([unknown C++ standard version])])[]dnl
m4_if(m4_eval([$1 < $2]), [1], [_DK_PROG_CXX_STD_CLAMP([$1], m4_shift3($@))], [m4_shift($@)])[]dnl
])

## DK_PROG_CXX_STD(flags-variable, min-version, [opt-version])
##
## Check if the C++ compiler supports the desired language standard version.
## The current language must be set to C++ before calling this macro.
##
AC_DEFUN([DK_PROG_CXX_STD],
[dnl
m4_assert([$# >= 2])[]dnl
AC_LANG_ASSERT([C++])[]dnl
AC_MSG_CHECKING([for C++ standard version])
dk_save_cxxflags=$CXXFLAGS
_DK_PROG_CXX_STD_TEST(_DK_PROG_CXX_STD_CLAMP(m4_default_quoted([$3], [$2]),
                                             [17], [-std=gnu++17 -std=gnu++1z],
                                             [14], [-std=gnu++14 -std=gnu++1y],
                                             [11], [-std=gnu++11 -std=gnu++0x]))
CXXFLAGS=$dk_save_cxxflags
AC_MSG_RESULT([$dk_cxx_std_version])
AS_IF([test "x$dk_cxx_std_version" = xno],
      [AC_MSG_ERROR([Support for C++$2 is required to compile this program.])])
AS_CASE([$dk_cxx_std_flag], [""|failed],,
        [$1=[$]$1[$]{$1:+' '}$dk_cxx_std_flag])
])
