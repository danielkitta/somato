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
#include "glsceneprivate.h"
#include "glutils.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <gdkmm.h>
#include <epoxy/gl.h>

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <functional>

namespace
{

enum
{
  FOCUS_ARRAY_OFFSET   = 0, // offset into geometry arrays
  FOCUS_VERTEX_COUNT   = 4,
  LAYOUTS_ARRAY_OFFSET = FOCUS_ARRAY_OFFSET + FOCUS_VERTEX_COUNT
};

enum
{
  ATTRIB_POSITION = 0,
  ATTRIB_TEXCOORD = 1
};

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

/*
 * Generate vertices for drawing the focus indicator of the GL widget.
 */
void generate_focus_rect(int width, int height, int padding,
                         volatile GL::UIVertex* geometry)
{
  g_return_if_fail(width > 0 && height > 0);

  const float x0 = static_cast<float>(2 * padding + 1 - width)  / width;
  const float y0 = static_cast<float>(2 * padding + 1 - height) / height;
  const float x1 = static_cast<float>(width  - 2 * padding - 1) / width;
  const float y1 = static_cast<float>(height - 2 * padding - 1) / height;

  geometry[0].set(x0, y0, 0., 0.);
  geometry[1].set(x1, y0, 1., 0.);
  geometry[2].set(x1, y1, 1., 1.);
  geometry[3].set(x0, y1, 0., 1.);
}

/*
 * Make the texture image stride at least a multiple of 8 to meet SGI's
 * alignment recommendation.  This also avoids the padding bytes that would
 * otherwise be necessary in order to ensure row alignment.
 *
 * Note that cairo 1.6 made it mandatory to retrieve the stride to be used
 * with an image surface at runtime.  Currently, the alignment requested by
 * cairo is actually lower than our own, but that could change some day.
 */
inline int aligned_stride(int width)
{
  return Cairo::ImageSurface::format_stride_for_width(Cairo::FORMAT_A8, (width + 7) & ~7);
}

} // anonymous namespace

namespace GL
{

LayoutTexture::LayoutTexture()
:
  color_        {1., 1., 1., 1.},
  need_update_  {false},
  array_offset_ {G_MAXINT},

  tex_name_     {0},
  tex_width_    {0},
  tex_height_   {0},

  ink_x_        {0},
  ink_y_        {0},
  ink_width_    {0},
  ink_height_   {0},

  log_width_    {0},
  log_height_   {0},

  window_x_     {0},
  window_y_     {0}
{}

LayoutTexture::~LayoutTexture()
{
  g_return_if_fail(tex_name_ == 0);
}

void LayoutTexture::set_content(const Glib::ustring& content)
{
  if (content.raw() != content_.raw())
  {
    content_ = content;
    need_update_ = true;
  }
}

void LayoutTexture::gl_set_layout(const Glib::RefPtr<Pango::Layout>& layout)
{
  // Measure ink extents to determine the dimensions of the texture image,
  // but keep the logical extents and the ink offsets around for positioning
  // purposes.  This is to avoid ugly "jumping" of top-aligned text labels
  // when changing the string results in a different ink height.
  Pango::Rectangle ink;
  Pango::Rectangle logical;

  // Note that at this point, the Pango context has already been updated to
  // the target surface and transformation in create_texture_pango_layout().
  layout->get_pixel_extents(ink, logical);

  // Make sure the extents are within reasonable boundaries.
  g_return_if_fail(ink.get_width() < 4095 && ink.get_height() < 4095);

  // Pad up the margins to account for measurement inaccuracies.
  enum { PADDING = 1 };

  const int ink_width  = std::max(0, ink.get_width())  + 2 * PADDING;
  const int ink_height = std::max(0, ink.get_height()) + 2 * PADDING;
  const int img_width  = aligned_stride(ink_width);

  std::vector<GLubyte> tex_image (ink_height * img_width);

  // Create a Cairo surface to draw the layout directly into the texture image.
  // Note that the image will be upside-down from the point of view of OpenGL,
  // thus the texture coordinates need to be adjusted accordingly.
  {
    const auto surface = Cairo::ImageSurface::create(&tex_image[0],
        Cairo::FORMAT_A8, img_width, ink_height, img_width * sizeof(GLubyte));
    const auto context = Cairo::Context::create(surface);

    context->move_to(PADDING - ink.get_x(), PADDING - ink.get_y());
    layout->show_in_cairo_context(context);
  }

  if (!tex_name_)
  {
    tex_width_  = 0;
    tex_height_ = 0;

    glGenTextures(1, &tex_name_);
    GL::Error::throw_if_fail(tex_name_ != 0);
  }
  glBindTexture(GL_TEXTURE_2D, tex_name_);

  if (tex_width_ == 0)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, img_width, ink_height,
               0, GL_RED, GL_UNSIGNED_BYTE, &tex_image[0]);

