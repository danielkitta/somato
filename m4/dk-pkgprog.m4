## Copyright (c) 2004-2007  Daniel Elstner  <daniel.kitta@gmail.com>
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
## with danielk's Autostuff; if not, write to the Free Software Foundation,
## Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#serial 20070105

## DK_PKG_PATH_PROG(variable, package, executable)
##
## Like AC_PATH_PROG(variable, executable,, <extra_path>), where <extra_path>
## is set to the contents of $PATH prepended by the package's binary executable
## directory.  This should catch even the weirdest setups.  An error message is
## generated if the executable cannot be found anywhere in the resulting path.
##
AC_DEFUN([DK_PKG_PATH_PROG],
[dnl
m4_if([$3],, [AC_FATAL([3 arguments expected])])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
dnl
dk_pkg_prefix=`$PKG_CONFIG --variable=exec_prefix "$2" 2>&AS_MESSAGE_LOG_FD`

AS_IF([test "x$dk_pkg_prefix" = x],
      [dk_pkg_path=$PATH],
      [dk_pkg_path=$dk_pkg_prefix/bin$PATH_SEPARATOR$PATH])

AC_PATH_PROG([$1], [$3],, [$dk_pkg_path])

AS_IF([test "x$$1" = x], [AC_MSG_ERROR([[
Oops, could not find "]$3[".  This program is
normally included with the ]$2[ package.  Please make sure
that your installation of ]$2[ is set up correctly.
]])])
])
