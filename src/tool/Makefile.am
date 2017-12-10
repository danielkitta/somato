## Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

AUTOMAKE_OPTIONS = -Wno-gnu

EXEEXT      = $(BUILD_EXEEXT)
CC          = $(CC_FOR_BUILD)
CPP         = $(CPP_FOR_BUILD)
CXX         = $(CXX_FOR_BUILD)
CXXCPP      = $(CXXCPP_FOR_BUILD)
CFLAGS      = $(CFLAGS_FOR_BUILD)
LDFLAGS     = $(LDFLAGS_FOR_BUILD)
CPPFLAGS    = $(CPPFLAGS_FOR_BUILD)
CXXFLAGS    = $(CXXFLAGS_FOR_BUILD)
CXXCPPFLAGS = $(CXXCPPFLAGS_FOR_BUILD)

noinst_PROGRAMS = bake-meshdata

bake_meshdata_SOURCES =		\
	bake-meshdata.cc	\
	meshloader.cc		\
	meshloader.h

bake_meshdata_LDADD = $(MESHDATA_MODULES_LIBS)

AM_CPPFLAGS = -I$(top_srcdir)/src $(MESHDATA_MODULES_CFLAGS)
AM_CXXFLAGS = $(TOOL_EXTRA_CXXFLAGS) $(TOOL_WARNING_FLAGS)

