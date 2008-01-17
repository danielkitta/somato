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

## This project does not use recursive make, but a single toplevel Makefile
## to build the entire tree.  Read Peter Miller's excellent paper to learn
## why recursive make invocations are both slow and error-prone:
## http://members.pcug.org.au/~millerp/rmch/recu-make-cons-harm.html

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I m4 $(ACLOCAL_FLAGS)
DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal

bin_PROGRAMS = src/somato

src_somato_SOURCES =		\
	src/array.h		\
	src/cube.cc		\
	src/cube.h		\
	src/cubescene.cc	\
	src/cubescene.h		\
	src/glscene.cc		\
	src/glscene.h		\
	src/glsceneprivate.h	\
	src/glutils.cc		\
	src/glutils.h		\
	src/main.cc		\
	src/mainwindow.cc	\
	src/mainwindow.h	\
	src/mathutils.cc	\
	src/mathutils.h		\
	src/puzzle.cc		\
	src/puzzle.h		\
	src/tesselate.cc	\
	src/tesselate.h		\
	src/vectormath.cc	\
	src/vectormath.h

EXTRA_src_somato_SOURCES =	\
	src/vector_classic.cc	\
	src/vector_classic.h	\
	src/vector_sse.cc	\
	src/vector_sse.h

global_defs = -DSOMATO_PKGDATADIR=\""$(pkgdatadir)"\"
AM_CPPFLAGS = $(global_defs) -I$(top_builddir) $(SOMATO_MODULES_CFLAGS) $(SOMATO_WARNING_FLAGS)
src_somato_LDADD = $(SOMATO_MODULES_LIBS)

dist_noinst_SCRIPTS = autogen.sh debian/rules
dist_noinst_DATA    = debian/changelog debian/control debian/copyright debian/menu

dist_pkgdata_DATA = ui/cubescene.rc ui/cubetexture.png ui/mainwindow.glade ui/mainwindow-ui.xml

iconthemedir      = $(datadir)/icons/hicolor
appicondir        = $(iconthemedir)/48x48/apps
dist_appicon_DATA = ui/somato.png

desktopdir        = $(datadir)/applications
dist_desktop_DATA = ui/somato.desktop

update_icon_cache = $(GTK_UPDATE_ICON_CACHE) --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	@$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	@$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

dist-deb: distdir
	cd "$(distdir)" && dpkg-buildpackage -nc -rfakeroot -uc -us
	rm -rf "$(distdir)"

.PHONY: dist-deb install-update-icon-cache uninstall-update-icon-cache
