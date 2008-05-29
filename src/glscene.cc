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

#include <cstring>
#include <algorithm>
#include <functional>

namespace
{

enum
{
  FOCUS_ARRAY_OFFSET    = 0,  // offset into geometry arrays
  FOCUS_TRIANGLE_COUNT  = 8,  // for throughput statistics
  FOCUS_VERTEX_COUNT    = 10,
  FOCUS_PATTERN_LENGTH  = 16  // must be a power of two
};

/*
 * Generate vertices for drawing the focus indicator of the GL widget.  This
 * function assumes an orthographic projection that establishes a 1:1 mapping
 * to window coordinates.
 *
 * The focus rectangle is drawn as a single strip of textured triangles
 * instead of a loop of stippled lines, because the former is more likely
 * to be accelerated in hardware.
 *
 * Note that even with an Nvidia GeForce4 Ti4200, which unlike my old Matrox
 * G400 card does accelerate line stippling in hardware, the triangle strip
 * variant is still slightly faster.  Update: I've got an Nvidia 6600 GT now,
 * and it doesn't make a noticeable difference (hard if you get thousands of
 * frames per second).  The fact remains that drawing triangles is definitely
 * at least as fast as drawing lines.  Triangles and texturing really are the
 * bread and butter of 3D graphics hardware, and thus it's wise to express as
 * much as possible in terms of this natural language.
 */
static
void generate_focus_rect(int width, int height, int padding, int line_width,
                         int repeat_count, GL::GeometryVector::iterator geometry)
{
  g_return_if_fail(line_width > 0 && repeat_count > 0);

  const int x0 = padding;
  const int y0 = padding;
  const int x1 = width  - padding;
  const int y1 = height - padding;

  const int sx = width  - 2 * padding;
  const int sy = height - 2 * padding;

  const float stride = FOCUS_PATTERN_LENGTH * repeat_count * line_width;

  // Applying 1D texture coordinates to a triangle strip that repeatedly
  // turns around corners proved to be a bit of a challenge.  However, the
  // back-and-forth moving done below does the trick without producing any
  // distortion.  Try with a line width of 16 for a detailed view.

  geometry[0].set_texcoord((sy - sx + line_width) / stride, 1.0);
  geometry[0].set_vertex(x0 + line_width, y0 + line_width);

  geometry[1].set_texcoord((sy - sx) / stride, 0.0);
  geometry[1].set_vertex(x0, y0);

  geometry[2].set_texcoord((sy - line_width) / stride, 1.0);
  geometry[2].set_vertex(x1 - line_width, y0 + line_width);

  geometry[3].set_texcoord(sy / stride, 0.0);
  geometry[3].set_vertex(x1, y0);

  geometry[4].set_texcoord(line_width / stride, 1.0);
  geometry[4].set_vertex(x1 - line_width, y1 - line_width);

  geometry[5].set_texcoord(0.0, 0.0);
  geometry[5].set_vertex(x1, y1);

  geometry[6].set_texcoord((sx - line_width) / stride, 1.0);
  geometry[6].set_vertex(x0 + line_width, y1 - line_width);

  geometry[7].set_texcoord(sx / stride, 0.0);
  geometry[7].set_vertex(x0, y1);

  geometry[8].set_texcoord((sx - sy + line_width) / stride, 1.0);
  geometry[8].set_vertex(x0 + line_width, y0 + line_width);

  geometry[9].set_texcoord((sx - sy) / stride, 0.0);
  geometry[9].set_vertex(x0, y0);
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
  return cairo_format_stride_for_width(CAIRO_FORMAT_A8, width + 7 & ~7);
#else
  return width + 7 & ~7;
#endif
}

} // anonymous namespace