  tex_width_  = img_width;
  tex_height_ = ink_height;

  ink_width_  = ink_width;
  ink_height_ = ink_height;

  ink_x_ = ink.get_x() - logical.get_x() - PADDING;
  ink_y_ = logical.get_y() + logical.get_height() - ink.get_y() - ink.get_height() - PADDING;

  // Expand the logical rectangle to account for the shadow offset.
  log_width_  = logical.get_width()  + 1;
  log_height_ = logical.get_height() + 1;
}

void LayoutTexture::gl_delete()
{
  if (tex_name_)
  {
    glDeleteTextures(1, &tex_name_);
    tex_name_ = 0;
  }
  tex_width_  = 0;
  tex_height_ = 0;
  ink_x_      = 0;
  ink_y_      = 0;
  ink_width_  = 0;
  ink_height_ = 0;
}

void Extensions::gl_query(bool use_es, int version)
{
  g_log(GL::log_domain, G_LOG_LEVEL_INFO, "OpenGL version: %s, GLSL version: %s",
        glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

  if (version < ((use_es) ? 0x0300 : 0x0302))
  {
    g_log(GL::log_domain, G_LOG_LEVEL_WARNING,
          "At least OpenGL 3.2 or OpenGL ES 3.0 is required");
  }
  debug_output = (!use_es && version >= 0x0403)
      || epoxy_has_gl_extension("GL_ARB_debug_output");

  vertex_type_2_10_10_10_rev = (version >= ((use_es) ? 0x0300 : 0x0303))
      || epoxy_has_gl_extension("GL_ARB_vertex_type_2_10_10_10_rev");

  texture_filter_anisotropic = (!use_es && version >= 0x0406)
      || epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic");

  if (texture_filter_anisotropic)
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
  else
    max_anisotropy = 1.;
}

Scene::Scene(BaseObjectType* obj)
:
  Gtk::GLArea{obj}
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
    if (auto guard = scoped_make_current())
      gl_update_framebuffer();

    queue_static_draw();
  }
}

int Scene::get_multisample() const
{
  return aa_samples_;
}

void Scene::set_show_focus(bool show_focus)
{
  if (show_focus != show_focus_)
  {
    show_focus_ = show_focus;

    if (has_visible_focus())
      queue_static_draw();
  }
}

bool Scene::get_show_focus() const
{
  return show_focus_;
}

