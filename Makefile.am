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

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal

bin_PROGRAMS = src/somato

src_somato_SOURCES =		\
	src/appdata.cc		\
	src/appdata.h		\
	src/application.cc	\
	src/application.h	\
	src/asynctask.cc	\
	src/asynctask.h		\
	src/cube.cc		\
	src/cube.h		\
	src/cubescene.cc	\
	src/cubescene.h		\
	src/glscene.cc		\
	src/glscene.h		\
	src/glsceneprivate.h	\
	src/glshader.cc		\
	src/glshader.h		\
	src/glutils.cc		\
	src/glutils.h		\
	src/main.cc		\
	src/mainwindow.cc	\
	src/mainwindow.h	\
	src/mathutils.cc	\
	src/mathutils.h		\
	src/meshloader.cc	\
	src/meshloader.h	\
	src/puzzle.cc		\
	src/puzzle.h		\
	src/vectormath.cc	\
	src/vectormath.h

EXTRA_src_somato_SOURCES =	\
	src/vector_classic.cc	\
	src/vector_classic.h	\
	src/vector_sse.cc	\
	src/vector_sse.h

dist_pkgdata_DATA =		\
	ui/app-menu.ui		\
	ui/cubescene.css	\
	ui/mainwindow.glade	\
	ui/puzzlepieces.dae	\
	ui/shortcuts-window.ui	\
	ui/woodtexture.png

shaderdir = $(pkgdatadir)/shaders
dist_shader_DATA =			\
	ui/shaders/cellgrid.frag	\
	ui/shaders/cellgrid.vert	\
	ui/shaders/focusrect.frag	\
	ui/shaders/focusrect.vert	\
	ui/shaders/puzzlepieces.frag	\
	ui/shaders/puzzlepieces.vert	\
	ui/shaders/textlabel.frag	\
	ui/shaders/textlabel.vert

dist_noinst_SCRIPTS =		\
	debian/rules		\
	autogen.sh

dist_noinst_DATA =		\
	debian/changelog	\
	debian/control		\
	debian/copyright	\
	debian/menu		\
	README.md		\
	screenshot.png

iconthemedir	  = $(datadir)/icons/hicolor
appicondir	  = $(iconthemedir)/48x48/apps
dist_appicon_DATA = ui/somato.png

desktopdir	  = $(datadir)/applications
dist_desktop_DATA = ui/somato.desktop

global_defs	  = -DSOMATO_PKGDATADIR=\""$(pkgdatadir)"\" -I$(top_builddir)
AM_CPPFLAGS	  = $(global_defs) $(SOMATO_MODULES_CFLAGS) $(SOMATO_WARNING_FLAGS)
AM_CXXFLAGS	  = $(SOMATO_EXTRA_CXXFLAGS)
src_somato_LDADD  = $(SOMATO_MODULES_LIBS)

update_icon_cache = $(GTK_UPDATE_ICON_CACHE) --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	@$(POST_INSTALL)
	-test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	@$(POST_UNINSTALL)
	-test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

dist-deb: distdir
	cd "$(distdir)" && dpkg-buildpackage -nc -rfakeroot -uc -us
	rm -rf "$(distdir)"

README: README.md
	cp -fp README.md $@

.PHONY: dist-deb install-update-icon-cache uninstall-update-icon-cache