namespace GL
{

Extensions::~Extensions()
{}

void Extensions::query()
{
  have_swap_control         = false;
  have_texture_rectangle    = false;
  have_texture_border_clamp = false;
  have_multitexture         = false;
  have_texture_env_combine  = false;
  have_vertex_buffer_object = false;

#if defined(GDK_WINDOWING_X11)
  SwapIntervalSGI     = 0;
#elif defined(GDK_WINDOWING_WIN32)
  SwapIntervalEXT     = 0;
#endif
  ActiveTexture       = 0;
  ClientActiveTexture = 0;
  GenBuffers          = 0;
  DeleteBuffers       = 0;
  BindBuffer          = 0;
  BufferData          = 0;

  version_    = GL::get_gl_version();
  extensions_ = glGetString(GL_EXTENSIONS);

  if (!extensions_)
    GL::Error::check();

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

  if (have_extension("GL_ARB_texture_rectangle")
      || have_extension("GL_EXT_texture_rectangle")
      || have_extension("GL_NV_texture_rectangle"))
  {
    have_texture_rectangle = true;
  }

  if (have_version(1, 3)
      || have_extension("GL_ARB_texture_border_clamp")
      || have_extension("GL_SGIS_texture_border_clamp"))
  {
    have_texture_border_clamp = true;
  }

  if (have_version(1, 3))
  {
    if (GL::get_proc_address(ActiveTexture,       "glActiveTexture") &&
        GL::get_proc_address(ClientActiveTexture, "glClientActiveTexture"))
    {
      have_multitexture = true;
    }
  }
  else if (have_extension("GL_ARB_multitexture"))
  {
    if (GL::get_proc_address(ActiveTexture,       "glActiveTextureARB") &&
        GL::get_proc_address(ClientActiveTexture, "glClientActiveTextureARB"))
    {
      have_multitexture = true;
    }
  }

  if (have_version(1, 3)
      || have_extension("GL_ARB_texture_env_combine")
      || have_extension("GL_EXT_texture_env_combine"))
  {
    have_texture_env_combine = true;
  }

  if (have_version(1, 5))
  {
    if (GL::get_proc_address(GenBuffers,    "glGenBuffers")    &&
        GL::get_proc_address(DeleteBuffers, "glDeleteBuffers") &&
        GL::get_proc_address(BindBuffer,    "glBindBuffer")    &&
        GL::get_proc_address(BufferData,    "glBufferData"))
    {
      have_vertex_buffer_object = true;
    }
  }
  else if (have_extension("GL_ARB_vertex_buffer_object"))
  {
    if (GL::get_proc_address(GenBuffers,    "glGenBuffersARB")    &&
        GL::get_proc_address(DeleteBuffers, "glDeleteBuffersARB") &&
        GL::get_proc_address(BindBuffer,    "glBindBufferARB")    &&
        GL::get_proc_address(BufferData,    "glBufferDataARB"))
    {
      have_vertex_buffer_object = true;
    }
  }
}

LayoutTexture::LayoutTexture()
:
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
{
  set_color(0xFF, 0xFF, 0xFF);
}

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

void LayoutTexture::gl_set_layout(const Glib::RefPtr<Pango::Layout>& layout,
                                  int img_border, GLenum target, GLenum clamp_mode)
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

  int img_height = ink_height + 2 * img_border;
  int img_width  = aligned_stride(ink_width + 2 * img_border);

  if (target == GL_TEXTURE_2D)
  {
    // Round up as needed to the closest power of two.
    img_width  = Math::round_pow2(img_width);
    img_height = Math::round_pow2(img_height);
  }
  // The dimensions of the new image are often identical with the previous
  // one's.  Exploit this by uploading only a sub-area of the texture image
  // which covers the union of the new ink rectangle and the previous one.
  const bool sub_image = (tex_name_ && tex_width_ == img_width && tex_height_ == img_height);

  if (sub_image && target == GL_TEXTURE_2D)
  {
    // At this point, ink_width_ and ink_height_ are either valid, or there
    // is a bug somewhere.  But there is no need for extensive checks right
    // here, as glTexSubImage2D() would bail out later anyway.
    img_height = Math::max(ink_height, ink_height_) + 2 * img_border;
    img_width  = aligned_stride(Math::max(ink_width, ink_width_) + 2 * img_border);
  }

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
    cairo_move_to(context, img_border + PADDING - ink.get_x(),
                         -(img_border + PADDING + ink.get_y() + ink.get_height()));

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

