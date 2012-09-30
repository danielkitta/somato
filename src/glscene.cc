/*
 * Copyright (c) 2004-2012  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "glscene.h"
#include "glsceneprivate.h"
#include "appdata.h"
#include "glutils.h"
#include "mathutils.h"

#include <glib.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <gdk/gdkgl.h>
#include <gtk/gtkgl.h>
#include <gdkmm.h>
#include <gtkmm/style.h>

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
  ATTRIB_TEXCOORD = 2
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
                         const GLchar* message, GLvoid*)
{
  g_log("OpenGL", G_LOG_LEVEL_DEBUG, "%s", message);
}
} // extern "C"

/*
 * Generate vertices for drawing the focus indicator of the GL widget.
 */
static
void generate_focus_rect(int width, int height, int padding,
                         GL::UIVertex* geometry)
{
  g_return_if_fail(width > 0 && height > 0);

  const float x0 = float(2 * padding + 1 - width)  / width;
  const float y0 = float(2 * padding + 1 - height) / height;
  const float x1 = float(width  - 2 * padding - 1) / width;
  const float y1 = float(height - 2 * padding - 1) / height;

  geometry[0].set_vertex(x0, y0);
  geometry[0].set_texcoord(0.0, 0.0);

  geometry[1].set_vertex(x1, y0);
  geometry[1].set_texcoord(1.0, 0.0);

  geometry[2].set_vertex(x1, y1);
  geometry[2].set_texcoord(1.0, 1.0);

  geometry[3].set_vertex(x0, y1);
  geometry[3].set_texcoord(0.0, 1.0);
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
static inline
int aligned_stride(int width)
{
  return cairo_format_stride_for_width(CAIRO_FORMAT_A8, (width + 7) & ~7);
}

} // anonymous namespace

