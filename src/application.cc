/*
 * Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "application.h"
#include "appdata.h"
#include "mainwindow.h"

#include <giomm/menumodel.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/builder.h>

#include <algorithm>
#include <memory>

namespace
{

const char *const program_license =
  "Somato is free software; you can redistribute it and/or modify it "
  "under the terms of the GNU General Public License as published by "
  "the Free Software Foundation; either version 2 of the License, or "
  "(at your option) any later version.\n"
  "\n"
  "Somato is distributed in the hope that it will be useful, "
  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
  "GNU General Public License for more details.\n"
  "\n"
  "You should have received a copy of the GNU General Public License "
  "along with Somato; if not, write to the Free Software Foundation, "
  "Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA\n";

} // anonymous namespace

namespace Somato
{

Glib::RefPtr<Application> Application::create()
{
  return Glib::RefPtr<Application>{new Application{}};
}

Application::~Application()
{}

Application::Application()
:
  Gtk::Application{"org.gtk.somato"}
{}

void Application::on_startup()
{
  Gtk::Application::on_startup();

  Gtk::Window::set_default_icon_name(PACKAGE_TARNAME);

  add_action("shortcuts", sigc::mem_fun(*this, &Application::show_shortcuts));
  add_action("about",     sigc::mem_fun(*this, &Application::show_about));
  add_action("quit",      sigc::mem_fun(*this, &Application::close_all));
  {
    const auto ui = Gtk::Builder::create_from_file(Util::locate_data_file("app-menu.ui"));
    set_app_menu(Glib::RefPtr<Gio::MenuModel>::cast_dynamic(ui->get_object("app-menu")));
  }
  set_accel_for_action ("app.quit",         "<Primary>q");
  set_accel_for_action ("win.first",        "Home");
  set_accel_for_action ("win.prev",         "Prior");
  set_accel_for_action ("win.next",         "Next");
  set_accel_for_action ("win.last",         "End");
  set_accel_for_action ("win.pause",        "space");
  set_accel_for_action ("win.cycle",        "c");
  set_accel_for_action ("win.grid",         "g");
  set_accel_for_action ("win.outline",      "o");
  set_accel_for_action ("win.antialias",    "a");
  set_accels_for_action("win.fullscreen",  {"f", "F11"});
  set_accel_for_action ("win.unfullscreen", "Escape");
  set_accels_for_action("win.zoom-plus",   {"plus", "equal"});
  set_accel_for_action ("win.zoom-minus",   "minus");
  set_accels_for_action("win.zoom-reset",  {"1", "0"});
  set_accels_for_action("win.speed-plus",  {"<Primary>plus", "<Primary>equal"});
  set_accel_for_action ("win.speed-minus",  "<Primary>minus");
  set_accels_for_action("win.speed-reset", {"<Primary>1", "<Primary>0"});
}

void Application::on_activate()
{
  Gtk::Application::on_activate();

  if (auto *const window = get_active_window())
  {
    window->present();
    return;
  }

  Somato::MainWindow* app_window = nullptr;
  {
    const auto ui = Gtk::Builder::create_from_file(Util::locate_data_file("mainwindow.glade"));
    ui->get_widget_derived("app_window", app_window);
  }
  add_window(*app_window);

  app_window->run_puzzle_solver();
  app_window->present();
}

void Application::on_window_removed(Gtk::Window* window)
{
  Gtk::Application::on_window_removed(window);

  delete window;
}

void Application::show_shortcuts()
{
}

void Application::show_about()
{
  MainWindow* main_window = nullptr;

  for (Gtk::Window *const window : get_windows())
  {
    if (auto *const dialog = dynamic_cast<Gtk::AboutDialog*>(window))
    {
      dialog->present();
      return;
    }
    if (!main_window)
      main_window = dynamic_cast<MainWindow*>(window);
  }
  std::unique_ptr<Gtk::AboutDialog> dialog {new Gtk::AboutDialog{}};

  dialog->set_version(PACKAGE_VERSION);
  dialog->set_logo_icon_name(PACKAGE_TARNAME);

  dialog->set_comments("An animated solver of the Soma puzzle by Piet Hein.");
  dialog->set_copyright(u8"Copyright \u00A9 2004-2017 Daniel Elstner");
  dialog->set_website(PACKAGE_URL);

  dialog->set_authors({"Daniel Elstner <daniel.kitta@gmail.com>"});
  dialog->set_license(program_license);
  dialog->set_wrap_license(true);

  if (main_window)
    dialog->set_transient_for(*main_window);

  add_window(*dialog);
  dialog.release()->present();
}

void Application::close_all()
{
  for (Gtk::Window *const window : get_windows())
    window->hide();
}

} // namespace Somato
