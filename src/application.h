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

#ifndef SOMATO_APPLICATION_H_INCLUDED
#define SOMATO_APPLICATION_H_INCLUDED

#include <gtkmm/application.h>

namespace Somato
{

class Application : public Gtk::Application
{
public:
  static Glib::RefPtr<Application> create();
  virtual ~Application();

protected:
  Application();

  void on_startup() override;
  void on_activate() override;
  void on_window_removed(Gtk::Window* window) override;

private:
  void show_shortcuts();
  void show_about();
  void close_all();
};

} // namespace Somato

#endif // !SOMATO_APPLICATION_H_INCLUDED
