/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
 *
 * This file is part of Somato.
 *
 * Somato is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Somato is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Somato; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define PACKAGE_NAME "Somato"
#define PACKAGE_TARNAME "somato"
#define PACKAGE_VERSION "0.6"
// XXX: temporary
#define SOMATO_PKGDATADIR "C:\\Users\\Daniel\\Documents\\Visual Studio 2008\\Projects\\Somato\\ui"

#if (_M_IX86_FP >= 1) || defined(_M_X64)
# define SOMATO_VECTOR_USE_SSE 1
#endif
#if (_M_IX86_FP >= 2) || defined(_M_X64)
# define SOMATO_VECTOR_USE_SSE2 1
#endif

#undef SOMATO_HAVE_POSIX_MEMALIGN
#undef SOMATO_HAVE_SINCOSF
