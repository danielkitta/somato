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

#include "glscene.h"
#include "gltextlayout.h"
#include "glutils.h"
#include "mathutils.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <cairomm/context.h>
#include <gdkmm.h>
#include <epoxy/gl.h>

#include <algorithm>
#include <utility>

namespace
{

/* Renderbuffer array indices.
 */
enum
{
  COLOR = 0,
  DEPTH = 1
};

extern "C"
{
static GLAPIENTRY
void gl_on_debug_message(GLenum, GLenum, GLuint, GLenum, GLsizei,
                         const GLchar* message, const GLvoid*)
{
  g_log(GL::log_domain, G_LOG_LEVEL_DEBUG, "%s", message);
}
} // extern "C"

} // anonymous namespace

namespace GL
{

Scene::Scene(BaseObjectType* obj)
:
  Gtk::GLArea{obj},
  text_layouts_ {new TextLayoutAtlas{}}
{
  add_events(Gdk::FOCUS_CHANGE_MASK);
}

Scene::~Scene()
{}

void Scene::reset_counters()
{
  frame_counter_    = 0;
  triangle_counter_ = 0;
}

unsigned int Scene::get_frame_counter() const
{
  return frame_counter_;
}

unsigned int Scene::get_triangle_counter() const
{
  return triangle_counter_;
}

void Scene::set_multisample(int n_samples)
{
  const int samples_set = std::min(aa_samples_, max_aa_samples_);
  aa_samples_ = n_samples;

  if (n_samples != samples_set)
  {
    size_changed_ = true;
    queue_static_draw();
  }
}

int Scene::get_multisample() const
{
  return aa_samples_;
}

void Scene::start_animation_tick()
{
  g_return_if_fail(anim_tick_id_ == 0);

  first_tick_ = true;
  anim_tick_id_ = gtk_widget_add_tick_callback(Gtk::Widget::gobj(), &Scene::tick_callback,
                                               this, &Scene::tick_callback_destroy);
}

void Scene::stop_animation_tick()
{
  if (anim_tick_id_ != 0)
    gtk_widget_remove_tick_callback(Gtk::Widget::gobj(), anim_tick_id_);
}

void Scene::reset_animation_tick()
{
  first_tick_ = true;
}

void Scene::queue_static_draw()
{
  if (anim_tick_id_ == 0 && get_is_drawable())
    queue_draw();
}

void Scene::gl_initialize()
{
  gl_update_viewport();
  text_layouts_->gl_init();

  // The source function is identity because we blend in an intensity
  // texture. That is, the color channels are premultiplied by alpha.
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void Scene::gl_cleanup()
{
  text_layouts_->gl_delete();
  gl_delete_framebuffer();
}

/*
 * Note that this method is pure virtual because at least a glClear() command
 * needs to be issued by an overriding method to make the whole thing work.
 */
int Scene::gl_render()
{
  int triangle_count = 0;

  if (text_layouts_->is_drawable())
  {
    glEnable(GL_BLEND);

    triangle_count = text_layouts_->gl_draw_layouts(has_focus());

    glBindVertexArray(0);
    glDisable(GL_BLEND);
  }
  return triangle_count;
}

void Scene::gl_update_viewport()
{
  scale_factor_ = get_scale_factor();
  alloc_width_  = std::max(1, get_allocated_width());
  alloc_height_ = std::max(1, get_allocated_height());

  g_log(GL::log_domain, G_LOG_LEVEL_DEBUG, "Viewport resized to %dx%d",
        get_viewport_width(), get_viewport_height());

  gl_update_framebuffer();
  glViewport(0, 0, get_viewport_width(), get_viewport_height());

  size_changed_ = false;
}

void Scene::on_realize()
{
  text_layouts_->unset_pango_context();

  Gtk::GLArea::on_realize();

  if (auto guard = scoped_make_current())
  {
    const auto context = get_context();
    const bool use_es = context->get_use_es();

    int major = 0, minor = 0;
    context->get_version(major, minor);

    GL::Extensions::query(use_es, major, minor);

    if (GL::extensions().debug_output)
    {
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                            0, nullptr, GL_TRUE);
      glDebugMessageCallback(&gl_on_debug_message, nullptr);
    }
    max_aa_samples_ = 0;

    // Don't enable multi-sample AA on OpenGL ES, as it may be broken
    // even though advertised.
    if (!use_es)
      glGetIntegerv(GL_MAX_SAMPLES, &max_aa_samples_);

    gl_initialize();
  }
}

void Scene::on_unrealize()
{
  if (const auto context = get_context())
  {
    // No need for scoped acquisition here, as Gtk::GLArea's unrealize
    // handler takes care of the final context clear.
    context->make_current();
    gl_cleanup();
  }
  Gtk::GLArea::on_unrealize();

  text_layouts_->unset_pango_context();
}

void Scene::on_size_allocate(Gtk::Allocation& allocation)
{
  size_changed_ = true;

  Gtk::GLArea::on_size_allocate(allocation);
}

/*
 * Instead of hooking into Gtk::GLArea::on_render(), completely replace
 * the drawing logic of Gtk::GLArea to bypass it. That way, we can create
 * our own frame buffer configuration with multi-sample render buffers,
 * without interference from Gtk::GLArea's hard-coded setup.
 */
bool Scene::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (!text_layouts_->has_pango_context())
  {
    auto context = create_pango_context();
    context->set_resolution(get_scale_factor() * 96);
    text_layouts_->set_pango_context(std::move(context));
  }
  make_current();