Scene::ContextGuard Scene::scoped_make_current()
{
  const auto context = get_context();

  if (context)
    context->make_current();

  return ContextGuard{!!context};
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

int Scene::get_viewport_width() const
{
  return std::max(1, get_allocated_width() * get_scale_factor());
}

int Scene::get_viewport_height() const
{
  return std::max(1, get_allocated_height() * get_scale_factor());
}

LayoutTexture* Scene::create_layout_texture()
{
  // For now, layout textures may only be created at initialization time.
  g_return_val_if_fail(ui_buffer_ == 0, nullptr);

  std::unique_ptr<LayoutTexture> layout {new LayoutTexture{}};

  layout->array_offset_ =
    LAYOUTS_ARRAY_OFFSET + LayoutTexture::VERTEX_COUNT * ui_layouts_.size();

  ui_layouts_.push_back(std::move(layout));

  return ui_layouts_.back().get();
}

void Scene::gl_update_ui()
{
  focus_drawable_ = false;

  gl_update_layouts();
  gl_reposition_layouts();

  if (!ui_vertex_array_ || !ui_buffer_)
  {
    ui_vertex_count_ = 0;

    if (!ui_vertex_array_)
    {
      glGenVertexArrays(1, &ui_vertex_array_);
      GL::Error::throw_if_fail(ui_vertex_array_ != 0);
    }
    if (!ui_buffer_)
    {
      glGenBuffers(1, &ui_buffer_);
      GL::Error::throw_if_fail(ui_buffer_ != 0);
    }

    glBindVertexArray(ui_vertex_array_);

    gl_update_ui_buffer();

    glVertexAttribPointer(ATTRIB_POSITION,
                          GL::attrib_size<decltype(UIVertex::position)>,
                          GL::attrib_type<decltype(UIVertex::position)>,
                          GL_FALSE, sizeof(UIVertex),
                          GL::buffer_offset(offsetof(UIVertex, position)));
    glVertexAttribPointer(ATTRIB_TEXCOORD,
                          GL::attrib_size<decltype(UIVertex::texcoord)>,
                          GL::attrib_type<decltype(UIVertex::texcoord)>,
                          GL_FALSE, sizeof(UIVertex),
                          GL::buffer_offset(offsetof(UIVertex, texcoord)));
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glEnableVertexAttribArray(ATTRIB_TEXCOORD);

    glBindVertexArray(0);
  }
  else
    gl_update_ui_buffer();

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Scene::gl_initialize()
{
  gl_create_label_shader();
  gl_create_focus_shader();

  gl_update_framebuffer();
  glViewport(0, 0, get_viewport_width(), get_viewport_height());
  gl_update_projection();
  gl_update_color();

  if (label_shader_)
  {
    label_shader_.use();
    glUniform1i(label_uf_texture_, 0);
    GL::ShaderProgram::unuse();
  }
}

void Scene::gl_cleanup()
{
  ui_vertex_count_ = 0;
  focus_drawable_  = false;

  if (ui_vertex_array_)
  {
    glDeleteVertexArrays(1, &ui_vertex_array_);
    ui_vertex_array_ = 0;
  }
  if (ui_buffer_)
  {
    glDeleteBuffers(1, &ui_buffer_);
    ui_buffer_ = 0;
  }

  for (const auto& layout : ui_layouts_)
    layout->gl_delete();

  gl_delete_framebuffer();

  label_uf_color_   = -1;
  label_uf_texture_ = -1;
  focus_uf_color_   = -1;

  label_shader_.reset();
  focus_shader_.reset();
}

/*
 * This method is invoked to sanitize the GL state after
 * an exception occured during execution of gl_render().
 */
void Scene::gl_reset_state()
{
  glBindVertexArray(0);

  glDisableVertexAttribArray(ATTRIB_TEXCOORD);
  glDisableVertexAttribArray(ATTRIB_POSITION);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDisable(GL_BLEND);

  GL::ShaderProgram::unuse();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

/*
 * Note that this method is pure virtual because at least a glClear() command
 * needs to be issued by an overriding method to make the whole thing work.
 */
int Scene::gl_render()
{
  int triangle_count = 0;

  if (ui_vertex_array_)
  {
    const auto first = std::find_if(ui_layouts_.cbegin(), ui_layouts_.cend(),
                                    std::mem_fn(&LayoutTexture::drawable));
    if (first != ui_layouts_.cend()
        || (show_focus_ && focus_drawable_ && has_visible_focus()))
    {
      glBindVertexArray(ui_vertex_array_);

      gl_render_focus();
      triangle_count = gl_render_layouts(first);

      GL::ShaderProgram::unuse();
      glBindVertexArray(0);
    }
  }
  return triangle_count;
}

void Scene::gl_update_projection()
{}

void Scene::gl_update_color()
{
#if 0
  const auto style = get_style_context();
  const auto state = style->get_state();

  const Gdk::RGBA color = style->get_color(state);

  focus_color_ = {static_cast<float>(color.get_red()),
                  static_cast<float>(color.get_green()),
                  static_cast<float>(color.get_blue()),
                  1.f};
#endif
  glClearColor(0., 0., 0., 0.);
}

void Scene::on_realize()
{
  texture_context_.reset();

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  Gtk::GLArea::on_realize();

  if (auto guard = scoped_make_current())
  {
    const auto context = get_context();
    const bool use_es = context->get_use_es();

    int major = 0, minor = 0;
    context->get_version(major, minor);

    gl_extensions_.gl_query(use_es, (major << 8) | minor);

    if (gl_ext()->debug_output)
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

  texture_context_.reset();
}

void Scene::on_size_allocate(Gtk::Allocation& allocation)
{
  Gtk::GLArea::on_size_allocate(allocation);

  if (auto guard = scoped_make_current())
  {
    gl_update_framebuffer();
    glViewport(0, 0, get_viewport_width(), get_viewport_height());
    gl_update_projection();
    gl_update_ui();
  }
}

/*
 * Instead of hooking into Gtk::GLArea::on_render(), completely replace
 * the drawing logic of Gtk::GLArea to bypass it. That way, we can create
 * our own frame buffer configuration with multi-sample render buffers,
 * without interference from Gtk::GLArea's hard-coded setup.
 */
bool Scene::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (auto guard = scoped_make_current())
  {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer_);

    unsigned int triangle_count = 0;
    try
    {
      triangle_count = gl_render();
    }
    catch (...)
    {
      gl_reset_state();
      throw;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_buffer_);

    gdk_cairo_draw_from_gl(cr->cobj(), gtk_widget_get_window(Gtk::Widget::gobj()),
                           render_buffers_[COLOR], GL_RENDERBUFFER,
                           get_scale_factor(), 0, 0,
                           get_viewport_width(), get_viewport_height());
    ++frame_counter_;
    triangle_counter_ += triangle_count;
  }
  return true;
}

void Scene::on_style_updated()
{
  texture_context_.reset();

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  Gtk::GLArea::on_style_updated();

  if (auto guard = scoped_make_current())
  {
    gl_update_color();
    gl_update_ui();
  }
}

void Scene::on_state_changed(Gtk::StateType previous_state)
{
  Gtk::GLArea::on_state_changed(previous_state);

  if (auto guard = scoped_make_current())
  {
    gl_update_color();
    gl_update_ui();
  }
}

void Scene::on_direction_changed(Gtk::TextDirection previous_direction)
{
  texture_context_.reset();

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  Gtk::GLArea::on_direction_changed(previous_direction);

  if (auto guard = scoped_make_current())
    gl_update_ui();
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

void Scene::gl_reposition_layouts()
{}

void Scene::gl_create_label_shader()
{
  GL::ShaderProgram program;

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/textlabel.vert"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/textlabel.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_TEXCOORD, "texcoord");
  program.link();

  label_uf_color_   = program.get_uniform_location("textColor");
  label_uf_texture_ = program.get_uniform_location("labelTexture");

  label_shader_ = std::move(program);
}

void Scene::gl_create_focus_shader()
{
  GL::ShaderProgram program;

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/focusrect.vert"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/focusrect.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.link();

  focus_uf_color_ = program.get_uniform_location("focusColor");

  focus_shader_ = std::move(program);
}

void Scene::gl_update_framebuffer()
{
  gl_delete_framebuffer();

  glGenRenderbuffers(2, render_buffers_);
  GL::Error::throw_if_fail(render_buffers_[COLOR] != 0 && render_buffers_[DEPTH] != 0);

  glGenFramebuffers(1, &frame_buffer_);
  GL::Error::throw_if_fail(frame_buffer_ != 0);

  const int view_width  = get_viewport_width();
  const int view_height = get_viewport_width();
  const int samples = std::min(aa_samples_, max_aa_samples_);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[COLOR]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB8,
                                   view_width, view_height);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[DEPTH]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24,
                                   view_width, view_height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer_);

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
  if (render_buffers_[COLOR] || render_buffers_[DEPTH])
  {
    glDeleteRenderbuffers(2, render_buffers_);
    render_buffers_[COLOR] = 0;
    render_buffers_[DEPTH] = 0;
  }
}

void Scene::gl_update_layouts()
{
  for (const auto& layout : ui_layouts_)
  {
    if (!layout->content_.empty())
    {
      if (!layout->tex_name_ || layout->need_update_)
        layout->gl_set_layout(create_texture_pango_layout(layout->content_));
    }
    else
    {
      layout->gl_delete();

      layout->log_width_  = 0;
      layout->log_height_ = 0;
    }
    layout->need_update_ = false;
  }
}

void Scene::gl_update_ui_buffer()
{
  g_return_if_fail(ui_buffer_ != 0);

  glBindBuffer(GL_ARRAY_BUFFER, ui_buffer_);

  const unsigned int vertex_count =
    FOCUS_VERTEX_COUNT + LayoutTexture::VERTEX_COUNT * ui_layouts_.size();

  if (vertex_count != ui_vertex_count_)
  {
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(UIVertex),
                 nullptr, GL_DYNAMIC_DRAW);
    ui_vertex_count_ = vertex_count;
  }

  if (GL::ScopedMapBuffer buffer {GL_ARRAY_BUFFER,
                                  0, vertex_count * sizeof(UIVertex),
                                  GL_MAP_WRITE_BIT
                                  | GL_MAP_INVALIDATE_RANGE_BIT
                                  | GL_MAP_INVALIDATE_BUFFER_BIT})
  {
    auto *const vertices = buffer.get<UIVertex>();

    gl_build_focus(vertices);
    gl_build_layouts(vertices);
  }
}