  glBindTexture(target, tex_name_);

  if (sub_image)
  {
    glTexSubImage2D(target, 0, 0, 0, img_width, img_height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, &tex_image[0]);
    GL::Error::check();
  }
  else
  {
    if (tex_width_ == 0)
    {
      glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(target, GL_TEXTURE_WRAP_S,     clamp_mode);
      glTexParameteri(target, GL_TEXTURE_WRAP_T,     clamp_mode);
    }

    glTexImage2D(target, 0, GL_INTENSITY8, img_width, img_height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, &tex_image[0]);
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
  gl_drawable_        (0),
  gl_extensions_      (),
  texture_context_    (),
  ui_geometry_        (FOCUS_VERTEX_COUNT),
  ui_layouts_         (),
  ui_buffer_          (0),
  stipple_texture_    (0),
  frame_counter_      (0),
  triangle_counter_   (0),
  exclusive_context_  (false),
  has_back_buffer_    (false),
  use_back_buffer_    (true),
  enable_vsync_       (false),
  vsync_enabled_      (false),
  show_focus_         (true),
  focus_drawable_     (false),
  use_multitexture_   (false)
{
  focus_color_[0] = 0xFF;
  focus_color_[1] = 0xFF;
  focus_color_[2] = 0xFF;

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

  if (!ui_geometry_.empty() && gl_ext()->have_vertex_buffer_object)
  {
    if (!ui_buffer_)
    {
      gl_ext()->GenBuffers(1, &ui_buffer_);

      GL::Error::throw_if_fail(ui_buffer_ != 0);
    }

    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, ui_buffer_);
    gl_ext()->BufferData(GL_ARRAY_BUFFER_ARB, ui_geometry_.size() * sizeof(UIVertex),
                         &ui_geometry_[0], GL_DYNAMIC_DRAW_ARB);
    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);

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
  gl_update_viewport();
  gl_update_projection();
  gl_update_color();
}

void Scene::gl_cleanup()
{
  focus_drawable_ = false;

  if (ui_buffer_ && gl_ext()->have_vertex_buffer_object)
  {
    gl_ext()->DeleteBuffers(1, &ui_buffer_);
    ui_buffer_ = 0;
  }

  std::for_each(ui_layouts_.begin(), ui_layouts_.end(),
                std::mem_fun(&LayoutTexture::gl_delete));

  if (stipple_texture_)
  {
    glDeleteTextures(1, &stipple_texture_);
    stipple_texture_ = 0;
  }
}

/*
 * This method is invoked to sanitize the GL state after
 * an exception occured during execution of gl_render().
 */
void Scene::gl_reset_state()
{
  glDisableClientState(GL_VERTEX_ARRAY);

  if (gl_ext()->have_vertex_buffer_object)
    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);

  if (gl_ext()->have_multitexture)
  {
    if (use_multitexture_)
    {
      gl_ext()->ClientActiveTexture(GL_TEXTURE1);
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);

      gl_ext()->ActiveTexture(GL_TEXTURE1);
      glDisable(GL_TEXTURE_2D);

      if (gl_ext()->have_texture_rectangle)
        glDisable(GL_TEXTURE_RECTANGLE_NV);
    }

    gl_ext()->ClientActiveTexture(GL_TEXTURE0);
    gl_ext()->ActiveTexture(GL_TEXTURE0);
  }

  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  glDisable(GL_TEXTURE_1D);
  glDisable(GL_TEXTURE_2D);

  if (gl_ext()->have_texture_rectangle)
    glDisable(GL_TEXTURE_RECTANGLE_NV);

  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
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
    GL::ScopeMatrix projection (GL_PROJECTION);

    glLoadIdentity();
    glOrtho(0.0, Math::max(1, get_width()), 0.0, Math::max(1, get_height()), -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (gl_ext()->have_vertex_buffer_object)
    {
      if (ui_buffer_)
      {
        gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, ui_buffer_);

        triangle_count = gl_render_ui(GL::buffer_offset(0));

        gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);
      }
    }
    else
    {
      if (!ui_geometry_.empty())
      {
        triangle_count = gl_render_ui(&ui_geometry_[0]);
      }
    }
  }

  return triangle_count;
}

