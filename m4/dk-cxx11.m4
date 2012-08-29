## Copyright (c) 2012  Daniel Elstner  <daniel.kitta@gmail.com>
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

#serial 20120829

## DK_PROG_CXX_CXX11()
##
## Check for and enable C++11 support.  Currently only implemented for GCC.
## The current language must be set to C++ before calling this macro.
##
AC_DEFUN([DK_PROG_CXX_CXX11],
[dnl
AC_LANG_ASSERT([C++])[]dnl
AC_CACHE_CHECK([for C++11 support], [dk_cv_cxx_cxx11],
[dnl
for dk_opt in '' ' -std=gnu++11' ' -std=gnu++0x'
do
{
  dk_save_CXX=$CXX
  CXX=$CXX$dk_opt
dnl
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <vector>
]], [[
  const std::vector<int> v{1};
  auto a = v[0];
  decltype(v[0]) b = 1;
  return a + b;
]])],
[dk_cv_cxx_cxx11=yes], [dk_cv_cxx_cxx11=no])
dnl
  test "x$dk_cv_cxx_cxx11" != xyes || break
  CXX=$dk_save_CXX
}
done])[]dnl
])
