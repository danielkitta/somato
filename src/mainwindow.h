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

#include <gdk/gdk.h>
#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <gtkmm/applicationwindow.h>

#include <memory>
#include <vector>

namespace Gio { class SimpleAction; }

namespace Gtk
{
  class Adjustment;
  class Builder;
}

namespace Somato
{

class CubeScene;

class MainWindow : public Gtk::ApplicationWindow
{
public:
  MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& ui);
  virtual ~MainWindow();

  void run_puzzle_solver();

protected:
  bool on_window_state_event(GdkEventWindowState* event) override;

private:
  Glib::RefPtr<Gtk::Adjustment>   zoom_;
  Glib::RefPtr<Gtk::Adjustment>   speed_;

  Glib::RefPtr<Gio::SimpleAction> action_first_,
                                  action_prev_,
                                  action_next_,
                                  action_last_,
                                  action_fullscreen_,
                                  action_unfullscreen_,
                                  action_opt_menu_,
                                  action_pause_,
                                  action_cycle_,
                                  action_grid_,
                                  action_outline_,
                                  action_antialias_,
                                  action_zoom_plus_,
                                  action_zoom_minus_,
                                  action_zoom_reset_,
                                  action_speed_plus_,
                                  action_speed_minus_,
                                  action_speed_reset_;

  CubeScene*                      cube_scene_  = nullptr;
  std::vector<Solution>           solutions_;
  std::unique_ptr<PuzzleThread>   puzzle_thread_;
  sigc::connection                conn_cycle_;
  int                             cube_index_    = -1;
  bool                            is_fullscreen_ = false;

  void init_cube_scene();
  bool start_animation();
  void switch_cube(int index);

  void on_puzzle_thread_done();
  void on_speed_value_changed();
  void on_zoom_value_changed();

  void on_cube_goto_first();
  void on_cube_go_back();
  void on_cube_go_forward();
  void on_cube_goto_last();
  void on_animation_play(const Glib::VariantBase& state);
  void on_animation_pause(const Glib::VariantBase& state);
  void on_toggle_fullscreen();
  void on_toggle_outline(const Glib::VariantBase& state);
  void on_toggle_wireframe(const Glib::VariantBase& state);
  void on_toggle_antialias(const Glib::VariantBase& state);

  bool on_scene_button_press_event(GdkEventButton* event);
  bool on_scene_scroll_event(GdkEventScroll* event);
  void on_scene_cycle_finished();
};

} // namespace Somato

#endif /* SOMATO_GUARD_MAINWINDOW_H */