namespace GL
{

Extensions::~Extensions()
{}

void Extensions::query()
{
  have_debug_output = false;
  have_swap_control = false;

  DebugMessageControl  = nullptr;
  DebugMessageCallback = nullptr;

#if defined(GDK_WINDOWING_X11)
  SwapIntervalSGI = nullptr;
#elif defined(GDK_WINDOWING_WIN32)
  SwapIntervalEXT = nullptr;
#endif

  if (GL::have_gl_extension("GL_ARB_debug_output"))
  {
    if (GL::get_proc_address(DebugMessageControl,  "glDebugMessageControlARB") &&
        GL::get_proc_address(DebugMessageCallback, "glDebugMessageCallbackARB"))
      have_debug_output = true;
  }

#if defined(GDK_WINDOWING_X11)
  if (GL::have_glx_extension("GLX_SGI_swap_control"))
  {
    if (GL::get_proc_address(SwapIntervalSGI, "glXSwapIntervalSGI"))
      have_swap_control = true;
  }
#elif defined(GDK_WINDOWING_WIN32)
  if (GL::have_wgl_extension("WGL_EXT_swap_control"))
  {
    if (GL::get_proc_address(SwapIntervalEXT, "wglSwapIntervalEXT"))
      have_swap_control = true;
  }
#endif
}

LayoutTexture::LayoutTexture()
:
  color_        {1.0, 1.0, 1.0, 1.0},
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

/*
 * Prepare a Pango context for the creation of Pango layouts for use with
 * LayoutTexture::gl_set_layout().
 *
 * This creates a dummy cairo context with surface type and transformation
 * matching what we are going to use at draw time, and updates the Pango
 * context accordingly.  The pangocairo documentation warns that not doing
 * so might result in wrong measurements due to possible differences in
 * hinting and such.
 */
// static
void LayoutTexture::prepare_pango_context(const Glib::RefPtr<Pango::Context>& context)
{
  cairo_surface_t *const surface = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
  cairo_t *const cairo_context = cairo_create(surface);

  cairo_surface_destroy(surface); // drop reference

  pango_cairo_update_context(cairo_context, context->gobj());

  cairo_destroy(cairo_context);
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
  // the target surface and transformation in prepare_pango_context().
  layout->get_pixel_extents(ink, logical);

  // Make sure the extents are within reasonable boundaries.
  g_return_if_fail(ink.get_width() < 4095 && ink.get_height() < 4095);

  // Pad up the margins to account for measurement inaccuracies.
  enum { PADDING = 1 };

  const int ink_width  = Math::max(0, ink.get_width())  + 2 * PADDING;
  const int ink_height = Math::max(0, ink.get_height()) + 2 * PADDING;
  const int img_width  = aligned_stride(ink_width);

  std::vector<GLubyte> tex_image (ink_height * img_width);

  // Create a Cairo surface to draw the layout directly into the texture image.
  // Note that the image will be upside-down from the point of view of OpenGL,
  // thus the texture coordinates need to be adjusted accordingly.
  {
    cairo_surface_t *const surface = cairo_image_surface_create_for_data(
        &tex_image[0], CAIRO_FORMAT_A8, img_width, ink_height, img_width * sizeof(GLubyte));

    cairo_t *const context = cairo_create(surface);

    cairo_surface_destroy(surface); // drop reference

    cairo_move_to(context, PADDING - ink.get_x(), PADDING - ink.get_y());
    pango_cairo_show_layout(context, layout->gobj());

    cairo_destroy(context);
  }

  if (!tex_name_)
  {
    tex_width_  = 0;
    tex_height_ = 0;

    glGenTextures(1, &tex_name_);
    GL::Error::throw_if_fail(tex_name_ != 0);
  }
  glBindTexture(GL_TEXTURE_RECTANGLE, tex_name_);

  if (tex_width_ == 0)
  {
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8, img_width, ink_height,
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

Scene::Scene()
:
  focus_color_        {1.0, 1.0, 1.0, 1.0},
  gl_drawable_        {nullptr},
  gl_context_         {nullptr},
  label_uf_color_     {-1},
  label_uf_texture_   {-1},
  focus_uf_color_     {-1},
  aa_samples_         {0},
  max_aa_samples_     {0},
  render_buffers_     {0, 0},
  frame_buffer_       {0},
  ui_vertex_count_    {0},
  ui_vertex_array_    {0},
  ui_buffer_          {0},
  frame_counter_      {0},
  triangle_counter_   {0},
  exclusive_context_  {false},
  has_back_buffer_    {false},
  use_back_buffer_    {true},
  enable_vsync_       {true},
  vsync_enabled_      {false},
  show_focus_         {true},
  focus_drawable_     {false}
{
  set_double_buffered(false);

  add_events(Gdk::EXPOSURE_MASK | Gdk::FOCUS_CHANGE_MASK | Gdk::VISIBILITY_NOTIFY_MASK);

  // GtkGLExt itself connects to the realize and unrealize signals in order to
  // manage the GL window.  The problem is that by the time the on_unrealize()
  // default signal handler is invoked, the GL drawable is no longer available
  // and we are thus unable to do our cleanup stage.  Fortunately it turns out
  // that connecting to the signals instead of overriding the default handlers
  // works around the problem.

  signal_realize()  .connect(sigc::mem_fun(*this, &Scene::on_signal_realize),   true);  // after
  signal_unrealize().connect(sigc::mem_fun(*this, &Scene::on_signal_unrealize), false); // before
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

void Scene::set_exclusive_context(bool exclusive_context)
{
  g_return_if_fail(gl_drawable_ == nullptr);

  exclusive_context_ = exclusive_context;
}

bool Scene::get_exclusive_context() const
{
  return exclusive_context_;
}

void Scene::set_use_back_buffer(bool use_back_buffer)
{
  if (use_back_buffer != use_back_buffer_)
  {
    use_back_buffer_ = use_back_buffer;

    if (has_back_buffer_ && is_realized())
    {
      ScopeContext context {*this};

      glDrawBuffer((use_back_buffer_) ? GL_BACK : GL_FRONT);
    }
  }
}

bool Scene::get_use_back_buffer() const
{
  return use_back_buffer_;
}

void Scene::set_enable_vsync(bool enable_vsync)
{
  if (enable_vsync != enable_vsync_)
  {
    enable_vsync_ = enable_vsync;

    if (is_realized())
    {
      ScopeContext context {*this};

      gl_update_vsync_state();
    }
  }
}

bool Scene::get_enable_vsync() const
{
  return enable_vsync_;
}

bool Scene::vsync_enabled() const
{
  return vsync_enabled_;
}

void Scene::set_multisample(int n_samples)
{
  const int samples_set = Math::min(aa_samples_, max_aa_samples_);
  aa_samples_ = n_samples;

  if (n_samples != samples_set)
  {
    if (is_realized())
    {
      ScopeContext context {*this};

      gl_update_framebuffer();
    }

    if (is_drawable())
      queue_draw();
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

    if (has_focus() && is_drawable())
      queue_draw();
  }
}

bool Scene::get_show_focus() const
{
  return show_focus_;
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

    glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          GL::buffer_offset(offsetof(UIVertex, vertex)));
    glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          GL::buffer_offset(offsetof(UIVertex, texcoord)));
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glEnableVertexAttribArray(ATTRIB_TEXCOORD);

    glBindVertexArray(0);
  }
  else
    gl_update_ui_buffer();

  glBindBuffer(GL_ARRAY_BUFFER, 0);
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

  void *const vertex_data =
    glMapBufferRange(GL_ARRAY_BUFFER,
                     0, vertex_count * sizeof(UIVertex),
                     GL_MAP_WRITE_BIT
                     | GL_MAP_INVALIDATE_RANGE_BIT
                     | GL_MAP_INVALIDATE_BUFFER_BIT);
  if (vertex_data)
  {
    UIVertex *const vertices = static_cast<UIVertex*>(vertex_data);

    gl_build_focus(vertices);
    gl_build_layouts(vertices);

    if (!glUnmapBuffer(GL_ARRAY_BUFFER))
      g_warning("glUnmapBuffer(GL_ARRAY_BUFFER) failed");
  }
  else
    g_warning("glMapBufferRange(GL_ARRAY_BUFFER) failed");
}

void Scene::gl_swap_buffers()
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_buffer_);

  const int width  = Math::max(1, get_width());
  const int height = Math::max(1, get_height());

  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);

  g_return_if_fail(gl_drawable_ != nullptr);

  if (has_back_buffer_ && use_back_buffer_)
    gdk_gl_drawable_swap_buffers(gl_drawable_);

  gdk_gl_drawable_wait_gl(gl_drawable_);
}