int Scene::gl_render_ui(void* arrays) const
{
  GLubyte *const byte_start = static_cast<GLubyte*>(arrays);

  if (use_multitexture_)
  {
    gl_ext()->ClientActiveTexture(GL_TEXTURE1);
    glTexCoordPointer(2, GL_FLOAT, sizeof(UIVertex), byte_start);

    gl_ext()->ClientActiveTexture(GL_TEXTURE0);
  }

  glTexCoordPointer(2, GL_FLOAT, sizeof(UIVertex), byte_start);
  glVertexPointer  (2, GL_FLOAT, sizeof(UIVertex), byte_start + 2 * sizeof(GLfloat));

  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnableClientState(GL_VERTEX_ARRAY);

  int triangle_count = 0;

  triangle_count += gl_render_focus();
  triangle_count += gl_render_layouts();

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  return triangle_count;
}

int Scene::gl_render_layouts() const
{
  int triangle_count = 0;

  const LayoutVector::const_iterator first =
      std::find_if(ui_layouts_.begin(), ui_layouts_.end(), LayoutTexture::IsDrawable());

  if (first != ui_layouts_.end())
  {
    // The source function is identity because we blend in an intensity
    // texture. That is, the color channels are premultiplied by alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    GLenum target = GL_TEXTURE_2D;

    if (gl_ext()->have_texture_rectangle)
      target = GL_TEXTURE_RECTANGLE_NV;

    if (use_multitexture_)
      triangle_count = gl_render_layouts_multitexture(target, first);
    else
      triangle_count = gl_render_layouts_multipass(target, first);

    glDisable(GL_BLEND);
  }

  return triangle_count;
}

/*
 * Render text layouts and shadow in a single pass.
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
int Scene::gl_render_layouts_multitexture(unsigned int target,
                                          LayoutVector::const_iterator first) const
{
  gl_ext()->ClientActiveTexture(GL_TEXTURE1);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glMatrixMode(GL_TEXTURE);

  const LayoutTexture* layout = *first;

  if (target != GL_TEXTURE_2D)
    glTranslatef(1.0, -1.0, 0.0);
  else
    glTranslatef(1.0f / layout->tex_width_, -1.0f / layout->tex_height_, 0.0);

  glBindTexture(target, layout->tex_name_);
  glEnable(target);

  gl_ext()->ActiveTexture(GL_TEXTURE1);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB,      GL_REPLACE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB,      GL_PREVIOUS);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA,    GL_ADD);

  glBindTexture(target, layout->tex_name_);
  glEnable(target);

  glColor3ubv(layout->color_);
  glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

  int triangle_count = LayoutTexture::TRIANGLE_COUNT;

  while (++first != ui_layouts_.end())
  {
    layout = *first;

    if (layout->drawable())
    {
      gl_ext()->ActiveTexture(GL_TEXTURE0);
      glBindTexture(target, layout->tex_name_);

      if (target == GL_TEXTURE_2D)
      {
        glLoadIdentity();
        glTranslatef(1.0f / layout->tex_width_, -1.0f / layout->tex_height_, 0.0);
      }

      gl_ext()->ActiveTexture(GL_TEXTURE1);
      glBindTexture(target, layout->tex_name_);

      glColor3ubv(layout->color_);
      glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

      triangle_count += LayoutTexture::TRIANGLE_COUNT;
    }
  }

  glDisable(target);

  gl_ext()->ActiveTexture(GL_TEXTURE0);
  glDisable(target);

  glLoadIdentity();

  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_ext()->ClientActiveTexture(GL_TEXTURE0);

  return triangle_count;
}

int Scene::gl_render_layouts_multipass(unsigned int target,
                                       LayoutVector::const_iterator first) const
{
  static const GLubyte shadow_color[3] = { 0x00, 0x00, 0x00 };

  int triangle_count = 0;

  glEnable(target);

  do
  {
    const LayoutTexture *const layout = *first;

    if (layout->drawable())
    {
      glBindTexture(target, layout->tex_name_);

      glTranslatef(1.0, -1.0, 0.0);
      glColor3ubv(shadow_color);
      glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

      glLoadIdentity();
      glColor3ubv(layout->color_);
      glDrawArrays(GL_TRIANGLE_STRIP, layout->array_offset_, LayoutTexture::VERTEX_COUNT);

      triangle_count += 2 * LayoutTexture::TRIANGLE_COUNT;
    }
  }
  while (++first != ui_layouts_.end());

  glDisable(target);

  return triangle_count;
}

/*
 * On entry, the GL environment is set up for 2D drawing of static UI
 * elements:  An orthographic projection is set up that maps vertices
 * directly to window coordinates, and the modelview transformation is
 * set to identity.
 */
