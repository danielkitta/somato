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

#include "mainwindow.h"
#include "glutils.h"

#include <glib.h>
#include <gtk/gtkgl.h>
#include <gtk/gtkwindow.h> /* for gtk_window_set_default_icon_name() */
#include <glibmm.h>
#include <gtkmm/main.h>
#include <gtkmm/rc.h>

#include <locale>
#include <stdexcept>

#include <config.h>

#ifdef G_OS_WIN32
# include <windows.h>
# include <crtdbg.h>
#endif

namespace
{

static
const char *const cubescene_rc_filename = SOMATO_PKGDATADIR G_DIR_SEPARATOR_S "cubescene.gtkrc";

static
void init_locale()
{
  try // do not abort if the user-specified locale does not exist
  {
    std::locale::global(std::locale(""));
  }
  catch (const std::runtime_error& error)
  {
    g_warning("%s", error.what());
  }
}

/*
 * Make GL::Error exceptions within signal handlers behave like the "soft"
 * g_return_if_fail() assertions.  That is, issue a useful warning message
 * but do not abort forcefully unless --g-fatal-warnings has been specified.
 * However, from this point on there are no guarantees whatsoever as to how
 * the program might behave.
 */
static
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

#if defined(G_OS_WIN32) && defined(_DEBUG)
extern "C"
{
/*
 * Because Windows applications are simply too sophisticated for plain old
 * stderr file stream output, we have to install a custom g_log handler to
 * make at least Visual Studio aware of what's going on. Without nagging
 * unsuspecting users with dialogs full of gibberish and hexadecimal error
 * numbers, that is.
 *
 * About the _DEBUG conditional:  Actually I'd prefer to have basic logging
 * functionality available in release builds too, but the CRT library won't
 * let me.
 */
static
void win32_message_log(const char* log_domain, GLogLevelFlags log_level,
                       const char* message, void*)
{
  int report_type = _CRT_WARN;

  if ((log_level & G_LOG_LEVEL_CRITICAL) != 0)
    report_type = _CRT_ERROR;
  if ((log_level & G_LOG_LEVEL_ERROR) != 0)
    report_type = _CRT_ASSERT;

  // This is ugly, but it's better to avoid free allocations at this point.
  // Also, MultiByteToWideChar() is suitably lenient with invalid UTF-8.
  WCHAR modulebuf[64];
  modulebuf[0] = L'\0';
  if (log_domain)
    MultiByteToWideChar(CP_UTF8, 0, log_domain, -1, modulebuf, G_N_ELEMENTS(modulebuf));
  modulebuf[G_N_ELEMENTS(modulebuf) - 1] = L'\0';

  WCHAR messagebuf[768];
  messagebuf[0] = L'\0';
  if (message)
    MultiByteToWideChar(CP_UTF8, 0, message, -1, messagebuf, G_N_ELEMENTS(messagebuf));
  messagebuf[G_N_ELEMENTS(messagebuf) - 1] = L'\0';

  _CrtDbgReportW(report_type, 0, 0, modulebuf, L"%s\n", messagebuf);
}
} // extern "C"
#endif /* G_OS_WIN32 && _DEBUG */

} // anonymous namespace

int main(int argc, char** argv)
{
#if defined(G_OS_WIN32) && defined(_DEBUG)
  g_log_set_default_handler(&win32_message_log, 0);
  _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_DEBUG);
  _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_DEBUG);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif
  try
  {
    Glib::thread_init();
    Gtk::RC::add_default_file(cubescene_rc_filename);

    Gtk::Main main_instance (argc, argv);
    gtk_gl_init(&argc, &argv);
    init_locale();

    Glib::set_application_name(PACKAGE_NAME);
#ifndef G_OS_WIN32
    gtk_window_set_default_icon_name(PACKAGE_TARNAME);
#endif
    Glib::add_exception_handler(&trap_gl_error);

    Somato::MainWindow window;

    window.run_puzzle_solver();
    Gtk::Main::run(*window.get_window());
  }
  catch (const GL::Error& error)
  {
    const Glib::ustring what = error.what();
    g_error("unhandled exception: GL error: %s", what.c_str());
  }
  catch (const Glib::Exception& ex)
  {
    const Glib::ustring what = ex.what();
    g_error("unhandled exception: %s", what.c_str());
  }
  catch (const std::exception& ex)
  {
    g_error("unhandled exception: %s", ex.what());
  }
  catch (...)
  {
    g_error("unhandled exception: (type unknown)");
  }

  return 0;
}
