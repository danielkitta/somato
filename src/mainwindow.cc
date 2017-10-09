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

#include "mainwindow.h"
#include "asynctask.h"
#include "cube.h"
#include "cubescene.h"
#include "mathutils.h"
#include "vectormath.h"

#include <glib.h>
#include <giomm/simpleaction.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/builder.h>
#include <gtkmm/gesturezoom.h>

#include <cmath>
#include <algorithm>

namespace
{

void step_increment(const Glib::RefPtr<Gtk::Adjustment>& adjustment)
{
  const double step = adjustment->get_step_increment();
  adjustment->set_value(adjustment->get_value() + step);
}

void step_decrement(const Glib::RefPtr<Gtk::Adjustment>& adjustment)
{
  const double step = adjustment->get_step_increment();
  adjustment->set_value(adjustment->get_value() - step);
}

} // anonymous namespace

namespace Somato
{

MainWindow::MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& ui)
:
  Gtk::ApplicationWindow{obj},
  zoom_  {Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(ui->get_object("adjustment_zoom"))},
  speed_ {Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(ui->get_object("adjustment_speed"))},

  action_first_        {add_action("first", sigc::mem_fun(*this, &MainWindow::cube_goto_first))},
  action_prev_         {add_action("prev",  sigc::mem_fun(*this, &MainWindow::cube_go_back))},
  action_next_         {add_action("next",  sigc::mem_fun(*this, &MainWindow::cube_go_forward))},
  action_last_         {add_action("last",  sigc::mem_fun(*this, &MainWindow::cube_goto_last))},

  action_fullscreen_   {add_action("fullscreen",   sigc::mem_fun(*this, &MainWindow::toggle_fullscreen))},
  action_unfullscreen_ {add_action("unfullscreen", sigc::mem_fun(*this, &Gtk::Window::unfullscreen))},

  action_opt_menu_     {add_action_bool("opt-menu")},
  action_pause_        {add_action_bool("pause")},
  action_cycle_        {add_action_bool("cycle", true)},
  action_grid_         {add_action_bool("grid")},
  action_outline_      {add_action_bool("outline")},
  action_antialias_    {add_action_bool("antialias", true)},

  action_zoom_plus_    {add_action("zoom-plus",  sigc::bind(&step_increment, zoom_))},
  action_zoom_minus_   {add_action("zoom-minus", sigc::bind(&step_decrement, zoom_))},
  action_zoom_reset_   {add_action("zoom-reset", [this] { zoom_->set_value(0.); })},

  action_speed_plus_   {add_action("speed-plus",  sigc::bind(&step_increment, speed_))},
  action_speed_minus_  {add_action("speed-minus", sigc::bind(&step_decrement, speed_))},
  action_speed_reset_  {add_action("speed-reset", [this] { speed_->set_value(0.); })}
{
  add_action_bool("opt-menu");
  action_unfullscreen_->set_enabled(false);

  ui->get_widget_derived("cube_scene", cube_scene_);
  zoom_gesture_ = Gtk::GestureZoom::create(*cube_scene_);

  init_cube_scene();

  action_pause_    ->signal_change_state().connect(sigc::mem_fun(*this, &MainWindow::set_pause));
  action_cycle_    ->signal_change_state().connect(sigc::mem_fun(*this, &MainWindow::set_cycle));
  action_grid_     ->signal_change_state().connect(sigc::mem_fun(*this, &MainWindow::set_cell_grid));
  action_outline_  ->signal_change_state().connect(sigc::mem_fun(*this, &MainWindow::set_outline));
  action_antialias_->signal_change_state().connect(sigc::mem_fun(*this, &MainWindow::set_antialias));

  zoom_->signal_value_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_zoom_value_changed));
  speed_->signal_value_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_speed_value_changed));
  zoom_gesture_->signal_begin().connect(
      sigc::mem_fun(*this, &MainWindow::on_zoom_gesture_begin));
  zoom_gesture_->signal_scale_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_zoom_gesture_scale_changed));

  // Synchronize initial action enable states.
  switch_cube(-1);
  on_zoom_value_changed();
  on_speed_value_changed();
  set_antialias(action_antialias_->get_state_variant());
}

MainWindow::~MainWindow()
{}

void MainWindow::run_puzzle_solver()
{
  auto thread = std::make_unique<PuzzleThread>();

  thread->signal_done().connect(sigc::mem_fun(*this, &MainWindow::start_animation));
  thread->run();

  puzzle_thread_ = std::move(thread);
}

bool MainWindow::on_window_state_event(GdkEventWindowState* event)
{
  is_fullscreen_ = ((event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0);

  cube_scene_->set_show_focus(!is_fullscreen_);
  action_unfullscreen_->set_enabled(is_fullscreen_);

  return Gtk::ApplicationWindow::on_window_state_event(event);
}

void MainWindow::init_cube_scene()
{
  cube_scene_->add_events(Gdk::BUTTON_PRESS_MASK
                          | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);

  // Rotate 18 degrees downward and 27 degrees to the right.
  cube_scene_->set_rotation(Math::Quat::from_axis(1., 0., 0., 0.10 * G_PI) *
                            Math::Quat::from_axis(0., 1., 0., 0.15 * G_PI));

  cube_scene_->signal_scroll_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_scroll_event));

  cube_scene_->signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_button_press_event));

  conn_cycle_ = cube_scene_->signal_cycle_finished().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_cycle_finished));

  cube_scene_->grab_focus();
}

