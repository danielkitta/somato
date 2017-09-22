/*
 * Copyright (c) 2004-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "application.h"
#include "glutils.h"

#include <glib.h>
#include <glibmm.h>
#include <locale>
#include <stdexcept>

namespace
{

/*
 * Make GL::Error exceptions within signal handlers behave like the "soft"
 * g_return_if_fail() assertions.  That is, issue a useful warning message
 * but do not abort forcefully unless --g-fatal-warnings has been specified.
 * However, from this point on there are no guarantees whatsoever as to how
 * the program might behave.
 */
void trap_gl_error()
{
  try
  {
    throw; // re-throw the exception currently being handled by the caller
  }
  catch (const GL::Error& error)
  {
    enum { MAX_ERROR_COUNT = 10 };
    static int error_count = 0;

    if (error_count < MAX_ERROR_COUNT)
    {
      const Glib::ustring what = error.what();
      g_critical("unhandled exception in signal handler: GL error: %s", what.c_str());

      if (++error_count == MAX_ERROR_COUNT)
        g_critical("suppressing subsequent GL errors...");
    }
  }
}

} // anonymous namespace

int main(int argc, char** argv)
{
  try // do not abort if the user-specified locale does not exist
  {
    std::locale::global(std::locale(""));
  }
  catch (const std::runtime_error& error)
  {
    g_warning("%s", error.what());
  }

  Glib::set_application_name(PACKAGE_NAME);
  Glib::add_exception_handler(&trap_gl_error);

  const auto app = Somato::Application::create();

  return app->run(argc, argv);
}