void Scene::gl_initialize()
{
  gl_create_label_shader();
  gl_create_focus_shader();

  gl_update_framebuffer();
  gl_update_viewport();
  gl_update_projection();
  gl_update_color();

  if (label_shader_)
  {
    label_shader_.use();
    glUniform1i(label_uf_texture_, 0);
    GL::ShaderProgram::unuse();
  }
}

void Scene::gl_create_label_shader()
{
  GL::ShaderProgram program;

  program.attach(GL::ShaderObject{GL_VERTEX_SHADER,
                                  Util::locate_shader_file("textlabel.vert")});
  program.attach(GL::ShaderObject{GL_FRAGMENT_SHADER,
                                  Util::locate_shader_file("textlabel.frag")});

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

  program.attach(GL::ShaderObject{GL_VERTEX_SHADER,
                                  Util::locate_shader_file("focusrect.vert")});
  program.attach(GL::ShaderObject{GL_FRAGMENT_SHADER,
                                  Util::locate_shader_file("focusrect.frag")});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.link();

  focus_uf_color_ = program.get_uniform_location("focusColor");

  focus_shader_ = std::move(program);
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
        || (show_focus_ && focus_drawable_ && has_focus()))
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

  glBindTexture(GL_TEXTURE_RECTANGLE, layout->tex_name_);
  glUniform4fv(label_uf_color_, 1, &layout->color()[0]);

  glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

  int triangle_count = LayoutTexture::TRIANGLE_COUNT;

  while (++first != ui_layouts_.cend())
  {
    layout = first->get();

    if (layout->drawable())
    {
      glBindTexture(GL_TEXTURE_RECTANGLE, layout->tex_name_);
      glUniform4fv(label_uf_color_, 1, &layout->color()[0]);

      glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

      triangle_count += LayoutTexture::TRIANGLE_COUNT;
    }
  }
  return triangle_count;
}