int Scene::gl_render_focus() const
{
  if (focus_drawable_)
  {
    glAlphaFunc(GL_GEQUAL, 0.5);
    glEnable(GL_ALPHA_TEST);

    glBindTexture(GL_TEXTURE_1D, stipple_texture_);
    glEnable(GL_TEXTURE_1D);

    glColor3ubv(focus_color_);
    glDrawArrays(GL_TRIANGLE_STRIP, FOCUS_ARRAY_OFFSET, FOCUS_VERTEX_COUNT);

    glDisable(GL_TEXTURE_1D);
    glDisable(GL_ALPHA_TEST);

    return FOCUS_TRIANGLE_COUNT;
  }

  return 0;
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
      int offset = 0;
      float s0 = 0.0;
      float t0 = 0.0;

      if (use_multitexture_)
      {
        offset = 1;

        if (gl_ext()->have_texture_border_clamp)
          s0 = -1.0;
        else
          t0 = 1.0;
      }

      const float width  = layout->ink_width_  + offset;
      const float height = layout->ink_height_ + offset;

      float s1 = s0 + width;
      float t1 = t0 + height;

      if (!gl_ext()->have_texture_rectangle)
      {
        g_return_if_fail(layout->tex_width_ > 0 && layout->tex_height_ > 0);

        s0 /= layout->tex_width_;
        s1 /= layout->tex_width_;
        t0 /= layout->tex_height_;
        t1 /= layout->tex_height_;
      }

      const float x0 = layout->window_x_ + layout->ink_x_;
      const float y0 = layout->window_y_ + layout->ink_y_ + 1 - offset;
      const float x1 = x0 + width;
      const float y1 = y0 + height;

      g_return_if_fail(layout->array_offset_ + LayoutTexture::VERTEX_COUNT <= ui_geometry_.size());

      const GeometryVector::iterator geometry = ui_geometry_.begin() + layout->array_offset_;

      geometry[0].set_texcoord(s0, t0);
      geometry[0].set_vertex(x0, y0);

      geometry[1].set_texcoord(s1, t0);
      geometry[1].set_vertex(x1, y0);

      geometry[2].set_texcoord(s0, t1);
      geometry[2].set_vertex(x0, y1);

      geometry[3].set_texcoord(s1, t1);
      geometry[3].set_vertex(x1, y1);
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
      if (!stipple_texture_)
        gl_init_stipple_texture();

      // Supporting arbitrary line stipple patterns is probably not worth the
      // effort.  However, we can easily make the most common focus patterns
      // work by using just the first repeat count for both on and off state.
      std::string focus_line_pattern;
      int         focus_line_width = 0;

      get_style_property("focus_line_pattern", focus_line_pattern);
      get_style_property("focus_line_width",   focus_line_width);

      g_return_if_fail(FOCUS_ARRAY_OFFSET + FOCUS_VERTEX_COUNT <= ui_geometry_.size());

      generate_focus_rect(width, height, focus_padding, Math::max(1, focus_line_width),
                          (focus_line_pattern.empty()) ? 1 : guchar(focus_line_pattern[0]),
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
}

/*
 * If you chain up from an overriding method, it is safe to rely on
 * the base implementation to leave the matrix mode at GL_PROJECTION.
 */
void Scene::gl_update_projection()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
}

