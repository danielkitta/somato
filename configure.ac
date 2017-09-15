## Copyright (c) 2004-2010  Daniel Elstner  <daniel.kitta@gmail.com>
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

AC_INIT([Somato], [0.7], [daniel.kitta@gmail.com], [somato],
        [http://danielkitta.org/projects/somato])
AC_PREREQ([2.58])

AC_CONFIG_SRCDIR([src/cubescene.cc])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.8 -Wall gnu no-define nostdinc check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_ARG_VAR([ACLOCAL_FLAGS], [aclocal flags, e.g. -I <macro dir>])

AC_PROG_CXX

PKG_CHECK_MODULES([SOMATO_MODULES],
                  [gthread-2.0 >= 2.8.0 pangocairo >= 1.10.0 gtk+-2.0 >= 2.8.0
                   gtkglext-1.0 >= 1.0.0 assimp >= 3.0
                   gtkmm-2.4 >= 2.6.0 libglademm-2.4 >= 2.6.0])

# Somato does not use the custom widget feature of libglade, thus there
# is no need for --export-dynamic.  Remove it to avoid pointless bloat.
# These days libglade no longer forces --export-dynamic by default, but
# gdkglext-1.0 depends on gmodule-2.0 which does add it.  Darn.
SOMATO_MODULES_LIBS=`expr " $SOMATO_MODULES_LIBS " |
                       sed 's/ -Wl,--export-dynamic / /;s/^  *//;s/  *$//'`

DK_PKG_PATH_PROG([GTK_UPDATE_ICON_CACHE], [gtk+-2.0], [gtk-update-icon-cache])

AC_LANG([C++])
DK_PROG_CXX_CXX11

DK_SH_VAR_PUSH([CPPFLAGS], ["$SOMATO_MODULES_CFLAGS $CPPFLAGS"])

# Note that this is only a compile-time requirement.
DK_REQUIRE_GL_VERSION([3.2])

# Before running any of the other tests, check whether it is necessary
# to explicitely link -lm on this platform.
AC_SEARCH_LIBS([atan2], [m])

DK_CHECK_FEATURE([lrintf],
[
  AC_LANG_PROGRAM([[#include <cmath>]],
                  [[(void) lrintf(1.5f);]])
])
DK_CHECK_FEATURE([sincosf],
[
  AC_LANG_PROGRAM([[#include <cmath>]],
                  [[float s, c; sincosf(0.0f, &s, &c);]])
])

SOMATO_ARG_ENABLE_VECTOR_SIMD

DK_ARG_ENABLE_WARNINGS([SOMATO_WARNING_FLAGS],
                       [-Wall -w1],
                       [-Wall -Wextra -w1])

DK_SH_VAR_POP([CPPFLAGS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