void Scene::gl_render_focus()
{
  if (show_focus_ && focus_drawable_ && focus_shader_ && has_focus())
  {
    focus_shader_.use();
    glUniform4fv(focus_uf_color_, 1, &focus_color_[0]);

    glDrawArrays(GL_LINE_LOOP, FOCUS_ARRAY_OFFSET, FOCUS_VERTEX_COUNT);
  }
}

/*
 * Generate vertices and texture coordinates for the text layouts.
 */
void Scene::gl_build_layouts(UIVertex* vertices)
{
  const int win_width  = Math::max(1, get_width());
  const int win_height = Math::max(1, get_height());

  for (const auto& layout : ui_layouts_)
  {
    const int width  = layout->ink_width_  + 1;
    const int height = layout->ink_height_ + 1;

    const float s0 = -1.0;
    const float t0 = height;
    const float s1 = width - 1;
    const float t1 = 0.0;

    const int win_x = layout->window_x_ + layout->ink_x_;
    const int win_y = layout->window_y_ + layout->ink_y_;

    const float x0 = float(2 * win_x - win_width)  / win_width;
    const float y0 = float(2 * win_y - win_height) / win_height;
    const float x1 = float(2 * (win_x + width)  - win_width)  / win_width;
    const float y1 = float(2 * (win_y + height) - win_height) / win_height;

    g_return_if_fail(layout->array_offset_ + LayoutTexture::VERTEX_COUNT <= ui_vertex_count_);

    UIVertex *const geometry = vertices + layout->array_offset_;

    geometry[0].set_vertex(x0, y0);
    geometry[0].set_texcoord(s0, t0);

    geometry[1].set_vertex(x1, y0);
    geometry[1].set_texcoord(s1, t0);

    geometry[2].set_vertex(x0, y1);
    geometry[2].set_texcoord(s0, t1);

    geometry[3].set_vertex(x1, y1);
    geometry[3].set_texcoord(s1, t1);
  }
}

void Scene::gl_build_focus(UIVertex* vertices)
{
  g_return_if_fail(FOCUS_ARRAY_OFFSET + FOCUS_VERTEX_COUNT <= ui_vertex_count_);

  bool interior_focus = false;
  int  focus_padding  = 0;

  get_style_property("interior_focus", interior_focus);
  get_style_property("focus_padding",  focus_padding);

  const int width  = get_width();
  const int height = get_height();

  focus_drawable_ = (interior_focus
                     && width  > 2 * focus_padding
                     && height > 2 * focus_padding);

  generate_focus_rect(Math::max(1, width), Math::max(1, height),
                      focus_padding, vertices + FOCUS_ARRAY_OFFSET);
}

void Scene::gl_update_viewport()
{
  const int width  = Math::max(1, get_width());
  const int height = Math::max(1, get_height());

  glViewport(0, 0, width, height);
}

void Scene::gl_update_projection()
{}

void Scene::gl_update_color()
{
  const auto style = get_style();
  const auto state = get_state();

  const Gdk::Color fg = style->get_fg(state);
  const Gdk::Color bg = style->get_bg(state);

  focus_color_ = Math::Vector4(fg.get_red(),
                               fg.get_green(),
                               fg.get_blue(),
                               0xFFFF) * (1.0f / 0xFFFF);

  const float red   = float(bg.get_red())   * (1.0f / 0xFFFF);
  const float green = float(bg.get_green()) * (1.0f / 0xFFFF);
  const float blue  = float(bg.get_blue())  * (1.0f / 0xFFFF);

  glClearColor(red, green, blue, 0.0);
}

