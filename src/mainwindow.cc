/*
 * Copyright (c) 2004-2008  Daniel Elstner  <daniel.kitta@gmail.com>
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
#include "appdata.h"
#include "cube.h"
#include "cubescene.h"
#include "glutils.h"
#include "mathutils.h"
#include "vectormath.h"

#include <glib.h>
#include <gtk/gtkmain.h>
#include <glibmm.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/accelkey.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/frame.h>
#include <gtkmm/menu.h>
#include <gtkmm/scale.h>
#include <gtkmm/statusbar.h>
#include <gtkmm/stock.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/uimanager.h>
#include <gtkmm/window.h>
#include <libglademm/xml.h>

#include <cmath>
#include <algorithm>
#include <iomanip>

#include <config.h>

namespace
{

static const char *const program_license =
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

static
void range_increment(Gtk::Range* range)
{
  const double step = range->get_adjustment()->get_step_increment();

  range->set_value(range->get_value() + step);
}

static
void range_decrement(Gtk::Range* range)
{
  const double step = range->get_adjustment()->get_step_increment();

  range->set_value(range->get_value() - step);
}

} // anonymous namespace

namespace Somato
{

/*
 * The UI actions are put into a struct in order to keep them around for
 * activation, toggling, change of sensitivity and so on.  Due to frequent
 * use, looking up the actions by path would both be cumbersome and add to
 * code bloat.
 */
struct MainWindow::Actions
{
  Glib::RefPtr<Gtk::Action>       application_about;
  Glib::RefPtr<Gtk::Action>       application_quit;

  Glib::RefPtr<Gtk::Action>       cube_goto_first;
  Glib::RefPtr<Gtk::Action>       cube_go_back;
  Glib::RefPtr<Gtk::Action>       cube_go_forward;
  Glib::RefPtr<Gtk::Action>       cube_goto_last;

  Glib::RefPtr<Gtk::ToggleAction> animation_play;
  Glib::RefPtr<Gtk::ToggleAction> animation_pause;

  Glib::RefPtr<Gtk::ToggleAction> toggle_fullscreen;
  Glib::RefPtr<Gtk::ToggleAction> toggle_outline;
  Glib::RefPtr<Gtk::ToggleAction> toggle_wireframe;
  Glib::RefPtr<Gtk::ToggleAction> toggle_profile;

  Glib::RefPtr<Gtk::Action>       exit_fullscreen;

  Glib::RefPtr<Gtk::Action>       speed_plus;
  Glib::RefPtr<Gtk::Action>       speed_minus;
  Glib::RefPtr<Gtk::Action>       speed_reset;

  Glib::RefPtr<Gtk::Action>       zoom_plus;
  Glib::RefPtr<Gtk::Action>       zoom_minus;
  Glib::RefPtr<Gtk::Action>       zoom_reset;

  Actions();
  ~Actions();

private:
  // noncopyable
  Actions(const Actions&);
  Actions& operator=(const Actions&);
};

MainWindow::Actions::Actions()
:
  application_about (Gtk::Action::create("application_about", Gtk::Stock::ABOUT)),
  application_quit  (Gtk::Action::create("application_quit",  Gtk::Stock::QUIT)),

  cube_goto_first   (Gtk::Action::create("cube_goto_first", Gtk::Stock::GOTO_FIRST)),
  cube_go_back      (Gtk::Action::create("cube_go_back",    Gtk::Stock::GO_BACK)),
  cube_go_forward   (Gtk::Action::create("cube_go_forward", Gtk::Stock::GO_FORWARD)),
  cube_goto_last    (Gtk::Action::create("cube_goto_last",  Gtk::Stock::GOTO_LAST)),

  animation_play    (Gtk::ToggleAction::create("animation_play",  Gtk::Stock::MEDIA_PLAY)),
  animation_pause   (Gtk::ToggleAction::create("animation_pause", Gtk::Stock::MEDIA_PAUSE,
                                               Glib::ustring(), Glib::ustring(), true)),

  toggle_fullscreen (Gtk::ToggleAction::create("toggle_fullscreen", "_Fullscreen Mode")),
  toggle_outline    (Gtk::ToggleAction::create("toggle_outline",    "Show _Outline")),
  toggle_wireframe  (Gtk::ToggleAction::create("toggle_wireframe",  "Show _Wireframe")),
  toggle_profile    (Gtk::ToggleAction::create("toggle_profile",    "_Profile Framerate")),

  exit_fullscreen   (Gtk::Action::create("exit_fullscreen")),

  speed_plus        (Gtk::Action::create("speed_plus")),
  speed_minus       (Gtk::Action::create("speed_minus")),
  speed_reset       (Gtk::Action::create("speed_reset")),

  zoom_plus         (Gtk::Action::create("zoom_plus")),
  zoom_minus        (Gtk::Action::create("zoom_minus")),
  zoom_reset        (Gtk::Action::create("zoom_reset"))
{
  animation_play ->property_is_important() = true;
  animation_pause->property_is_important() = true;
}

