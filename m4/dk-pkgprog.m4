## Copyright (c) 2004-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#serial 20171209

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

## DK_PKG_CONFIG_SUBST(variable, arguments, [action-if-found], [action-if-not-found])
##
## Run the pkg-config utility with the specified command-line <arguments>
## and capture its standard output in the named shell <variable>.  If the
## command exited successfully, execute <action-if-found> in the shell if
## specified.  If the command failed, run <action-if-not-found> if given,
## otherwise ignore the error.
##
AC_DEFUN([DK_PKG_CONFIG_SUBST],
[dnl
m4_assert([$# >= 2])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
AC_MSG_CHECKING([for $1])
dnl
AS_IF([test -z "[$]{$1+set}"],
      [$1=`$PKG_CONFIG $2 2>&AS_MESSAGE_LOG_FD`
       AS_IF([test "[$]?" -eq 0], [$3], [$4])])
dnl
AC_MSG_RESULT([[$]$1])
AC_SUBST([$1])[]dnl
])

## DK_PKG_PROG_BUILD_PKG_CONFIG
##
## Like PKG_PROG_PKG_CONFIG, but for tools to run on the build machine.
##
AC_DEFUN([DK_PKG_PROG_BUILD_PKG_CONFIG],
[dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
AC_ARG_VAR([BUILD_PKG_CONFIG], [path to pkg-config utility for build machine packages])
AC_ARG_VAR([BUILD_PKG_CONFIG_PATH], [directories to add to build machine pkg-config's search path])
AC_ARG_VAR([BUILD_PKG_CONFIG_LIBDIR], [path overriding build machine pkg-config's built-in search path])
dnl
AC_CHECK_PROGS([BUILD_PKG_CONFIG], [[$]{build_alias:+$build_alias-pkg-config} pkg-config])
AS_IF([$BUILD_PKG_CONFIG --atleast-pkgconfig-version=0.28 2>&AS_MESSAGE_LOG_FD],,
      [AC_MSG_ERROR([[build machine pkg-config not found]])])
])

## DK_PKG_CHECK_BUILD_MODULES(variable, arguments, [action-if-found], [action-if-not-found])
##
## Like PKG_CHECK_MODULES, but for tools to run on the build machine.
##
AC_DEFUN([DK_PKG_CHECK_BUILD_MODULES],
[dnl
m4_assert([$# >= 2])[]dnl
AC_REQUIRE([DK_PKG_PROG_BUILD_PKG_CONFIG])[]dnl
dnl
dk_save_pkg_config=$PKG_CONFIG
dk_save_pkg_config_path=$PKG_CONFIG_PATH
dk_save_pkg_config_libdir=$PKG_CONFIG_LIBDIR
dk_save_pkg_config_sysroot_dir=$PKG_CONFIG_SYSROOT_DIR
PKG_CONFIG=$BUILD_PKG_CONFIG
PKG_CONFIG_PATH=$BUILD_PKG_CONFIG_PATH
PKG_CONFIG_LIBDIR=$BUILD_PKG_CONFIG_LIBDIR
PKG_CONFIG_SYSROOT_DIR=
m4_apply([PKG_CHECK_MODULES], [$@])
PKG_CONFIG=$dk_save_pkg_config
PKG_CONFIG_PATH=$dk_save_pkg_config_path
PKG_CONFIG_LIBDIR=$dk_save_pkg_config_libdir
PKG_CONFIG_SYSROOT_DIR=$dk_save_pkg_config_sysroot_dir
])