void Scene::gl_build_focus(volatile UIVertex* vertices)
{
  g_return_if_fail(FOCUS_ARRAY_OFFSET + FOCUS_VERTEX_COUNT <= ui_vertex_count_);

  bool interior_focus = false;
  int  focus_padding  = 0;

  get_style_property("interior_focus", interior_focus);
  get_style_property("focus_padding",  focus_padding);

  const int view_width  = get_viewport_width();
  const int view_height = get_viewport_height();

  focus_drawable_ = (interior_focus
                     && view_width  > 2 * focus_padding
                     && view_height > 2 * focus_padding);

  generate_focus_rect(view_width, view_height, focus_padding,
                      vertices + FOCUS_ARRAY_OFFSET);
}

/*
 * Generate vertices and texture coordinates for the text layouts.
 */
void Scene::gl_build_layouts(volatile UIVertex* vertices)
{
  const int view_width  = get_viewport_width();
  const int view_height = get_viewport_height();

  for (const auto& layout : ui_layouts_)
  {
    const int width  = layout->ink_width_  + 1;
    const int height = layout->ink_height_ + 1;

    const float tex_width  = layout->tex_width_;
    const float tex_height = layout->tex_height_;

    const float s0 = -1. / tex_width;
    const float t0 = height / tex_height;
    const float s1 = (width - 1) / tex_width;
    const float t1 = 0.;

    const int view_x = layout->window_x_ + layout->ink_x_;
    const int view_y = layout->window_y_ + layout->ink_y_;

    const float x0 = static_cast<float>(2 * view_x - view_width)  / view_width;
    const float y0 = static_cast<float>(2 * view_y - view_height) / view_height;
    const float x1 = static_cast<float>(2 * (view_x + width)  - view_width)  / view_width;
    const float y1 = static_cast<float>(2 * (view_y + height) - view_height) / view_height;

    g_return_if_fail(layout->array_offset_ + LayoutTexture::VERTEX_COUNT <= ui_vertex_count_);

    volatile UIVertex *const geometry = vertices + layout->array_offset_;

    geometry[0].set(x0, y0, s0, t0);
    geometry[1].set(x1, y0, s1, t0);
    geometry[2].set(x0, y1, s0, t1);
    geometry[3].set(x1, y1, s1, t1);
  }
}

