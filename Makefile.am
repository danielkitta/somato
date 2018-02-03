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

SUBDIRS = src/tool

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal

bin_PROGRAMS = src/somato

if SIMD_SSE
simd_sources = src/simd_sse.cc src/simd_sse.h
else
simd_sources = src/simd_fallback.cc src/simd_fallback.h
endif

src_somato_SOURCES =		\
	src/application.cc	\
	src/application.h	\
	src/asynctask.cc	\
	src/asynctask.h		\
	src/bitcube.cc		\
	src/bitcube.h		\
	src/cubescene.cc	\
	src/cubescene.h		\
	src/glscene.cc		\
	src/glscene.h		\
	src/glshader.cc		\
	src/glshader.h		\
	src/gltextlayout.cc	\
	src/gltextlayout.h	\
	src/gltypes.h		\
	src/glutils.cc		\
	src/glutils.h		\
	src/somato.cc		\
	src/mainwindow.cc	\
	src/mainwindow.h	\
	src/mathutils.cc	\
	src/mathutils.h		\
	src/meshtypes.h		\
	src/puzzle.cc		\
	src/puzzle.h		\
	src/puzzlecube.h	\
	src/vectormath.cc	\
	src/vectormath.h	\
	$(simd_sources)

nodist_src_somato_SOURCES =	\
	src/resources.cc

resource_desc = ui/somato.gresource.xml

resource_files =			\
	ui/gtk/help-overlay.ui		\
	ui/gtk/menus.ui			\
	ui/shaders/cellgrid.frag	\
	ui/shaders/cellgrid.geom	\
	ui/shaders/cellgrid.vert	\
	ui/shaders/pieceoutline.frag	\
	ui/shaders/pieceoutline.geom	\
	ui/shaders/pieceoutline.vert	\
	ui/shaders/puzzlepieces.frag	\
	ui/shaders/puzzlepieces.vert	\
	ui/shaders/textlabel.frag	\
	ui/shaders/textlabel.vert	\
	ui/mainwindow.glade

resource_deps =			\
	$(resource_desc)	\
	$(resource_files)	\
	ui/mesh-desc.bin	\
	ui/woodtexture-$(SOMATO_TEXTURE_COMPRESSION).ktx

dist_noinst_SCRIPTS =		\
	debian/rules		\
	autogen.sh

dist_noinst_DATA =		\
	$(resource_files)	\
	ui/puzzlepieces.dae	\
	ui/woodtexture.png	\
	ui/woodtexture-dxt.ktx	\
	ui/woodtexture-etc.ktx	\
	debian/changelog	\
	debian/control		\
	debian/copyright	\
	debian/menu		\
	README.md		\
	screenshot.png

DISTCLEANFILES	  = src/resources.cc ui/*.bin

iconthemedir	  = $(datadir)/icons/hicolor
appicondir	  = $(iconthemedir)/48x48/apps
dist_appicon_DATA = ui/somato.png

desktopdir	  = $(datadir)/applications
dist_desktop_DATA = ui/somato.desktop

AM_CPPFLAGS	  = -I$(top_builddir) $(SOMATO_MODULES_CFLAGS)
AM_CXXFLAGS	  = $(SOMATO_EXTRA_CXXFLAGS) $(SOMATO_WARNING_FLAGS)
src_somato_LDADD  = $(SOMATO_MODULES_LIBS)

bake_meshdata     = src/tool/bake-meshdata$(BUILD_EXEEXT)
update_icon_cache = $(GTK_UPDATE_ICON_CACHE) --ignore-theme-index --force

ui/mesh-desc.bin: $(bake_meshdata) ui/puzzlepieces.dae
	$(AM_V_GEN)$(bake_meshdata) $(SOMATO_BYTE_ORDER) \
	 --mesh-file "$(srcdir)/ui/puzzlepieces.dae" --output-dir ui \
	 PieceOrange PieceGreen PieceRed PieceYellow PieceBlue PieceLavender PieceCyan

src/resources.cc: $(resource_deps)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --sourcedir=ui --sourcedir="$(srcdir)/ui" \
	 --generate-source --internal --target="$@" "$(resource_desc)"

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
	cp -fp "$(srcdir)/README.md" $@

.PHONY: dist-deb install-update-icon-cache uninstall-update-icon-cache
