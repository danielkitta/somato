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

AC_INIT([Somato], [0.8], [daniel.kitta@gmail.com], [somato],
        [https://github.com/danielkitta/somato])
AC_PREREQ([2.63])

AC_CONFIG_SRCDIR([src/cubescene.cc])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11 -Wall no-define nostdinc])
AM_SILENT_RULES([yes])

AC_PROG_CXX

PKG_CHECK_MODULES([MESHDATA_MODULES], [glib-2.0 assimp >= 3.0])
PKG_CHECK_MODULES([SOMATO_MODULES], [gthread-2.0 epoxy >= 1.3 gtkmm-3.0 >= 3.22])

DK_PKG_CONFIG_SUBST([GLIB_COMPILE_RESOURCES],
                    [--variable=glib_compile_resources gio-2.0],,
                    [AC_MSG_FAILURE([[GLib resource compiler not found.]])])

DK_PKG_PATH_PROG([GTK_UPDATE_ICON_CACHE], [gtk+-3.0], [gtk-update-icon-cache])

AC_DEFINE([RESOURCE_PREFIX], ["/org/gtk/somato/"],
          [Base path for loading resources.])

AC_LANG([C++])
DK_PROG_CXX_STD([14])
DK_CHECK_VISIBILITY_SUPPORT([SOMATO_EXTRA_CXXFLAGS], [SOMATO_EXPORT])

DK_CHECK_COMPILE_FLAGS([SOMATO_EXTRA_CXXFLAGS], [math without errno],
                       [-fno-math-errno])
DK_CHECK_COMPILE_FLAGS([SOMATO_EXTRA_CXXFLAGS], [link-time optimization],
                       [-flto])

DK_SH_VAR_PUSH([CPPFLAGS], ["$SOMATO_MODULES_CFLAGS $CPPFLAGS"])

# Before running any of the other tests, check whether it is necessary
# to explicitely link -lm on this platform.
AC_SEARCH_LIBS([atan2], [m])

SOMATO_ARG_ENABLE_VECTOR_SIMD
DK_ARG_ENABLE_WARNINGS([SOMATO_WARNING_FLAGS], [-Wall], [-Wall -Wextra])

DK_SH_VAR_POP([CPPFLAGS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
