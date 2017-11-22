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

#include <algorithm>

namespace
{

struct UIVertex
{
  float          position[2];
  GL::Packed2i16 texcoord;
  GL::Packed4u8  color;

  void set(float x, float y, GL::Packed2i16 t, GL::Packed4u8 c) volatile
  {
    position[0] = x;
    position[1] = y;
    texcoord    = t;
    color       = c;
  }
};

using UIIndex = GLushort;

/* UI vertex shader input attribute locations.
 */
enum
{
  ATTRIB_POSITION = 0,
  ATTRIB_TEXCOORD = 1,
  ATTRIB_COLOR    = 2
};

/* UI text layout fragment shader texture unit.
 */
enum
{
  SAMPLER_LAYOUT = 0
};

/* Index usage convention for arrays of buffer objects.
 */
enum
{
  VERTICES = 0,
  INDICES  = 1
};

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

/* Make the texture image stride at least a multiple of 8 to meet SGI's
 * alignment recommendation.  This also avoids the padding bytes that would
 * otherwise be necessary in order to ensure row alignment.
 *
 * Note that cairo 1.6 made it mandatory to retrieve the stride to be used
 * with an image surface at runtime.  Currently, the alignment requested by
 * cairo is actually lower than our own, but that could change some day.
 */
inline int aligned_stride(int width)
{
  return Cairo::ImageSurface::format_stride_for_width(Cairo::FORMAT_A8, (width + 7) & ~7u);
}

} // anonymous namespace

