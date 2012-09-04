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

#include <config.h>

std::string Util::locate_data_file(const std::string& basename)
{
#ifdef G_OS_WIN32
  if (char *const exedir = g_win32_get_package_installation_directory_of_module(0))
  {
    const std::string fullpath = Glib::build_filename(Glib::ScopedPtr<char>(exedir).get(), basename);

    if (Glib::file_test(fullpath, Glib::FILE_TEST_IS_REGULAR))
      return fullpath;
  }
  return basename;
#else
  const std::string fullpath = Glib::build_filename(SOMATO_PKGDATADIR, basename);

  if (Glib::file_test(fullpath, Glib::FILE_TEST_IS_REGULAR))
    return fullpath;
  else // for debugging
    return Glib::build_filename("ui", basename);
#endif
}

std::string Util::locate_shader_file(const std::string& basename)
{
  return Util::locate_data_file(Glib::build_filename("shaders", basename));
}
