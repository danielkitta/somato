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

#include "glscene.h"
#include "glsceneprivate.h"
#include "appdata.h"
#include "array.h"
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
  FOCUS_ARRAY_OFFSET = 0, // offset into geometry arrays
  FOCUS_VERTEX_COUNT = 4
};

enum
{
  ATTRIB_POSITION = 0,
  ATTRIB_TEXCOORD = 2
};

/*
 * Generate vertices for drawing the focus indicator of the GL widget.  This
 * function assumes an orthographic projection that establishes a 1:1 mapping
 * to window coordinates.
 */
static
void generate_focus_rect(int width, int height, int padding,
                         GL::GeometryVector::iterator geometry)
{
  const float x0 = padding + 0.5f;
  const float y0 = padding + 0.5f;
  const float x1 = width  - padding - 0.5f;
  const float y1 = height - padding - 0.5f;

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
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1,5,8)
  return cairo_format_stride_for_width(CAIRO_FORMAT_A8, (width + 7) & ~7);
#else
  return (width + 7) & ~7;
#endif
}

} // anonymous namespace

namespace GL
{

Extensions::~Extensions()
{}

void Extensions::query()
{
  have_swap_control = false;

#if defined(GDK_WINDOWING_X11)
  SwapIntervalSGI = nullptr;
#elif defined(GDK_WINDOWING_WIN32)
  SwapIntervalEXT = nullptr;
#endif

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
  content_      (),
  need_update_  (false),
  array_offset_ (G_MAXINT),

  tex_name_     (0),
  tex_width_    (0),
  tex_height_   (0),

  ink_x_        (0),
  ink_y_        (0),
  ink_width_    (0),
  ink_height_   (0),

  log_width_    (0),
  log_height_   (0),

  window_x_     (0),
  window_y_     (0)
{}

LayoutTexture::~LayoutTexture()
{
  g_return_if_fail(tex_name_ == 0);
}

void LayoutTexture::set_content(const Glib::ustring& content)
{
  // One would think that the std::string implementation short-cuts the
  // comparison of strings of different length when testing for equality,
  // thus rendering the optimization here superfluous.  Sadly, it ain't so,
  // at least not with the libstdc++ that ships with GCC 4.1.2.

  if (content.raw().size() != content_.raw().size() || content.raw() != content_.raw())
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

  cairo_scale(cairo_context, 1.0, -1.0);
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
  g_return_if_fail(ink.get_width() < 4096 && ink.get_height() < 4096);

  // Pad up the margins to account for measurement inaccuracies.
  enum { PADDING = 1 };

  const int ink_width  = Math::max(0, ink.get_width())  + 2 * PADDING;
  const int ink_height = Math::max(0, ink.get_height()) + 2 * PADDING;

  int img_height = ink_height;
  int img_width  = aligned_stride(ink_width);

  // The dimensions of the new image are often identical with the previous
  // one's.  Exploit this by uploading only a sub-area of the texture image
  // which covers the union of the new ink rectangle and the previous one.
  const bool sub_image = (tex_name_ && tex_width_ == img_width && tex_height_ == img_height);

  Util::MemChunk<GLubyte> tex_image (img_height * img_width);

  std::memset(&tex_image[0], 0x00, tex_image.bytes());

  // Create a cairo surface to draw the layout directly into the texture
  // image -- upside-down and at the right position.  This rocking new
  // functionality allows us to get away without any buffer copies, yay!
  {
    cairo_surface_t *const surface = cairo_image_surface_create_for_data(
        &tex_image[0], CAIRO_FORMAT_A8, img_width, img_height, img_width * sizeof(GLubyte));

    cairo_t *const context = cairo_create(surface);

    cairo_surface_destroy(surface); // drop reference

    cairo_scale(context, 1.0, -1.0);
    cairo_move_to(context, PADDING - ink.get_x(),
                         -(PADDING + ink.get_y() + ink.get_height()));

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

  if (sub_image)
  {
    glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, img_width, img_height,
                    GL_RED, GL_UNSIGNED_BYTE, &tex_image[0]);
    GL::Error::check();
  }
  else
  {
    if (tex_width_ == 0)
    {
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8, img_width, img_height,
                 0, GL_RED, GL_UNSIGNED_BYTE, &tex_image[0]);
    GL::Error::check();

    tex_width_  = img_width;
    tex_height_ = img_height;
  }

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
  gl_drawable_        (0),
  gl_extensions_      (),
  texture_context_    (),
  ui_geometry_        (FOCUS_VERTEX_COUNT),
  ui_layouts_         (),
  label_uf_winsize_   {-1},
  label_uf_color_     {-1},
  label_uf_texture_   {-1},
  focus_uf_winsize_   {-1},
  focus_uf_color_     {-1},
  ui_buffer_          (0),
  frame_counter_      (0),
  triangle_counter_   (0),
  exclusive_context_  (false),
  has_back_buffer_    (false),
  use_back_buffer_    (true),
  enable_vsync_       (true),
  vsync_enabled_      (false),
  show_focus_         (true),
  focus_drawable_     (false)
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
{
  std::for_each(ui_layouts_.begin(), ui_layouts_.end(), Util::Delete<LayoutTexture*>());
}

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
  g_return_if_fail(gl_drawable_ == 0);

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
      ScopeContext context (*this);

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
      ScopeContext context (*this);

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

void Scene::set_show_focus(bool show_focus)
{
  if (show_focus != show_focus_)
  {
    show_focus_ = show_focus;

    if (has_focus())
    {
      if (is_realized())
      {
        ScopeContext context (*this);

        gl_update_ui();
      }

      if (is_drawable())
        queue_draw();
    }
  }
}

bool Scene::get_show_focus() const
{
  return show_focus_;
}

LayoutTexture* Scene::create_layout_texture()
{
  // For now, layout textures may only be created at initialization time.
  g_return_val_if_fail(ui_buffer_ == 0, 0);

  std::auto_ptr<LayoutTexture> layout (new LayoutTexture());

  layout->array_offset_ = ui_geometry_.size();
  ui_geometry_.resize(layout->array_offset_ + LayoutTexture::VERTEX_COUNT);

  ui_layouts_.push_back(layout.get());

  return layout.release();
}

void Scene::gl_update_ui()
{
  focus_drawable_ = false;

  gl_update_layouts();
  gl_reposition_layouts();

  gl_build_focus();
  gl_build_layouts();

  if (!ui_geometry_.empty())
  {
    if (!ui_buffer_)
    {
      glGenBuffers(1, &ui_buffer_);
      GL::Error::throw_if_fail(ui_buffer_ != 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, ui_buffer_);
    glBufferData(GL_ARRAY_BUFFER, ui_geometry_.size() * sizeof(UIVertex),
                 &ui_geometry_[0], GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GL::Error::check();
  }
}

void Scene::gl_swap_buffers()
{
  GdkGLDrawable *const drawable = static_cast<GdkGLDrawable*>(gl_drawable_);

  g_return_if_fail(drawable != 0);

  if (has_back_buffer_ && use_back_buffer_)
    gdk_gl_drawable_swap_buffers(drawable);
  else
    glFinish();
}

void Scene::gl_initialize()
{
  gl_create_label_shader();
  gl_create_focus_shader();

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

  label_uf_winsize_ = program.get_uniform_location("windowSize");
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

  focus_uf_winsize_ = program.get_uniform_location("windowSize");
  focus_uf_color_   = program.get_uniform_location("focusColor");

  focus_shader_ = std::move(program);
}

void Scene::gl_cleanup()
{
  focus_drawable_ = false;

  if (ui_buffer_)
  {
    glDeleteBuffers(1, &ui_buffer_);
    ui_buffer_ = 0;
  }

  std::for_each(ui_layouts_.begin(), ui_layouts_.end(),
                std::mem_fun(&LayoutTexture::gl_delete));

  label_uf_winsize_ = -1;
  label_uf_color_   = -1;
  label_uf_texture_ = -1;
  focus_uf_winsize_ = -1;
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
  glDisableVertexAttribArray(ATTRIB_POSITION);
  glDisableVertexAttribArray(ATTRIB_TEXCOORD);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDisable(GL_BLEND);

  GL::ShaderProgram::unuse();
}

/*
 * Note that this method is pure virtual because at least a glClear() command
 * needs to be issued by an overriding method to make the whole thing work.
 *
 * On execution of the user interface code, all GL rasterization operations
 * not enabled by default are assumed to be disabled.  Further, for texture
 * unit 0 the default texture environment and texture matrix are assumed.
 * If these conditions hold true, execution of the user interface rendering
 * code invalidates the following GL state:
 *
 *   - modelview matrix (reset to identity)
 *   - current matrix mode
 *   - alpha test function
 *   - blend function
 *   - 1D texture binding
 *   - 2D or rectangle texture binding
 *   - array pointers and buffer binding
 *   - current color
 *   - current texture coordinates
 *   - current raster position
 */
int Scene::gl_render()
{
  int triangle_count = 0;

  const LayoutVector::const_iterator pbegin = ui_layouts_.begin();
  const LayoutVector::const_iterator pend   = ui_layouts_.end();

  if (focus_drawable_ || std::find_if(pbegin, pend, LayoutTexture::IsDrawable()) != pend)
  {
    if (ui_buffer_)
    {
      glBindBuffer(GL_ARRAY_BUFFER, ui_buffer_);

      triangle_count = gl_render_ui();
      
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
  }
  return triangle_count;
}

int Scene::gl_render_ui()
{
  glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                        GL::buffer_offset(offsetof(UIVertex, vertex)));
  glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                        GL::buffer_offset(offsetof(UIVertex, texcoord)));
  glEnableVertexAttribArray(ATTRIB_POSITION);
  glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  gl_render_focus();
  const int triangle_count = gl_render_layouts();

  GL::ShaderProgram::unuse();

  glDisableVertexAttribArray(ATTRIB_TEXCOORD);
  glDisableVertexAttribArray(ATTRIB_POSITION);

  return triangle_count;
}

int Scene::gl_render_layouts()
{
  int triangle_count = 0;

  const auto first = std::find_if(ui_layouts_.begin(), ui_layouts_.end(),
                                  LayoutTexture::IsDrawable());

  if (first != ui_layouts_.end() && label_shader_)
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
 * Render text layouts and shadow in a single pass.  The target must be
 * either GL_TEXTURE_RECTANGLE_NV or GL_TEXTURE_2D.  At least the first
 * element in the non-empty sequence [first, ui_layouts_.end()) must be
 * enabled and ready for drawing.
 *
 * Assumed GL state on entry:
 *
 *   - server active texture unit GL_TEXTURE0
 *   - identity texture matrix for texture units 0 and 1
 *   - GL_MODULATE texture environment for unit 0
 *   - vertex array set up and enabled
 *   - texture coordinate array set up for units 0 and 1
 *   - texture coordinate array enabled for unit 0
 *
 * Invalidated GL state (state on return within parentheses):
 *
 *   - client active texture unit (reset to GL_TEXTURE0)
 *   - GL_TEXTURE_COORD_ARRAY enable of unit 1 (disabled)
 *   - texture target enable of units 0 and 1 (used target disabled)
 *   - texture environment of unit 1
 *   - texture binding of units 0 and 1
 *   - current color
 *   - current texture coordinates
 *   - current raster position
 */
int Scene::gl_render_layout_arrays(LayoutVector::const_iterator first)
{
  const LayoutTexture* layout = *first;

  glBindTexture(GL_TEXTURE_RECTANGLE, layout->tex_name_);
  glUniform4fv(label_uf_color_, 1, &layout->color()[0]);

  glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

  int triangle_count = LayoutTexture::TRIANGLE_COUNT;

  while (++first != ui_layouts_.end())
  {
    layout = *first;

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
  if (focus_drawable_ && focus_shader_)
  {
    focus_shader_.use();
    glUniform4fv(focus_uf_color_, 1, &focus_color_[0]);

    glDrawArrays(GL_LINE_LOOP, FOCUS_ARRAY_OFFSET, FOCUS_VERTEX_COUNT);
  }
}

/*
 * Generate vertices and texture coordinates for the layout,
 * assuming a 1:1 projection to window coordinates.
 */
void Scene::gl_build_layouts()
{
  for (LayoutVector::iterator p = ui_layouts_.begin(); p != ui_layouts_.end(); ++p)
  {
    const LayoutTexture *const layout = *p;

    if (layout->drawable())
    {
      int offset = 1;
      float s0 = -1.0;
      float t0 = 0.0;

      const float width  = layout->ink_width_  + offset;
      const float height = layout->ink_height_ + offset;

      float s1 = s0 + width;
      float t1 = t0 + height;

      const float x0 = layout->window_x_ + layout->ink_x_;
      const float y0 = layout->window_y_ + layout->ink_y_ + 1 - offset;
      const float x1 = x0 + width;
      const float y1 = y0 + height;

      g_return_if_fail(layout->array_offset_ + LayoutTexture::VERTEX_COUNT <= ui_geometry_.size());

      const GeometryVector::iterator geometry = ui_geometry_.begin() + layout->array_offset_;

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
}

void Scene::gl_build_focus()
{
  if (show_focus_ && has_focus())
  {
    bool interior_focus = false;
    int  focus_padding  = 0;

    get_style_property("interior_focus", interior_focus);
    get_style_property("focus_padding",  focus_padding);

    const int width  = get_width();
    const int height = get_height();

    if (interior_focus && width > 2 * focus_padding && height > 2 * focus_padding)
    {
      g_return_if_fail(FOCUS_ARRAY_OFFSET + FOCUS_VERTEX_COUNT <= ui_geometry_.size());

      generate_focus_rect(width, height, focus_padding,
                          ui_geometry_.begin() + FOCUS_ARRAY_OFFSET);
      focus_drawable_ = true;
    }
  }
}

void Scene::gl_update_viewport()
{
  const int width  = Math::max(1, get_width());
  const int height = Math::max(1, get_height());

  glViewport(0, 0, width, height);

  if (label_shader_)
  {
    label_shader_.use();
    glUniform2f(label_uf_winsize_, width, height);
  }
  if (focus_shader_)
  {
    focus_shader_.use();
    glUniform2f(focus_uf_winsize_, width, height);
  }
  GL::ShaderProgram::unuse();
}

void Scene::gl_update_projection()
{}

void Scene::gl_update_color()
{
  const Glib::RefPtr<Gtk::Style> style = get_style();
  const Gtk::StateType           state = get_state();

  const Gdk::Color fg = style->get_fg(state);
  const Gdk::Color bg = style->get_bg(state);

  focus_color_ = Math::Vector4(fg.get_red(), fg.get_green(), fg.get_blue(), 0xFFFF)
                 * (1.0f / 0xFFFF);

  const float red   = float(bg.get_red())   * (1.0f / 0xFFFF);
  const float green = float(bg.get_green()) * (1.0f / 0xFFFF);
  const float blue  = float(bg.get_blue())  * (1.0f / 0xFFFF);

  glClearColor(red, green, blue, 0.0);
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
  for (LayoutVector::iterator p = ui_layouts_.begin(); p != ui_layouts_.end(); ++p)
  {
    LayoutTexture *const layout = *p;

    if (!layout->content_.empty())
    {
      if (!layout->tex_name_ || layout->need_update_)
      {
        layout->gl_set_layout(create_texture_pango_layout(layout->content_));
      }
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

void Scene::setup_gl_context()
{
  GL::configure_widget(*this, GDK_GL_MODE_RGBA | GDK_GL_MODE_DOUBLE);
}

GL::Extensions* Scene::gl_query_extensions()
{
  return new GL::Extensions();
}

void Scene::gl_reposition_layouts()
{}

/*
 * Set up the widget for GL drawing as soon as a screen is available.
 * You can control the GL configuration by overriding setup_gl_context().
 */
void Scene::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen)
{
  Gtk::DrawingArea::on_screen_changed(previous_screen);

  if (has_screen() && !is_realized())
    setup_gl_context();
}

void Scene::on_size_allocate(Gtk::Allocation& allocation)
{
  Gtk::DrawingArea::on_size_allocate(allocation);

  if (is_realized())
  {
    ScopeContext context (*this);

    gl_update_viewport();
    gl_update_projection();
    gl_update_ui();
  }
}

void Scene::on_state_changed(Gtk::StateType previous_state)
{
  if (is_realized())
  {
    ScopeContext context (*this);

    gl_update_color();
    gl_update_ui();
  }

  Gtk::DrawingArea::on_state_changed(previous_state);
}

void Scene::on_style_changed(const Glib::RefPtr<Gtk::Style>& previous_style)
{
  // Avoid both reset() and clear()... it's all Murray's fault :-P
  Glib::RefPtr<Pango::Context>().swap(texture_context_);

  std::for_each(ui_layouts_.begin(), ui_layouts_.end(), LayoutTexture::Invalidate());

  if (is_realized())
  {
    ScopeContext context (*this);

    gl_update_color();
    gl_update_ui();
  }

  Gtk::DrawingArea::on_style_changed(previous_style);
}

void Scene::on_direction_changed(Gtk::TextDirection previous_direction)
{
  Glib::RefPtr<Pango::Context>().swap(texture_context_);

  std::for_each(ui_layouts_.begin(), ui_layouts_.end(), LayoutTexture::Invalidate());

  if (is_realized())
  {
    ScopeContext context (*this);

    gl_update_ui();
  }

  Gtk::DrawingArea::on_direction_changed(previous_direction);
}

bool Scene::on_expose_event(GdkEventExpose*)
{
  if (is_drawable())
  {
    ScopeContext context (*this);

    unsigned int triangle_count = 0;

    try
    {
      triangle_count = gl_render();
      GL::Error::check();
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

bool Scene::on_focus_in_event(GdkEventFocus* event)
{
  if (show_focus_ && is_realized())
  {
    ScopeContext context (*this);

    gl_update_ui();
  }

  return Gtk::DrawingArea::on_focus_in_event(event);
}

bool Scene::on_focus_out_event(GdkEventFocus* event)
{
  focus_drawable_ = false;

  return Gtk::DrawingArea::on_focus_out_event(event);
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
  g_return_if_fail(gl_drawable_ == 0);

  Glib::RefPtr<Pango::Context>().swap(texture_context_);
  std::for_each(ui_layouts_.begin(), ui_layouts_.end(), LayoutTexture::Invalidate());

  GtkWidget     *const glwidget = Gtk::Widget::gobj();
  GdkGLDrawable *const drawable = gtk_widget_get_gl_drawable(glwidget);

  gl_drawable_ = drawable;

  has_back_buffer_ = (gdk_gl_drawable_is_double_buffered(drawable) != 0);

  if (exclusive_context_)
  {
    if (!gdk_gl_drawable_gl_begin(drawable, gtk_widget_get_gl_context(glwidget)))
      throw GL::Error("rendering context could not be made current");
  }

  {
    ScopeContext context {*this};

    gl_extensions_.reset(gl_query_extensions());
    g_return_if_fail(gl_ext() != nullptr);

    if (GL::get_gl_version() < GL::make_version(3, 2))
      g_error("at least OpenGL 3.2 is required to run this program");

    gl_update_vsync_state();

    if (has_back_buffer_ && !use_back_buffer_)
      glDrawBuffer(GL_FRONT);

    gl_initialize();
  }
}

void Scene::on_signal_unrealize()
{
  if (gl_drawable_)
  {
    {
      ScopeContext context (*this);

      gl_cleanup();
      gl_extensions_.reset();
    }

    if (exclusive_context_)
      gdk_gl_drawable_gl_end(static_cast<GdkGLDrawable*>(gl_drawable_));

    gl_drawable_ = 0;

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
    GdkGLContext *const context = gtk_widget_get_gl_context(scene.Gtk::Widget::gobj());

    if (!gdk_gl_drawable_gl_begin(static_cast<GdkGLDrawable*>(scene.gl_drawable_), context))
      throw GL::Error("rendering context could not be made current");
  }
}

// static
void Scene::ScopeContext::end_(Scene& scene)
{
  if (!scene.exclusive_context_)
    gdk_gl_drawable_gl_end(static_cast<GdkGLDrawable*>(scene.gl_drawable_));
}

} // namespace GL