  if (size_changed_)
    gl_update_viewport();

  if (text_layouts_->update_needed())
    text_layouts_->gl_update(get_viewport_width(), get_viewport_height());

  const unsigned int triangle_count = gl_render();

  gdk_cairo_draw_from_gl(cr->cobj(), gtk_widget_get_window(Gtk::Widget::gobj()),
                         render_buffers_[COLOR], GL_RENDERBUFFER,
                         scale_factor_, 0, 0,
                         get_viewport_width(), get_viewport_height());
  ++frame_counter_;
  triangle_counter_ += triangle_count;

  return true;
}

void Scene::on_style_updated()
{
  text_layouts_->unset_pango_context();

  Gtk::GLArea::on_style_updated();
}

void Scene::on_direction_changed(Gtk::TextDirection previous_direction)
{
  text_layouts_->unset_pango_context();

  Gtk::GLArea::on_direction_changed(previous_direction);
}

Glib::RefPtr<Gdk::GLContext> Scene::on_create_context()
{
  if (const auto window = get_window())
  {
    try
    {
      auto context = window->create_gl_context();
      g_warn_if_fail(context);

      if (context)
      {
        int major = 0, minor = 0;
        context->get_required_version(major, minor);

        // The default minimum OpenGL ES version is 2.0, but we require 3.0.
        // For desktop OpenGL the default minimum version is already 3.2.
        if (major < 3)
          context->set_required_version(3, 0);

        context->set_debug_enabled(GL::debug_mode_requested());

        if (context->realize())
          return std::move(context);
      }
    }
    catch (const Glib::Error& error)
    {
      set_error(error);
    }
  }
  return {};
}

bool Scene::on_animation_tick(gint64)
{
  return false;
}

bool Scene::try_make_current()
{
  const auto context = get_context();

  if (context)
    context->make_current();

  return !!context;
}

void Scene::gl_update_framebuffer()
{
  gl_delete_framebuffer();

  glGenRenderbuffers(G_N_ELEMENTS(render_buffers_), render_buffers_);
  GL::Error::throw_if_fail(render_buffers_[COLOR] && render_buffers_[DEPTH]);

  glGenFramebuffers(1, &frame_buffer_);
  GL::Error::throw_if_fail(frame_buffer_ != 0);

  const int samples = std::min(aa_samples_, max_aa_samples_);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[COLOR]);
  GL::set_object_label(GL_RENDERBUFFER, render_buffers_[COLOR], "sceneColor");

  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB8,
                                   get_viewport_width(), get_viewport_height());

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[DEPTH]);
  GL::set_object_label(GL_RENDERBUFFER, render_buffers_[DEPTH], "sceneDepth");

  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24,
                                   get_viewport_width(), get_viewport_height());
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer_);
  GL::set_object_label(GL_FRAMEBUFFER, frame_buffer_, "sceneFrame");

  glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, render_buffers_[COLOR]);
  glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, render_buffers_[DEPTH]);

  const GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
    throw GL::FramebufferError{status};
}

void Scene::gl_delete_framebuffer()
{
  if (frame_buffer_)
  {
    glDeleteFramebuffers(1, &frame_buffer_);
    frame_buffer_ = 0;
  }
  if (render_buffers_[COLOR] | render_buffers_[DEPTH])
  {
    glDeleteRenderbuffers(G_N_ELEMENTS(render_buffers_), render_buffers_);
    render_buffers_[COLOR] = 0;
    render_buffers_[DEPTH] = 0;
  }
}

gboolean Scene::tick_callback(GtkWidget*, GdkFrameClock* frame_clock, gpointer user_data)
{
  auto *const scene = static_cast<Scene*>(user_data);
  const gint64 now = gdk_frame_clock_get_frame_time(frame_clock);

  if (scene->first_tick_)
  {
    scene->first_tick_ = false;
    scene->anim_start_time_ = now;
  }
  return scene->on_animation_tick(now - scene->anim_start_time_);
}

void Scene::tick_callback_destroy(gpointer user_data)
{
  auto *const scene = static_cast<Scene*>(user_data);

  scene->anim_tick_id_ = 0;
}

} // namespace GL
