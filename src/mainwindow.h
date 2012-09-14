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

#ifndef SOMATO_GUARD_MAINWINDOW_H
#define SOMATO_GUARD_MAINWINDOW_H

#include "puzzle.h"

#include <gdk/gdkevents.h>
#include <sigc++/sigc++.h>
#include <glibmm/refptr.h>
#include <glibmm/timer.h>

#include <memory>
#include <vector>

#ifndef SOMATO_HIDE_FROM_INTELLISENSE
namespace Gtk
{
class ActionGroup;
class Box;
class Container;
class Range;
class Statusbar;
class UIManager;
class Widget;
class Window;
}
#endif

namespace Somato
{

class CubeScene;

class MainWindow : public sigc::trackable
{
public:
  MainWindow();
  virtual ~MainWindow();

  Gtk::Window* get_window();

  void run_puzzle_solver();

private:
  struct Actions;

  Glib::RefPtr<Gtk::UIManager>  ui_manager_;
  std::unique_ptr<Actions>      actions_;

  std::unique_ptr<Gtk::Window>  window_;
  Gtk::Box*                     vbox_main_;
  Gtk::Container*               frame_scene_;
  CubeScene*                    cube_scene_;
  Gtk::Range*                   scale_speed_;
  Gtk::Range*                   scale_zoom_;
  Gtk::Statusbar*               statusbar_;

  std::unique_ptr<Gtk::Window>  aboutdialog_;

  std::vector<Solution>         solutions_;
  std::unique_ptr<PuzzleThread> puzzle_thread_;
  Glib::Timer                   profile_timer_;
  sigc::connection              conn_cycle_;
  sigc::connection              conn_profile_;
  int                           cube_index_;

  unsigned int                  context_cube_;
  unsigned int                  context_profile_;

  Glib::RefPtr<Gtk::ActionGroup> create_action_group();

  void load_ui();
  void init_cube_scene();
  bool start_animation();
  void switch_cube(int index);

  void on_puzzle_thread_done();
  void on_speed_value_changed();
  void on_zoom_value_changed();

  void on_ui_add_widget(Gtk::Widget* widget);
  void on_cube_goto_first();
  void on_cube_go_back();
  void on_cube_go_forward();
  void on_cube_goto_last();
  void on_animation_play();
  void on_animation_pause();
  void on_application_about();
  void on_aboutdialog_response(int response_id);
  void on_toggle_fullscreen();
  void on_exit_fullscreen();
  void on_toggle_outline();
  void on_toggle_wireframe();
  void on_toggle_antialias();
  void on_toggle_profile();

  bool on_profile_idle();

  bool on_scene_button_press_event(GdkEventButton* event);
  bool on_scene_scroll_event(GdkEventScroll* event);
  void on_scene_cycle_finished();
};

} // namespace Somato

#endif /* SOMATO_GUARD_MAINWINDOW_H */
