## Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#serial 20170916

## _DK_PROG_CXX_STD(std-version, compiler-flags, test-code)
##
m4_define([_DK_PROG_CXX_STD],
[dnl
AC_CACHE_CHECK([for C++$1 support], [dk_cv_cxx_std$1],
[dnl
for dk_opt in '' $2
do
{
  dk_save_CXX=$CXX
  CXX=$CXX${dk_opt:+' '}$dk_opt
dnl
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <vector>
]], [[
  const std::vector<int> v {1, 2, 3};
  auto a = v[0];
  decltype(v[0]) b = 1;
  $3
  return a + b;
]])],
[dk_cv_cxx_std$1=yes], [dk_cv_cxx_std$1=no])
dnl
  test "x$dk_cv_cxx_std$1" != xyes || break
  CXX=$dk_save_CXX
}
done])[]dnl
])

## DK_PROG_CXX_STD(std-version)
##
## Check if the C++ compiler supports the desired language standard version.
## Set the appropriate compiler flags as needed. Currently only implemented
## for GCC.
## The current language must be set to C++ before calling this macro.
##
AC_DEFUN([DK_PROG_CXX_STD],
[dnl
AC_LANG_ASSERT([C++])[]dnl
_DK_PROG_CXX_STD([$1],
  m4_case([$1], [11], [-std=gnu++11 -std=gnu++0x],
                [14], [-std=gnu++14 -std=gnu++1y],
                [17], [-std=gnu++17 -std=gnu++1z],
                      [m4_fatal([unknown standard version])]),
  m4_case([$1], [14], [a=0b0101'1010'0101;],
                [17], [if (int c=a; c) b=c;]))[]dnl
])