MainWindow::Actions::~Actions()
{}

MainWindow::MainWindow()
:
  ui_manager_       {Gtk::UIManager::create()},
  actions_          {new Actions()},
  vbox_main_        {nullptr},
  frame_scene_      {nullptr},
  cube_scene_       {nullptr},
  scale_speed_      {nullptr},
  scale_zoom_       {nullptr},
  statusbar_        {nullptr},
  cube_index_       {-1},
  context_cube_     {0},
  context_profile_  {0}
{
  load_ui();
}

MainWindow::~MainWindow()
{}

Gtk::Window* MainWindow::get_window()
{
  return window_.get();
}

void MainWindow::run_puzzle_solver()
{
  std::unique_ptr<PuzzleThread> thread {new PuzzleThread()};

  thread->signal_done().connect(sigc::mem_fun(*this, &MainWindow::on_puzzle_thread_done));
  thread->run();

  puzzle_thread_ = std::move(thread);
}

Glib::RefPtr<Gtk::ActionGroup> MainWindow::create_action_group()
{
  const Glib::RefPtr<Gtk::ActionGroup> group = Gtk::ActionGroup::create("main_window");

  group->add(actions_->application_about,
             sigc::mem_fun(*this, &MainWindow::on_application_about));

  group->add(actions_->application_quit, Gtk::AccelKey("q"),
             sigc::mem_fun(*window_, &Gtk::Widget::hide));

  group->add(actions_->cube_goto_first, Gtk::AccelKey("<shift>b"),
             sigc::mem_fun(*this, &MainWindow::on_cube_goto_first));

  group->add(actions_->cube_go_back, Gtk::AccelKey("b"),
             sigc::mem_fun(*this, &MainWindow::on_cube_go_back));

  group->add(actions_->cube_go_forward, Gtk::AccelKey("n"),
             sigc::mem_fun(*this, &MainWindow::on_cube_go_forward));

  group->add(actions_->cube_goto_last, Gtk::AccelKey("<shift>n"),
             sigc::mem_fun(*this, &MainWindow::on_cube_goto_last));

  group->add(actions_->animation_play, Gtk::AccelKey("p"),
             sigc::mem_fun(*this, &MainWindow::on_animation_play));

  group->add(actions_->animation_pause, Gtk::AccelKey("space"),
             sigc::mem_fun(*this, &MainWindow::on_animation_pause));

  group->add(actions_->toggle_fullscreen, Gtk::AccelKey("f"),
             sigc::mem_fun(*this, &MainWindow::on_toggle_fullscreen));

  group->add(actions_->toggle_outline, Gtk::AccelKey("o"),
             sigc::mem_fun(*this, &MainWindow::on_toggle_outline));

  group->add(actions_->toggle_wireframe, Gtk::AccelKey("w"),
             sigc::mem_fun(*this, &MainWindow::on_toggle_wireframe));

  group->add(actions_->toggle_profile, Gtk::AccelKey("<control>p"),
             sigc::mem_fun(*this, &MainWindow::on_toggle_profile));

  // The "I'm an unsuspecting user, get me out of here!" key
  group->add(actions_->exit_fullscreen, Gtk::AccelKey("Escape"),
             sigc::mem_fun(*this, &MainWindow::on_exit_fullscreen));

  group->add(actions_->speed_plus, Gtk::AccelKey("<control>plus"),
             sigc::bind(&range_increment, scale_speed_));

  group->add(actions_->speed_minus, Gtk::AccelKey("<control>minus"),
             sigc::bind(&range_decrement, scale_speed_));

  group->add(actions_->speed_reset, Gtk::AccelKey("<control>1"),
             sigc::bind(sigc::mem_fun(*scale_speed_, &Gtk::Range::set_value), 0.0));

  group->add(actions_->zoom_plus, Gtk::AccelKey("plus"),
             sigc::bind(&range_increment, scale_zoom_));

  group->add(actions_->zoom_minus, Gtk::AccelKey("minus"),
             sigc::bind(&range_decrement, scale_zoom_));

  group->add(actions_->zoom_reset, Gtk::AccelKey("1"),
             sigc::bind(sigc::mem_fun(*scale_zoom_, &Gtk::Range::set_value), 0.0));

  return group;
}

