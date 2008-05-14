/*
 * Copyright (c) 2008  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "appdata.h"

#include <glibmm.h>
#include <glib.h>

#ifdef G_OS_WIN32
# include <windows.h>
#endif
#include <config.h>

std::string Util::locate_data_file(const char* basename)
{
  g_assert(basename != 0);
  const std::string base = basename;

#ifdef G_OS_WIN32
  if (char *const exedir = g_win32_get_package_installation_directory_of_module(0))
  {
    const std::string exepath = Glib::build_filename(Glib::ScopedPtr<char>(exedir).get(), base);

    if (Glib::file_test(exepath, Glib::FILE_TEST_IS_REGULAR))
      return exepath;

    const std::string pkgpath = Glib::build_filename(SOMATO_PKGDATADIR, base);

    if (Glib::file_test(pkgpath, Glib::FILE_TEST_IS_REGULAR))
      return pkgpath;
  }
  return base;
#else
  return Glib::build_filename(SOMATO_PKGDATADIR, base);
#endif
}