namespace GL
{

LayoutTexView::LayoutTexView()
:
  color_        {},
  text_changed_ {false},
  attr_changed_ {false},
  x_offset_     {0},
  ink_x_        {0},
  ink_y_        {0},
  ink_width_    {0},
  ink_height_   {0},
  log_width_    {0},
  log_height_   {0},
  window_x_     {0},
  window_y_     {0}
{}

LayoutTexView::~LayoutTexView()
{}

LayoutAtlas::LayoutAtlas()
{}

LayoutAtlas::~LayoutAtlas()
{
  g_warn_if_fail(!tex_name && !vao && !buffers[VERTICES] && !buffers[INDICES]);
}

void LayoutTexView::set_content(Glib::ustring content)
{
  if (content.raw() != content_.raw())
  {
    content_ = std::move(content);
    text_changed_ = true;
  }
}

void LayoutTexView::set_window_pos(int x, int y)
{
  if (x != window_x_ || y != window_y_)
  {
    window_x_ = x;
    window_y_ = y;

    if (ink_width_ > 0)
      attr_changed_ = true;
  }
}

bool LayoutAtlas::needs_texture_update() const
{
  return std::any_of(cbegin(views), cend(views),
                     [](const auto& v) { return v->text_changed_; });
}

bool LayoutAtlas::needs_vertex_update() const
{
  return std::any_of(cbegin(views), cend(views),
                     [](const auto& v) { return v->attr_changed_; });
}

std::pair<int, int> LayoutAtlas::get_drawable_range() const
{
  const auto is_drawable = [](const std::unique_ptr<LayoutTexView>& view)
    { return view->drawable(); };

  const auto start = std::find_if(cbegin(views), cend(views), is_drawable);

  if (start == cend(views) || !tex_name || !vao)
    return {0, 0};

  const auto stop = std::find_if(crbegin(views), crend(views), is_drawable);

  return {start - cbegin(views), crend(views) - stop};
}

void LayoutAtlas::gl_update_texture()
{
  std::vector<Glib::RefPtr<Pango::Layout>> layouts;
  layouts.reserve(views.size());

  int img_width  = 0;
  int img_height = 0;

  for (const auto& view : views)
  {
    layouts.push_back(update_layout_extents(*view));

    if (view->ink_width_ > 0)
    {
      if (view->x_offset_ != img_width)
      {
        view->x_offset_ = img_width;
        view->attr_changed_ = true;
      }
      img_width += view->ink_width_ + PADDING;
      img_height = std::max(img_height, view->ink_height_);
    }
  }
  if (img_width <= PADDING)
    return;

  // Remove the padding overshoot before adding alignment.
  img_width  = aligned_stride(img_width - PADDING);
  img_height = (img_height + 3) & ~3u;

  std::vector<GLubyte> tex_image (img_height * img_width);
  {
    // Create a Cairo surface to draw the layout directly into the texture image.
    // Note that the image will be upside-down from the point of view of OpenGL,
    // thus the texture coordinates need to be adjusted accordingly.
    const auto surface = Cairo::ImageSurface::create(&tex_image[0],
        Cairo::FORMAT_A8, img_width, img_height, img_width * sizeof(GLubyte));
    const auto context = Cairo::Context::create(surface);

    for (std::size_t i = 0; i < layouts.size(); ++i)
      if (const auto& layout = layouts[i])
      {
        const Pango::Rectangle ink = layout->get_pixel_ink_extents();

        context->move_to(views[i]->x_offset_ + MARGIN - ink.get_x(),
                         MARGIN - ink.get_y());
        layout->show_in_cairo_context(context);
      }
  }
  glActiveTexture(GL_TEXTURE0 + SAMPLER_LAYOUT);

  if (!tex_name)
  {
    tex_width  = 0;
    tex_height = 0;

    glGenTextures(1, &tex_name);
    GL::Error::throw_if_fail(tex_name != 0);
  }
  glBindTexture(GL_TEXTURE_2D, tex_name);

  if (tex_width == 0)
  {
    GL::set_object_label(GL_TEXTURE, tex_name, "layoutAtlas");

    const GLenum clamp_mode = (GL::extensions().texture_border_clamp)
                              ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  if (tex_width != img_width || tex_height != img_height)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, img_width, img_height,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    tex_width  = img_width;
    tex_height = img_height;
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img_width, img_height,
                  GL_RED, GL_UNSIGNED_BYTE, &tex_image[0]);

  for (const auto& view : views)
    view->text_changed_ = false;
}

void LayoutAtlas::gl_generate_vertices(int view_width, int view_height)
{
  if (tex_width <= 0 || tex_height <= 0)
    return;

  g_return_if_fail(buffers[VERTICES]);
  glBindBuffer(GL_ARRAY_BUFFER, buffers[VERTICES]);

  const std::size_t vertex_data_size =
      views.size() * LayoutTexView::VERTEX_COUNT * sizeof(UIVertex);

  if (instance_count != views.size())
    glBufferData(GL_ARRAY_BUFFER, vertex_data_size, nullptr, GL_DYNAMIC_DRAW);

  const bool ok = access_mapped_buffer(GL_ARRAY_BUFFER, 0, vertex_data_size,
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
                                       [=](volatile void* data)
  {
    const float tex_scale_x  = 0.5f / tex_width;
    const float tex_scale_y  = 0.5f / tex_height;
    const float view_scale_x = 1.f / view_width;
    const float view_scale_y = 1.f / view_height;

    // Shift coordinates to center of 2x2 block for texture gather.
    const int shadow_offset = (GL::extensions().texture_gather) ? -1 : -2;

    // Shift coordinates into normalized [-1, 1] range (reversed in shader).
    const int tex_offset_x = shadow_offset - tex_width;
    const int tex_offset_y = shadow_offset - tex_height;

    auto* pv = static_cast<volatile UIVertex*>(data);

    for (const auto& view : views)
    {
      const int width  = view->ink_width_  + 1;
      const int height = view->ink_height_ + 1;

      const float s0 = (2 * view->x_offset_ + tex_offset_x)             * tex_scale_x;
      const float s1 = (2 * view->x_offset_ + tex_offset_x + 2 * width) * tex_scale_x;
      const float t0 = (tex_offset_y + 2 * height) * tex_scale_y;
      const float t1 = (tex_offset_y)              * tex_scale_y;

      const int view_x = view->window_x_ + view->ink_x_;
      const int view_y = view->window_y_ + view->ink_y_;

      const float x0 = (2 * view_x - view_width)  * view_scale_x;
      const float y0 = (2 * view_y - view_height) * view_scale_y;
      const float x1 = (2 * (view_x + width)  - view_width)  * view_scale_x;
      const float y1 = (2 * (view_y + height) - view_height) * view_scale_y;

      const auto color = view->color_;

      pv[0].set(x0, y0, pack_2i16_norm(s0, t0), color);
      pv[1].set(x1, y0, pack_2i16_norm(s1, t0), color);
      pv[2].set(x0, y1, pack_2i16_norm(s0, t1), color);
      pv[3].set(x1, y1, pack_2i16_norm(s1, t1), color);

      pv += LayoutTexView::VERTEX_COUNT;
    }
  });

  if (ok)
  {
    for (const auto& view : views)
      view->attr_changed_ = false;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  if (instance_count != views.size())
  {
    glBindVertexArray(vao);
    gl_generate_indices();
    glBindVertexArray(0);

    instance_count = views.size();
  }
}

void LayoutAtlas::gl_generate_indices()
{
  constexpr std::size_t index_stride  = LayoutTexView::INDEX_COUNT;
  constexpr std::size_t vertex_stride = LayoutTexView::VERTEX_COUNT;

  const auto indices = std::make_unique<UIIndex[]>(views.size() * index_stride);

  // For each instance, generate 6 indices to draw 2 triangles from 4 vertices.
  for (std::size_t i = 0; i < views.size(); ++i)
  {
    indices[index_stride*i + 0] = vertex_stride*i + 0;
    indices[index_stride*i + 1] = vertex_stride*i + 1;
    indices[index_stride*i + 2] = vertex_stride*i + 2;
    indices[index_stride*i + 3] = vertex_stride*i + 3;
    indices[index_stride*i + 4] = vertex_stride*i + 2;
    indices[index_stride*i + 5] = vertex_stride*i + 1;
  }
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               views.size() * index_stride * sizeof(UIIndex),
               indices.get(), GL_STATIC_DRAW);
}

void LayoutAtlas::gl_create_vao()
{
  g_return_if_fail(!buffers[VERTICES] && !buffers[INDICES]);

  instance_count = 0;

  glGenVertexArrays(1, &vao);
  GL::Error::throw_if_fail(vao != 0);

  glGenBuffers(G_N_ELEMENTS(buffers), buffers);
  GL::Error::throw_if_fail(buffers[VERTICES] && buffers[INDICES]);

  glBindVertexArray(vao);
  GL::set_object_label(GL_VERTEX_ARRAY, vao, "layoutsArray");

  glBindBuffer(GL_ARRAY_BUFFER, buffers[VERTICES]);
  GL::set_object_label(GL_BUFFER, buffers[VERTICES], "layoutVertices");

  glBufferData(GL_ARRAY_BUFFER,
               views.size() * LayoutTexView::VERTEX_COUNT * sizeof(UIVertex),
               nullptr, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(ATTRIB_POSITION,
                        GL::attrib_size<decltype(UIVertex::position)>,
                        GL::attrib_type<decltype(UIVertex::position)>,
                        GL_FALSE, sizeof(UIVertex),
                        GL::buffer_offset(offsetof(UIVertex, position)));
  glVertexAttribPointer(ATTRIB_TEXCOORD,
                        GL::attrib_size<decltype(UIVertex::texcoord)>,
                        GL::attrib_type<decltype(UIVertex::texcoord)>,
                        GL_TRUE, sizeof(UIVertex),
                        GL::buffer_offset(offsetof(UIVertex, texcoord)));
  glVertexAttribPointer(ATTRIB_COLOR,
                        GL::attrib_size<decltype(UIVertex::color)>,
                        GL::attrib_type<decltype(UIVertex::color)>,
                        GL_TRUE, sizeof(UIVertex),
                        GL::buffer_offset(offsetof(UIVertex, color)));

  glEnableVertexAttribArray(ATTRIB_POSITION);
  glEnableVertexAttribArray(ATTRIB_TEXCOORD);
  glEnableVertexAttribArray(ATTRIB_COLOR);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[INDICES]);
  GL::set_object_label(GL_BUFFER, buffers[INDICES], "layoutIndices");

  gl_generate_indices();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  instance_count = views.size();
}

void LayoutAtlas::gl_delete()
{
  instance_count = 0;

  if (tex_name)
  {
    glDeleteTextures(1, &tex_name);
    tex_name = 0;
  }
  tex_width  = 0;
  tex_height = 0;

  glDeleteVertexArrays(1, &vao);
  vao = 0;

  if (buffers[VERTICES] || buffers[INDICES])
  {
    glDeleteBuffers(G_N_ELEMENTS(buffers), buffers);
    buffers[VERTICES] = 0;
    buffers[INDICES]  = 0;
  }
}

Glib::RefPtr<Pango::Layout> LayoutAtlas::update_layout_extents(LayoutTexView& view)
{
  if (view.content_.empty())
  {
    if (view.ink_width_ > 0)
      view.attr_changed_ = true;

    view.ink_x_      = 0;
    view.ink_y_      = 0;
    view.ink_width_  = 0;
    view.ink_height_ = 0;
    view.log_width_  = 0;
    view.log_height_ = 0;

    return {};
  }
  auto layout = Pango::Layout::create(layout_context);
  layout->set_text(view.content_);

  Pango::Rectangle ink, logical;
  // Measure ink extents to determine the dimensions of the image, but
  // keep the logical extents and the ink offsets around for positioning.
  layout->get_pixel_extents(ink, logical);

  // Make sure the extents are within reasonable boundaries.
  g_return_val_if_fail(ink.get_width() < 4095 && ink.get_height() < 4095,
                       Glib::RefPtr<Pango::Layout>{});

  const int ink_x = ink.get_x() - logical.get_x() - MARGIN;
  const int ink_y = logical.get_y() + logical.get_height()
                  - ink.get_y() - ink.get_height() - MARGIN;

  const int ink_width  = std::max(0, ink.get_width())  + 2 * MARGIN;
  const int ink_height = std::max(0, ink.get_height()) + 2 * MARGIN;

  // Expand the logical rectangle to account for the shadow offset.
  const int log_width  = logical.get_width()  + 1;
  const int log_height = logical.get_height() + 1;

  if (ink_x != view.ink_x_ || ink_y != view.ink_y_
      || ink_width != view.ink_width_ || ink_height != view.ink_height_
      || log_width != view.log_width_ || log_height != view.log_height_)
  {
    view.ink_x_        = ink_x;
    view.ink_y_        = ink_y;
    view.ink_width_    = ink_width;
    view.ink_height_   = ink_height;
    view.log_width_    = log_width;
    view.log_height_   = log_height;
    view.attr_changed_ = true;
  }
  return std::move(layout);
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

LayoutTexView* Scene::create_layout_view()
{
  // For now, layout textures may only be created at initialization time.
  g_return_val_if_fail(!layouts_.vao, nullptr);

  layouts_.views.push_back(std::make_unique<LayoutTexView>());

  return layouts_.views.back().get();
}

void Scene::gl_update_ui()
{
  gl_reposition_layouts();
  gl_update_layouts();
}

void Scene::gl_initialize()
{
  gl_create_label_shader();
  gl_update_framebuffer();

  glViewport(0, 0, get_viewport_width(), get_viewport_height());

  gl_update_projection();
  layouts_.gl_create_vao();

  // The source function is identity because we blend in an intensity
  // texture. That is, the color channels are premultiplied by alpha.
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (label_shader_)
  {
    label_shader_.use();

    glUniform1i(label_uf_texture_, SAMPLER_LAYOUT);
    gl_update_focus_state();
  }
}

void Scene::gl_cleanup()
{
  layouts_.gl_delete();
  gl_delete_framebuffer();

  label_uf_intensity_ = -1;
  label_uf_texture_   = -1;
  label_shader_.reset();
}

/*
 * This method is invoked to sanitize the GL state after
 * an exception occured during execution of gl_render().
 */
void Scene::gl_reset_state()
{
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDisable(GL_BLEND);

  GL::ShaderProgram::unuse();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glActiveTexture(GL_TEXTURE0);
}

/*
 * Note that this method is pure virtual because at least a glClear() command
 * needs to be issued by an overriding method to make the whole thing work.
 */
int Scene::gl_render()
{
  int triangle_count = 0;
  const auto range = layouts_.get_drawable_range();

  if (range.first < range.second && label_shader_)
  {
    glEnable(GL_BLEND);
    label_shader_.use();
    glBindVertexArray(layouts_.vao);

    glDrawRangeElements(GL_TRIANGLES,
                        LayoutTexView::VERTEX_COUNT * range.first,
                        LayoutTexView::VERTEX_COUNT * range.second - 1,
                        LayoutTexView::INDEX_COUNT * (range.second - range.first),
                        GL::attrib_type<UIIndex>,
                        GL::buffer_offset<UIIndex>(LayoutTexView::INDEX_COUNT * range.first));
    glBindVertexArray(0);
    glDisable(GL_BLEND);

    triangle_count += LayoutTexView::TRIANGLE_COUNT * (range.second - range.first);
  }
  return triangle_count;
}

void Scene::gl_update_projection()
{}

void Scene::on_realize()
{
  layouts_.layout_context.reset();

  for (const auto& view : layouts_.views)
    view->invalidate();

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

  layouts_.layout_context.reset();
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
  layouts_.layout_context.reset();

  for (const auto& view : layouts_.views)
    view->invalidate();

  Gtk::GLArea::on_style_updated();

  if (auto guard = scoped_make_current())
  {
    gl_update_ui();
  }
}

void Scene::on_state_changed(Gtk::StateType previous_state)
{
  Gtk::GLArea::on_state_changed(previous_state);

  if (auto guard = scoped_make_current())
  {
    if (label_shader_)
    {
      label_shader_.use();
      gl_update_focus_state();
    }
    gl_update_ui();
  }
}

void Scene::on_direction_changed(Gtk::TextDirection previous_direction)
{
  layouts_.layout_context.reset();

  for (const auto& view : layouts_.views)
    view->invalidate();

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

bool Scene::try_make_current()
{
  const auto context = get_context();

  if (context)
    context->make_current();

  return !!context;
}

void Scene::gl_create_label_shader()
{
  GL::ShaderProgram program;
  program.set_label("textlabel");

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/textlabel.vert"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/textlabel.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_TEXCOORD, "texcoord");
  program.bind_attrib_location(ATTRIB_COLOR,    "color");
  program.link();

  label_uf_intensity_ = program.get_uniform_location("textIntensity");
  label_uf_texture_   = program.get_uniform_location("labelTexture");

  label_shader_ = std::move(program);
}

void Scene::gl_update_framebuffer()
{
  gl_delete_framebuffer();

  glGenRenderbuffers(G_N_ELEMENTS(render_buffers_), render_buffers_);
  GL::Error::throw_if_fail(render_buffers_[COLOR] && render_buffers_[DEPTH]);

  glGenFramebuffers(1, &frame_buffer_);
  GL::Error::throw_if_fail(frame_buffer_ != 0);

  const int view_width  = get_viewport_width();
  const int view_height = get_viewport_height();
  const int samples = std::min(aa_samples_, max_aa_samples_);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[COLOR]);
  GL::set_object_label(GL_RENDERBUFFER, render_buffers_[COLOR], "sceneColor");

  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB8,
                                   view_width, view_height);

  glBindRenderbuffer(GL_RENDERBUFFER, render_buffers_[DEPTH]);
  GL::set_object_label(GL_RENDERBUFFER, render_buffers_[DEPTH], "sceneDepth");

  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24,
                                   view_width, view_height);
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
  if (render_buffers_[COLOR] || render_buffers_[DEPTH])
  {
    glDeleteRenderbuffers(G_N_ELEMENTS(render_buffers_), render_buffers_);
    render_buffers_[COLOR] = 0;
    render_buffers_[DEPTH] = 0;
  }
}

void Scene::gl_update_focus_state()
{
  const float intensity = (has_focus()) ? 1.f : 0.75f;
  glUniform1f(label_uf_intensity_, intensity);
}

void Scene::gl_update_layouts()
{
  if (!layouts_.vao)
    return;

  if (layouts_.needs_texture_update())
  {
    if (!layouts_.layout_context)
    {
      // Create a dummy cairo context with surface type and transformation
      // matching what we are going to use at draw time.
      const auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_A8, 1, 1);
      const auto cairo = Cairo::Context::create(surface);

      auto context = create_pango_context();
      context->update_from_cairo_context(cairo);

      layouts_.layout_context = std::move(context);
    }
    layouts_.gl_update_texture();
  }

  if (layouts_.needs_vertex_update())
    layouts_.gl_generate_vertices(get_viewport_width(), get_viewport_height());
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