void MainWindow::load_ui()
{
  const Glib::RefPtr<Gnome::Glade::Xml> xml =
      Gnome::Glade::Xml::create(Util::locate_data_file("mainwindow.glade"));

  Gtk::Window* main_window = 0;
  window_.reset(xml->get_widget("main_window", main_window));

  vbox_main_ = dynamic_cast<Gtk::Box*>(window_->get_child());

  xml->get_widget("frame_scene", frame_scene_);
  xml->get_widget("scale_speed", scale_speed_);
  xml->get_widget("scale_zoom",  scale_zoom_);
  xml->get_widget("statusbar",   statusbar_);

  actions_->speed_plus ->connect_proxy(*xml->get_widget("button_speed_plus"));
  actions_->speed_minus->connect_proxy(*xml->get_widget("button_speed_minus"));
  actions_->zoom_plus  ->connect_proxy(*xml->get_widget("button_zoom_plus"));
  actions_->zoom_minus ->connect_proxy(*xml->get_widget("button_zoom_minus"));

  init_cube_scene();

  context_cube_    = statusbar_->get_context_id("context_cube");
  context_profile_ = statusbar_->get_context_id("context_profile");

  scale_speed_->signal_value_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_speed_value_changed));

  scale_zoom_->signal_value_changed().connect(
      sigc::mem_fun(*this, &MainWindow::on_zoom_value_changed));

  actions_->exit_fullscreen->set_sensitive(false);
  switch_cube(-1); // set initial sensitivity of UI actions

  ui_manager_->insert_action_group(create_action_group());
  window_->add_accel_group(ui_manager_->get_accel_group());

  ui_manager_->signal_add_widget().connect(sigc::mem_fun(*this, &MainWindow::on_ui_add_widget));
  ui_manager_->add_ui_from_file(Util::locate_data_file("mainwindow-ui.xml"));

  ui_manager_->ensure_update();
}

void MainWindow::init_cube_scene()
{
  frame_scene_->add(*Gtk::manage(cube_scene_ = new CubeScene()));

  cube_scene_->set_exclusive_context(true);
  cube_scene_->set_name("cube_scene");
  cube_scene_->add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK);

  // Rotate 18 degrees downward and 27 degrees to the right.
  cube_scene_->set_rotation(Math::Quat::from_axis(Math::Vector4(1.0, 0.0, 0.0), 0.10 * G_PI) *
                            Math::Quat::from_axis(Math::Vector4(0.0, 1.0, 0.0), 0.15 * G_PI));

  cube_scene_->signal_scroll_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_scroll_event));

  cube_scene_->signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_button_press_event));

  conn_cycle_ = cube_scene_->signal_cycle_finished().connect(
      sigc::mem_fun(*this, &MainWindow::on_scene_cycle_finished));

  // Synchronize setup with initial UI state.
  on_speed_value_changed();
  on_zoom_value_changed();
  on_toggle_outline();
  on_toggle_wireframe();
  on_animation_play();
  on_animation_pause();

  cube_scene_->show();
  cube_scene_->grab_focus();
}

bool MainWindow::start_animation()
{
  // Now we may safely delete the puzzle thread object.
  puzzle_thread_.reset();

  switch_cube(0);
  actions_->animation_pause->set_active(false);

  return false; // disconnect
}