void Scene::gl_update_framebuffer()
{
  gl_delete_framebuffer();

  glGenRenderbuffers(2, render_buffers_);
  GL::Error::throw_if_fail(render_buffers_[COLOR] != 0 && render_buffers_[DEPTH] != 0);

  glGenFramebuffers(1, &frame_buffer_);
  GL::Error::throw_if_fail(frame_buffer_ != 0);

  const int width   = Math::max(1, get_width());
  const int height  = Math::max(1, get_height());
  const int samples = Math::min(aa_samples_, max_aa_samples_);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[COLOR]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB, width, height);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[DEPTH]);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);

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

void Scene::gl_update_vsync_state()
{
  if (has_back_buffer_ && gl_ext()->have_swap_control)
  {
#if defined(GDK_WINDOWING_X11)

    // Once enabled, the GLX_SGI_swap_control extension does not provide a way
    // to turn off synchronization with the vertical retrace.  In fact even the
    // explicit enable is a stretch of the API specification, as vertical sync
    // is supposed to be enabled by default.

    if (enable_vsync_ && gl_ext()->SwapIntervalSGI(1) == 0)
      vsync_enabled_ = true;

#elif defined(GDK_WINDOWING_WIN32)

    if (gl_ext()->SwapIntervalEXT((enable_vsync_) ? 1 : 0) != 0)
      vsync_enabled_ = enable_vsync_;

#endif /* defined(GDK_WINDOWING_WIN32) */
  }
  else
  {
    // It is necessary to assume vertical sync to be disabled whenever we
    // cannot know for sure.  That's because if the synchronization really
    // doesn't take place, timeouts must be used instead of idle handlers
    // to schedule frames, in order to avoid exhausting either CPU or GPU
    // by drawing, like, 10000 frames per second.
    vsync_enabled_ = false;
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

Glib::RefPtr<Pango::Layout> Scene::create_texture_pango_layout(const Glib::ustring& text)
{
  if (!texture_context_)
  {
    auto context = create_pango_context();
    LayoutTexture::prepare_pango_context(context);

    swap(texture_context_, context);
  }

  const auto layout = Pango::Layout::create(texture_context_);
  layout->set_text(text);

  return layout;
}

GL::Extensions* Scene::gl_query_extensions()
{
  return new GL::Extensions{};
}

void Scene::gl_reposition_layouts()
{}

/*
 * Set up the widget for GL drawing as soon as a screen is available.
 */
void Scene::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen)
{
  Gtk::DrawingArea::on_screen_changed(previous_screen);

  if (has_screen() && !is_realized())
    GL::configure_widget(*this, GDK_GL_MODE_RGBA | GDK_GL_MODE_DOUBLE);
}

void Scene::on_size_allocate(Gtk::Allocation& allocation)
{
  Gtk::DrawingArea::on_size_allocate(allocation);

  if (is_realized())
  {
    ScopeContext context {*this};

    gl_update_framebuffer();
    gl_update_viewport();
    gl_update_projection();
    gl_update_ui();
  }
}

void Scene::on_state_changed(Gtk::StateType previous_state)
{
  if (is_realized())
  {
    ScopeContext context {*this};

    gl_update_color();
    gl_update_ui();
  }

  Gtk::DrawingArea::on_state_changed(previous_state);
}

void Scene::on_style_changed(const Glib::RefPtr<Gtk::Style>& previous_style)
{
  // Avoid both reset() and clear()... it's all Murray's fault :-P
  Glib::RefPtr<Pango::Context>().swap(texture_context_);

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  if (is_realized())
  {
    ScopeContext context {*this};

    gl_update_color();
    gl_update_ui();
  }

  Gtk::DrawingArea::on_style_changed(previous_style);
}

void Scene::on_direction_changed(Gtk::TextDirection previous_direction)
{
  Glib::RefPtr<Pango::Context>().swap(texture_context_);

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  if (is_realized())
  {
    ScopeContext context {*this};

    gl_update_ui();
  }

  Gtk::DrawingArea::on_direction_changed(previous_direction);
}

bool Scene::on_expose_event(GdkEventExpose*)
{
  if (is_drawable())
  {
    ScopeContext context {*this};

    unsigned int triangle_count = 0;

    try
    {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer_);
      triangle_count = gl_render();
    }
    catch (...)
    {
      gl_reset_state();
      throw;
    }

    gl_swap_buffers();

    // Rely on quiet modulo overflow of unsigned integer arithmetic.
    ++frame_counter_;
    triangle_counter_ += triangle_count;
  }

  return true;
}

