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

#include "glshader.h"
#include "glutils.h"
#include "vectormath.h"

#include <glibmm/ustring.h>
#include <gdkmm/glcontext.h>
#include <gtkmm/glarea.h>
#include <memory>
#include <utility>
#include <vector>

namespace Pango
{
  class Context;
  class Layout;
}

namespace GL
{

class LayoutTexture;

struct Extensions
{
  bool  debug_output               = false;
  bool  texture_filter_anisotropic = false;
  float max_anisotropy             = 1.;

  void gl_query();
};

struct UIVertex
{
  float vertex[2]   = {0., 0.};
  float texcoord[2] = {0., 0.};

  void set_vertex(float x, float y) { vertex[0] = x; vertex[1] = y; }
  void set_texcoord(float s, float t) { texcoord[0] = s; texcoord[1] = t; }
};

typedef std::vector<std::unique_ptr<LayoutTexture>> LayoutVector;
typedef std::vector<UIVertex> GeometryVector;

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

  void set_show_focus(bool show_focus);
  bool get_show_focus() const;

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

  const GL::Extensions* gl_ext() const { return &gl_extensions_; }
  ContextGuard scoped_make_current();

  void start_animation_tick();
  void stop_animation_tick();
  void reset_animation_tick();
  bool animation_tick_active() const { return (anim_tick_id_ != 0); }
  void queue_static_draw();

  int get_viewport_width() const;
  int get_viewport_height() const;

  LayoutTexture* create_layout_texture();
  void gl_update_ui();

  virtual void gl_initialize();
  virtual void gl_cleanup();
  virtual void gl_reset_state();
  virtual int  gl_render() = 0;
  virtual void gl_update_projection();
  virtual void gl_update_color();

  void on_realize() override;
  void on_unrealize() override;
  void on_size_allocate(Gtk::Allocation& allocation) override;
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
  void on_style_updated() override;
  void on_state_changed(Gtk::StateType previous_state) override;
  void on_direction_changed(Gtk::TextDirection previous_direction) override;

  Glib::RefPtr<Gdk::GLContext> on_create_context() override;

private:
  virtual bool on_animation_tick(gint64 animation_time);
  virtual void gl_reposition_layouts();

  void gl_create_label_shader();
  void gl_create_focus_shader();

  void gl_update_framebuffer();
  void gl_delete_framebuffer();

  void gl_update_layouts();
  void gl_update_ui_buffer();
  void gl_build_focus(UIVertex* vertices);
  void gl_build_layouts(UIVertex* vertices);

  void gl_render_focus();
  int  gl_render_layouts(LayoutVector::const_iterator first);
  int  gl_render_layout_arrays(LayoutVector::const_iterator first);

  Glib::RefPtr<Pango::Layout> create_texture_pango_layout(const Glib::ustring& text);

  static gboolean tick_callback(GtkWidget* widget, GdkFrameClock* frame_clock,
                                gpointer user_data);
  static void tick_callback_destroy(gpointer user_data);

  Math::Vector4     focus_color_ = {0.6, 0.6, 0.6, 1.};
  gint64            anim_start_time_ = 0;

  Glib::RefPtr<Pango::Context> texture_context_;
  LayoutVector                 ui_layouts_;

  GL::Extensions    gl_extensions_;
  GL::ShaderProgram label_shader_;
  int               label_uf_color_   = -1;
  int               label_uf_texture_ = -1;
  GL::ShaderProgram focus_shader_;
  int               focus_uf_color_   = -1;

  int          aa_samples_      = 0;
  int          max_aa_samples_  = 0;

  unsigned int render_buffers_[2] = {0, 0};
  unsigned int frame_buffer_      = 0;

  unsigned int ui_vertex_count_  = 0;
  unsigned int ui_vertex_array_  = 0;
  unsigned int ui_buffer_        = 0;
  unsigned int frame_counter_    = 0;
  unsigned int triangle_counter_ = 0;
  unsigned int anim_tick_id_     = 0;

  bool first_tick_     = false;
  bool show_focus_     = true;
  bool focus_drawable_ = false;
};

} // namespace GL

#endif /* SOMATO_GLSCENE_H_INCLUDED */
