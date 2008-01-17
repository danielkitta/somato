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

#serial 20070106

## _DK_REQUIRE_GL_VERSION_VAR(major.minor, cache variable, cpp define)
##
m4_define([_DK_REQUIRE_GL_VERSION_VAR],
[dnl
AC_CACHE_CHECK([for OpenGL $1 headers], [$2],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <GL/gl.h>
]], [[
#ifndef ]$3[
 choke me
#endif
]])], [$2=yes], [$2=no])])

AS_IF([test "x$$2" != xyes], [AC_MSG_FAILURE([[
The OpenGL ]$1[ API is required in order to compile $PACKAGE_NAME.
Note that it is not mandatory for your system to actually implement
all of OpenGL ]$1[.  At runtime, features that are found not to be
available will not be used.  However, in order to build this program
you will have to update your system's GL header files.
]])])
])

## DK_REQUIRE_GL_VERSION(major.minor)
##
## Check whether the GL/gl.h header file support API version <major.minor>.
## Abort with an error message if otherwise.  Note that the version number
## argument must specify an actually existing OpenGL API version; i.e. "1.9"
## would not be valid as there never was an OpenGL 1.9.
##
AC_DEFUN([DK_REQUIRE_GL_VERSION],
[dnl
m4_if([$1],, [AC_FATAL([argument expected])])[]dnl
_DK_REQUIRE_GL_VERSION_VAR([$1],
                           [AS_TR_SH([dk_cv_gl_version_$1])],
                           [AS_TR_CPP([GL_VERSION_$1])])[]dnl
])