void Scene::gl_update_color()
{
  const Glib::RefPtr<Gtk::Style> style = get_style();
  const Gtk::StateType           state = get_state();

  const Gdk::Color fg = style->get_fg(state);
  const Gdk::Color bg = style->get_bg(state);

  focus_color_[0] = (fg.get_red()   & 0xFF00u) >> 8;
  focus_color_[1] = (fg.get_green() & 0xFF00u) >> 8;
  focus_color_[2] = (fg.get_blue()  & 0xFF00u) >> 8;

  const float red   = float(bg.get_red())   / G_MAXUINT16;
  const float green = float(bg.get_green()) / G_MAXUINT16;
  const float blue  = float(bg.get_blue())  / G_MAXUINT16;

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
  GLenum  target     = GL_TEXTURE_2D;
  GLenum  clamp_mode = GL_CLAMP;
  int     img_border = 0;

  if (gl_ext()->have_texture_rectangle)
    target = GL_TEXTURE_RECTANGLE_NV;

  // Unconditionally enable border clamping whenever available, so that
  // out-of-bounds texture coordinates always reference the border color.
  // Otherwise, when multitexturing, include a border within the image to
  // avoid wrapping artifacts due to out-of-bounds texture coordinates.
  if (gl_ext()->have_texture_border_clamp)
    clamp_mode = GL_CLAMP_TO_BORDER;
  else if (use_multitexture_)
    img_border = 1;

  for (LayoutVector::iterator p = ui_layouts_.begin(); p != ui_layouts_.end(); ++p)
  {
    LayoutTexture *const layout = *p;

    if (!layout->content_.empty())
    {
      if (!layout->tex_name_ || layout->need_update_)
      {
        layout->gl_set_layout(create_texture_pango_layout(layout->content_),
                              img_border, target, clamp_mode);
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
    Glib::RefPtr<Pango::Context> context = create_pango_context();
    LayoutTexture::prepare_pango_context(context);

    swap(texture_context_, context);
  }

  const Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(texture_context_);
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
    ScopeContext context (*this);

    use_multitexture_ = false;
    gl_extensions_.reset(gl_query_extensions());

    g_return_if_fail(gl_ext() != 0);

    if (!gl_ext()->have_version(1, 1))
      g_error("at least OpenGL 1.1 is required to run this program");

    gl_update_vsync_state();

    if (gl_ext()->have_multitexture && gl_ext()->have_texture_env_combine)
    {
      GLint max_texture_units = 0;
      glGetIntegerv(GL_MAX_TEXTURE_UNITS, &max_texture_units);

      use_multitexture_ = (max_texture_units >= 2);
    }

    if (has_back_buffer_ && !use_back_buffer_)
      glDrawBuffer(GL_FRONT);

    // Use 8-byte alignment for all pixel rectangle transfers.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    glPixelStorei(GL_PACK_ALIGNMENT,   8);

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

      use_multitexture_ = false;
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

void Scene::gl_init_stipple_texture()
{
  g_return_if_fail(stipple_texture_ == 0);

  // Generate the focus line pattern dynamically, with proper alignment.
  Util::MemChunk<GLubyte> pattern ((FOCUS_PATTERN_LENGTH + 7) & ~7);

  for (Util::MemChunk<GLubyte>::size_type i = 0; i < pattern.size() / 2; ++i)
  {
    pattern[2 * i]     = 0xFF;
    pattern[2 * i + 1] = 0x00;
  }

  glGenTextures(1, &stipple_texture_);

  GL::Error::throw_if_fail(stipple_texture_ != 0);

  glBindTexture(GL_TEXTURE_1D, stipple_texture_);

  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glTexImage1D(GL_TEXTURE_1D, 0, GL_ALPHA, FOCUS_PATTERN_LENGTH, 0,
               GL_ALPHA, GL_UNSIGNED_BYTE, &pattern[0]);
  GL::Error::check();
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
