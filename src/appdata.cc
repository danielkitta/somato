/*
 * Copyright (c) 2008-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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
 * along with Somato.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "appdata.h"

#include <glib.h>
#include <glibmm.h>

std::string Util::locate_data_file(const std::string& basename)
{
  const std::string fullpath = Glib::build_filename(SOMATO_PKGDATADIR, basename);

  if (Glib::file_test(fullpath, Glib::FILE_TEST_IS_REGULAR))
    return fullpath;
  else // for debugging
    return Glib::build_filename("ui", basename);
}

std::string Util::locate_shader_file(const std::string& basename)
{
  return Util::locate_data_file(Glib::build_filename("shaders", basename));
}
