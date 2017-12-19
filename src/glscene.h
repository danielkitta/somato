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

#ifndef SOMATO_GLSCENE_H_INCLUDED
#define SOMATO_GLSCENE_H_INCLUDED

#include <gtkmm/glarea.h>
#include <memory>

namespace GL
{

class TextLayoutAtlas;

/*
 * Base GL widget class that implements all the generic stuff not specific
 * to the animation of Soma cubes.  Note that per convention all methods with
 * "gl_" prefix expect the caller to set up the GL context.  Also, be careful
 * to never invoke unknown functions or signal handlers while a GL context is
 * active, as recursive activation is not allowed.
 */
class Scene : public Gtk::GLArea
{
public:
  virtual ~Scene();

  void reset_counters();
  unsigned int get_frame_counter() const;
  unsigned int get_triangle_counter() const;

  void set_multisample(int n_samples);
  int  get_multisample() const;

protected:
  class ContextGuard
  {
  public:
    explicit ContextGuard(bool current) : current_ {current} {}
    ~ContextGuard() { if (current_) Gdk::GLContext::clear_current(); }

    ContextGuard(ContextGuard&& other)
      : current_ {other.current_} { other.current_ = false; }
    ContextGuard(const ContextGuard& other) = delete;
    ContextGuard& operator=(const ContextGuard& other) = delete;

    explicit operator bool() const { return current_; }

  private:
    bool current_;
  };

  explicit Scene(BaseObjectType* obj);

  ContextGuard scoped_make_current() { return ContextGuard{try_make_current()}; }

  void start_animation_tick();
  void stop_animation_tick();
  void reset_animation_tick();
  bool animation_tick_active() const { return (anim_tick_id_ != 0); }
  void queue_static_draw();

  int get_unscaled_width()  const { return alloc_width_;  }
  int get_unscaled_height() const { return alloc_height_; }
  int get_viewport_width()  const { return scale_factor_ * alloc_width_;  }
  int get_viewport_height() const { return scale_factor_ * alloc_height_; }

  TextLayoutAtlas* text_layouts() { return text_layouts_.get(); }

  virtual void gl_initialize();
  virtual void gl_cleanup();
  virtual int  gl_render() = 0;
  virtual void gl_update_viewport();

  void on_realize() override;
  void on_unrealize() override;
  void on_size_allocate(Gtk::Allocation& allocation) override;
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
  void on_style_updated() override;
  void on_direction_changed(Gtk::TextDirection previous_direction) override;

  Glib::RefPtr<Gdk::GLContext> on_create_context() override;

private:
  virtual bool on_animation_tick(gint64 animation_time);

  bool try_make_current();

  void gl_update_framebuffer();
  void gl_delete_framebuffer();

  static gboolean tick_callback(GtkWidget* widget, GdkFrameClock* frame_clock,
                                gpointer user_data);
  static void tick_callback_destroy(gpointer user_data);

  std::unique_ptr<TextLayoutAtlas> text_layouts_;

  gint64        anim_start_time_    = 0;
  unsigned int  anim_tick_id_       = 0;
  unsigned int  frame_counter_      = 0;
  unsigned int  triangle_counter_   = 0;

  unsigned int  frame_buffer_       = 0;
  unsigned int  render_buffers_[2]  = {0, 0};
  int           aa_samples_         = 0;
  int           max_aa_samples_     = 0;
  int           scale_factor_       = 1;
  int           alloc_width_        = 1;
  int           alloc_height_       = 1;

  bool          first_tick_         = false;
  bool          size_changed_       = true;
};

} // namespace GL

#endif // !SOMATO_GLSCENE_H_INCLUDED