bool Scene::on_visibility_notify_event(GdkEventVisibility* event)
{
  // Explicitely invalidating the window at this point helps to avoid some
  // of the situations in which garbage is displayed due to a missing update.
  // This is obviously a hack.  A proper solution would require locating the
  // actual source of the misbehavior.

  if (event->state != GDK_VISIBILITY_FULLY_OBSCURED && is_drawable())
    queue_draw();

  return Gtk::DrawingArea::on_visibility_notify_event(event);
}

void Scene::on_signal_realize()
{
  g_return_if_fail(gl_drawable_ == nullptr);
  g_return_if_fail(gl_context_ == nullptr);

  Glib::RefPtr<Pango::Context>().swap(texture_context_);

  for (const auto& layout : ui_layouts_)
    layout->invalidate();

  gl_drawable_ = gtk_widget_get_gl_drawable(Gtk::Widget::gobj());
  gl_context_  = GL::create_context(gl_drawable_);

  has_back_buffer_ = (gdk_gl_drawable_is_double_buffered(gl_drawable_) != FALSE);

  if (exclusive_context_)
  {
    if (!gdk_gl_drawable_gl_begin(gl_drawable_, gl_context_))
      throw GL::Error("rendering context could not be made current");
  }

  {
    ScopeContext context {*this};

    gl_extensions_.reset(gl_query_extensions());
    g_return_if_fail(gl_ext() != nullptr);

    if (GL::get_gl_version() < GL::make_version(3, 2))
      g_error("at least OpenGL 3.2 is required to run this program");

    if (gl_ext()->have_debug_output)
    {
      gl_ext()->DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
      gl_ext()->DebugMessageCallback(&gl_on_debug_message, nullptr);
    }

    max_aa_samples_ = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_aa_samples_);

    gl_update_vsync_state();

    if (has_back_buffer_ && !use_back_buffer_)
      glDrawBuffer(GL_FRONT);

    gl_initialize();
  }
}

void Scene::on_signal_unrealize()
{
  if (gl_drawable_ && gl_context_)
  {
    {
      ScopeContext context {*this};

      gl_cleanup();
      gl_extensions_.reset();
    }

    if (exclusive_context_)
      gdk_gl_drawable_gl_end(gl_drawable_);

    GL::destroy_context(gl_context_);

    gl_drawable_ = nullptr;
    gl_context_  = nullptr;

    vsync_enabled_   = false;
    has_back_buffer_ = false;
  }

  Glib::RefPtr<Pango::Context>().swap(texture_context_);
}

// static
void Scene::ScopeContext::begin_(Scene& scene)
{
  if (!scene.exclusive_context_)
  {
    if (!gdk_gl_drawable_gl_begin(scene.gl_drawable_, scene.gl_context_))
      throw GL::Error("rendering context could not be made current");
  }
}

// static
void Scene::ScopeContext::end_(Scene& scene)
{
  if (!scene.exclusive_context_)
    gdk_gl_drawable_gl_end(scene.gl_drawable_);
}

} // namespace GL