void MainWindow::switch_cube(int index)
{
  const int max_index = int(solutions_.size()) - 1;

  cube_index_ = Math::min(Math::max(0, index), max_index);

  actions_->cube_goto_first->set_sensitive(cube_index_ > 0);
  actions_->cube_go_back   ->set_sensitive(cube_index_ > 0);
  actions_->cube_go_forward->set_sensitive(cube_index_ < max_index);
  actions_->cube_goto_last ->set_sensitive(cube_index_ < max_index);
  actions_->animation_play ->set_sensitive(cube_index_ >= 0);
  actions_->animation_pause->set_sensitive(cube_index_ >= 0);

  if (cube_index_ >= 0)
  {
    cube_scene_->set_heading(Glib::ustring::compose("Soma cube #%1", cube_index_ + 1));
    cube_scene_->set_cube_pieces(solutions_[cube_index_]);

    statusbar_->pop(context_cube_);
    statusbar_->push(Glib::ustring::compose("%1 triangles, %2 vertices",
                                            cube_scene_->get_cube_triangle_count(),
                                            cube_scene_->get_cube_vertex_count()),
                     context_cube_);
  }
}

void MainWindow::on_puzzle_thread_done()
{
  puzzle_thread_->swap_result(solutions_);

  if (!solutions_.empty())
    Glib::signal_idle().connect(sigc::mem_fun(*this, &MainWindow::start_animation));
}

void MainWindow::on_speed_value_changed()
{
  const Gtk::Adjustment *const adjustment = scale_speed_->get_adjustment();

  const double upper = adjustment->get_upper();
  const double lower = adjustment->get_lower();
  const double value = scale_speed_->get_value();

  actions_->speed_plus ->set_sensitive(value < upper);
  actions_->speed_minus->set_sensitive(value > lower);

  // Interpret the value as exponent of a logarithmic scale in base 10.
  // The exponent needs to be divided by the maximum value since it has
  // been scaled in order to make the step increment a whole number.

  cube_scene_->set_pieces_per_second(std::pow(10.0, value / upper));
}

void MainWindow::on_zoom_value_changed()
{
  const Gtk::Adjustment *const adjustment = scale_zoom_->get_adjustment();

  const double upper = adjustment->get_upper();
  const double lower = adjustment->get_lower();
  const double value = scale_zoom_->get_value();

  actions_->zoom_plus ->set_sensitive(value < upper);
  actions_->zoom_minus->set_sensitive(value > lower);

  // Interpret the value as exponent of a logarithmic scale in base 3.
  // The exponent needs to be divided by the maximum value since it has
  // been scaled in order to make the step increment a whole number.

  cube_scene_->set_zoom(std::pow(3.0, value / upper));
}

void MainWindow::on_ui_add_widget(Gtk::Widget* widget)
{
  vbox_main_->pack_start(*widget, Gtk::PACK_SHRINK);
}

void MainWindow::on_cube_goto_first()
{
  switch_cube(0);
}

void MainWindow::on_cube_go_back()
{
  switch_cube(cube_index_ - 1);
}

void MainWindow::on_cube_go_forward()
{
  switch_cube(cube_index_ + 1);
}

void MainWindow::on_cube_goto_last()
{
  switch_cube(G_MAXINT);
}

void MainWindow::on_animation_play()
{
  conn_cycle_.block(!actions_->animation_play->get_active());
}

void MainWindow::on_animation_pause()
{
  cube_scene_->set_animation_running(!actions_->animation_pause->get_active());
}

void MainWindow::on_application_about()
{
  if (aboutdialog_.get())
  {
    aboutdialog_->present(gtk_get_current_event_time());
  }
  else
  {
    std::unique_ptr<Gtk::AboutDialog> dialog {new Gtk::AboutDialog()};

    dialog->set_version(PACKAGE_VERSION);
#ifndef GDK_WINDOWING_WIN32
    dialog->set_logo_icon_name(PACKAGE_TARNAME);
#endif
    dialog->set_comments("The best Soma puzzle solver ever. For real.");
    dialog->set_copyright("Copyright \302\251 2004-2008 Daniel Elstner");
    dialog->set_website("http://danielkitta.org/projects/somato");

    const char* const program_authors[] = { "Daniel Elstner <daniel.kitta@gmail.com>", nullptr };

    dialog->set_authors(program_authors);
    dialog->set_license(program_license);
    dialog->set_wrap_license(true);

    dialog->set_transient_for(*window_);
    dialog->show();
    dialog->signal_response().connect(sigc::mem_fun(*this, &MainWindow::on_aboutdialog_response));

    aboutdialog_ = std::move(dialog);
  }
}