void Scene::gl_render_focus()
{
  if (show_focus_ && focus_drawable_ && focus_shader_ && has_visible_focus())
  {
    focus_shader_.use();
    glUniform4fv(focus_uf_color_, 1, &focus_color_[0]);

    glDrawArrays(GL_LINE_LOOP, FOCUS_ARRAY_OFFSET, FOCUS_VERTEX_COUNT);
  }
}

int Scene::gl_render_layouts(LayoutVector::const_iterator first)
{
  int triangle_count = 0;

  if (first != ui_layouts_.cend() && label_shader_)
  {
    label_shader_.use();

    // The source function is identity because we blend in an intensity
    // texture. That is, the color channels are premultiplied by alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    triangle_count = gl_render_layout_arrays(first);

    glDisable(GL_BLEND);
  }

  return triangle_count;
}

/*
 * Render text layouts and shadow in a single pass.  At least the first
 * element in the non-empty sequence [first, ui_layouts_.cend()) must be
 * enabled and ready for drawing.
 */
int Scene::gl_render_layout_arrays(LayoutVector::const_iterator first)
{
  const LayoutTexture* layout = first->get();

  glBindTexture(GL_TEXTURE_2D, layout->tex_name_);
  glUniform4fv(label_uf_color_, 1, &layout->color()[0]);

  glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

  int triangle_count = LayoutTexture::TRIANGLE_COUNT;

  while (++first != ui_layouts_.cend())
  {
    layout = first->get();

    if (layout->drawable())
    {
      glBindTexture(GL_TEXTURE_2D, layout->tex_name_);
      glUniform4fv(label_uf_color_, 1, &layout->color()[0]);

      glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

      triangle_count += LayoutTexture::TRIANGLE_COUNT;
    }
  }
  return triangle_count;
}

Glib::RefPtr<Pango::Layout> Scene::create_texture_pango_layout(const Glib::ustring& text)
{
  if (!texture_context_)
  {
    // Create a dummy cairo context with surface type and transformation
    // matching what we are going to use at draw time.
    const auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_A8, 1, 1);
    const auto cairo = Cairo::Context::create(surface);

    auto context = create_pango_context();
    context->update_from_cairo_context(cairo);

    texture_context_ = std::move(context);
  }
  const auto layout = Pango::Layout::create(texture_context_);
  layout->set_text(text);

  return layout;
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