void MainWindow::start_animation()
{
  const auto puzzle = Async::deferred_delete(puzzle_thread_);
  g_return_if_fail(puzzle);

  solutions_ = puzzle->acquire_results();
  switch_cube(0);

  bool paused = false;
  action_pause_->get_state(paused);

  cube_scene_->set_animation_running(!paused && !solutions_.empty());
}

void MainWindow::switch_cube(int index)
{
  const int max_index = int(solutions_.size()) - 1;

  cube_index_ = std::min(std::max(0, index), max_index);

  action_first_->set_enabled(cube_index_ > 0);
  action_prev_ ->set_enabled(cube_index_ > 0);
  action_next_ ->set_enabled(cube_index_ < max_index);
  action_last_ ->set_enabled(cube_index_ < max_index);
  action_cycle_->set_enabled(cube_index_ >= 0);
  action_pause_->set_enabled(cube_index_ >= 0);

  if (cube_index_ >= 0)
  {
    cube_scene_->set_heading(Glib::ustring::compose("Soma cube #%1", cube_index_ + 1));
    cube_scene_->set_cube_pieces(solutions_[cube_index_]);
  }
}

void MainWindow::on_speed_value_changed()
{
  const double upper = speed_->get_upper();
  const double lower = speed_->get_lower();
  const double value = speed_->get_value();

  action_speed_plus_ ->set_enabled(value < upper);
  action_speed_minus_->set_enabled(value > lower);
  action_speed_reset_->set_enabled(value != 0.);

  // Interpret the value as exponent of a logarithmic scale in base 10.
  // The exponent needs to be divided by the maximum value since it has
  // been scaled in order to make the step increment a whole number.

  cube_scene_->set_pieces_per_second(std::pow(10., value / upper));
}

void MainWindow::on_zoom_value_changed()
{
  const double upper = zoom_->get_upper();
  const double lower = zoom_->get_lower();
  const double value = zoom_->get_value();

  action_zoom_plus_ ->set_enabled(value < upper);
  action_zoom_minus_->set_enabled(value > lower);
  action_zoom_reset_->set_enabled(value != 0.);

  // Interpret the value as exponent of a logarithmic scale in base 3.
  // The exponent needs to be divided by the maximum value since it has
  // been scaled in order to make the step increment a whole number.

  cube_scene_->set_zoom(std::pow(3., value / upper));
}

void MainWindow::on_zoom_gesture_begin(GdkEventSequence*)
{
  gesture_start_zoom_ = zoom_->get_value();
}

void MainWindow::on_zoom_gesture_scale_changed(double scale)
{
  if (scale > 0.)
  {
    const double step  = std::log2(scale) * (1. / std::log2(3.));
    const double upper = zoom_->get_upper();

    zoom_->set_value(gesture_start_zoom_ + step * upper);
  }
}

void MainWindow::cube_goto_first()
{
  switch_cube(0);
}

void MainWindow::cube_go_back()
{
  switch_cube(cube_index_ - 1);
}

void MainWindow::cube_go_forward()
{
  switch_cube(cube_index_ + 1);
}

void MainWindow::cube_goto_last()
{
  switch_cube(G_MAXINT);
}

void MainWindow::set_cycle(const Glib::VariantBase& state)
{
  action_cycle_->set_state(state);
  const bool cycle = static_cast<const Glib::Variant<bool>&>(state).get();

  conn_cycle_.block(!cycle);
}

void MainWindow::set_pause(const Glib::VariantBase& state)
{
  action_pause_->set_state(state);
  const bool pause = static_cast<const Glib::Variant<bool>&>(state).get();

  cube_scene_->set_animation_running(!pause);
}

void MainWindow::toggle_fullscreen()
{
  if (is_fullscreen_)
    unfullscreen();
  else
    fullscreen();
}

void MainWindow::set_outline(const Glib::VariantBase& state)
{
  action_outline_->set_state(state);
  const bool outline = static_cast<const Glib::Variant<bool>&>(state).get();

  cube_scene_->set_show_outline(outline);
}

void MainWindow::set_cell_grid(const Glib::VariantBase& state)
{
  action_grid_->set_state(state);
  const bool grid = static_cast<const Glib::Variant<bool>&>(state).get();

  cube_scene_->set_show_cell_grid(grid);
}

void MainWindow::set_antialias(const Glib::VariantBase& state)
{
  enum : int { AA_SAMPLES = 4 };

  action_antialias_->set_state(state);
  const bool antialias = static_cast<const Glib::Variant<bool>&>(state).get();

  cube_scene_->set_multisample((antialias) ? AA_SAMPLES : 0);
}

bool MainWindow::on_scene_scroll_event(GdkEventScroll* event)
{
  switch (event->direction)
  {
    case GDK_SCROLL_UP:
      action_zoom_plus_->activate();
      return true;
    case GDK_SCROLL_DOWN:
      action_zoom_minus_->activate();
      return true;
    case GDK_SCROLL_SMOOTH:
      if (event->delta_y < 0.)
        action_zoom_minus_->activate();
      else if (event->delta_y > 0.)
        action_zoom_plus_->activate();
      return true;
    default:
      return false;
  }
}

bool MainWindow::on_scene_button_press_event(GdkEventButton* event)
{
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    action_fullscreen_->activate();
    return true;
  }
  return false;
}

void MainWindow::on_scene_cycle_finished()
{
  const int count = solutions_.size();
  const int next  = cube_index_ + 1;

  switch_cube((next < count) ? next : 0);
}

} // namespace Somato