void MainWindow::on_aboutdialog_response(int)
{
  aboutdialog_.reset();
}

/*
 * If toggle_fullscreen is active, put the main window into fullscreen mode
 * and make cube_scene the only visible child of the window.  If the toggle
 * is inactive revert these actions.
 *
 * Note that the scene does not replace the main vbox but is made the only
 * visible child within it.  This way we can avoid the intermediate state
 * where the widget is not anchored to a toplevel window, thereby causing
 * an unnecessary unrealize/realize cycle.
 *
 * Even if the window manager does not honor the fullscreen request, this
 * function should continue to work without crashing or exhibiting other
 * undesirable behavior.
 */
void MainWindow::on_toggle_fullscreen()
{
  if (actions_->toggle_fullscreen->get_active())
  {
    if (cube_scene_->get_parent() == frame_scene_)
    {
      vbox_main_->foreach(&Gtk::Widget::hide);
      window_->fullscreen();

      cube_scene_->reparent(*vbox_main_);

      cube_scene_->set_show_focus(false);
      cube_scene_->grab_focus();

      actions_->exit_fullscreen->set_sensitive(true);
    }
  }
  else
  {
    if (cube_scene_->get_parent() == vbox_main_)
    {
      cube_scene_->reparent(*frame_scene_);

      window_->unfullscreen();
      vbox_main_->foreach(&Gtk::Widget::show);

      cube_scene_->set_show_focus(true);
      cube_scene_->grab_focus();

      actions_->exit_fullscreen->set_sensitive(false);
    }
  }
}

void MainWindow::on_exit_fullscreen()
{
  actions_->toggle_fullscreen->set_active(false);
}

void MainWindow::on_toggle_outline()
{
  cube_scene_->set_show_outline(actions_->toggle_outline->get_active());
}

void MainWindow::on_toggle_wireframe()
{
  cube_scene_->set_show_wireframe(actions_->toggle_wireframe->get_active());
}

void MainWindow::on_toggle_profile()
{
  conn_profile_.disconnect();

  if (actions_->toggle_profile->get_active())
  {
    cube_scene_->set_use_back_buffer(false);
    statusbar_->push("Profiling...", context_profile_);

    conn_profile_ = Glib::signal_idle().connect(sigc::mem_fun(*this, &MainWindow::on_profile_idle));

    cube_scene_->reset_counters();
    profile_timer_.start();
  }
  else
  {
    cube_scene_->set_use_back_buffer(true);

    statusbar_->pop(context_profile_);
  }
}

bool MainWindow::on_profile_idle()
{
  if (cube_scene_->is_drawable())
  {
    cube_scene_->queue_draw();
    cube_scene_->get_window()->process_updates(true);
  }

  const double elapsed = profile_timer_.elapsed();

  if (elapsed >= 1.0)
  {
    using Glib::ustring;

    const double frames    = cube_scene_->get_frame_counter()    / elapsed;
    const double triangles = cube_scene_->get_triangle_counter() / elapsed;

    const ustring message = ustring::compose("%1 frames/s, %2 triangles/s",
        ustring::format(std::fixed, std::setprecision(0), frames),
        ustring::format(std::fixed, std::setprecision(0), triangles));

    if (actions_->toggle_fullscreen->get_active())
      g_message("%s", message.c_str());

    statusbar_->pop(context_profile_);
    statusbar_->push(message, context_profile_);

    cube_scene_->reset_counters();
    profile_timer_.start();
  }

  return true; // call me again
}

bool MainWindow::on_scene_scroll_event(GdkEventScroll* event)
{
  switch (event->direction)
  {
    case GDK_SCROLL_UP:   actions_->zoom_plus ->activate(); return true;
    case GDK_SCROLL_DOWN: actions_->zoom_minus->activate(); return true;
    default:              return false;
  }
}

bool MainWindow::on_scene_button_press_event(GdkEventButton* event)
{
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
  {
    if (Gtk::Menu *const menu = dynamic_cast<Gtk::Menu*>(ui_manager_->get_widget("/ScenePopup")))
    {
      menu->popup(event->button, event->time);
      return true;
    }
  }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    actions_->toggle_fullscreen->activate();
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
