/*
 * Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "gltextlayout.h"
#include "mathutils.h"

#include <glib.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <epoxy/gl.h>

#include <cstddef>
#include <algorithm>
#include <memory>
#include <utility>

namespace
{

/* Text layout vertex format.
 */
struct LayoutVertex
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

/* Text layout element index type.
 */
using LayoutIndex = GLushort;

/* Index usage convention for arrays of buffer objects.
 */
enum
{
  VERTICES = 0,
  INDICES  = 1
};

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

/* Texture atlas tile dimensions for 8 bit per texel. Note that the tile
 * width is expected to be an integer multiple of the Cairo image surface
 * row alignment, so that it always equals the row stride.
 * The specified dimensions match the tile size used by Intel hardware.
 */
enum
{
  TILE_WIDTH  = 128,
  TILE_HEIGHT = 32
};

/* Element counts per text layout item.
 */
enum
{
  ITEM_PRIMITIVES = 2,
  ITEM_VERTICES   = 4,
  ITEM_INDICES    = 6
};

/* Ink spill margins and padding between adjacent sub-images.
 */
enum : int
{
  MARGIN  = 1,
  PADDING = 1
};

/* Text intensity without and with focus.
 */
const GLfloat focus_intensity[] = { 0.6, 1. };

} // anonymous namespace

namespace GL
{

TextLayoutAtlas::TextLayoutAtlas()
{}

TextLayoutAtlas::~TextLayoutAtlas()
{
  g_warn_if_fail(!buffers_[VERTICES] && !buffers_[INDICES] && !tex_name_ && !vao_);
}

void TextLayoutAtlas::set_layout_count(unsigned int count)
{
  g_return_if_fail(!vao_);

  items_.resize(count);
}

void TextLayoutAtlas::set_layout_text(unsigned int idx, Glib::ustring text)
{
  g_return_if_fail(idx < items_.size());
  TextLayout& item = items_[idx];

  if (text.raw() != item.content.raw())
  {
    item.content = std::move(text);

    if (item.content.empty())
    {
      item.ink_width  = 0;
      item.ink_height = 0;

      need_repos_ = true; // rebuild vertices only
    }
    else
      need_repaint_ = true; // full texture repaint
  }
}

void TextLayoutAtlas::set_layout_color(unsigned int idx, Packed4u8 color)
{
  g_return_if_fail(idx < items_.size());
  TextLayout& item = items_[idx];

  if (color != item.color)
  {
    item.color  = color;
    need_repos_ = true;
  }
}

void TextLayoutAtlas::set_layout_pos(unsigned int idx, TextLayout::Anchor anchor, int x, int y)
{
  g_return_if_fail(idx < items_.size());
  TextLayout& item = items_[idx];

  if (anchor != item.anchor || x != item.pos_x || y != item.pos_y)
  {
    item.anchor = anchor;
    item.pos_x  = x;
    item.pos_y  = y;
    need_repos_ = true;
  }
}

void TextLayoutAtlas::set_pango_context(Glib::RefPtr<Pango::Context> context)
{
  // Create a dummy cairo context with surface type and transformation
  // matching what we are going to use at draw time.
  const auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_A8, 1, 1);
  context->update_from_cairo_context(Cairo::Context::create(surface));

  context_ = std::move(context);
  need_repaint_ = true;
}

void TextLayoutAtlas::unset_pango_context()
{
  context_.reset();
}

void TextLayoutAtlas::gl_init()
{
  gl_create_shader();
  gl_create_texture();
  gl_create_array();

  shader_.use();
  glUniform1i(uf_texture_, SAMPLER_LAYOUT);
  glUniform1fv(uf_intensity_, 1, &focus_intensity[had_focus_]);

  need_repos_ = need_repaint_ = !items_.empty();

  for (TextLayout& item : items_)
  {
    item.ink_width  = 0;
    item.ink_height = 0;
  }
}

void TextLayoutAtlas::gl_delete()
{
  draw_count_ = 0;

  if (vao_)
  {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (buffers_[VERTICES] | buffers_[INDICES])
  {
    glDeleteBuffers(G_N_ELEMENTS(buffers_), buffers_);
    buffers_[VERTICES] = 0;
    buffers_[INDICES]  = 0;
  }
  if (tex_name_)
  {
    glDeleteTextures(1, &tex_name_);
    tex_name_ = 0;
  }
  uf_texture_   = -1;
  uf_intensity_ = -1;
  shader_.reset();
}

void TextLayoutAtlas::gl_update(int view_width, int view_height)
{
  g_return_if_fail(context_);

  if (need_repaint_)
    gl_update_texture();

  if (need_repos_)
    gl_update_vertices(view_width, view_height);
}

int TextLayoutAtlas::gl_draw_layouts(bool has_focus)
{
  shader_.use();

  if (has_focus != had_focus_)
  {
    had_focus_ = has_focus;
    glUniform1fv(uf_intensity_, 1, &focus_intensity[has_focus]);
  }
  if (draw_count_ > 0)
  {
    glBindVertexArray(vao_);
    glDrawRangeElements(GL_TRIANGLES, 0, ITEM_VERTICES * draw_count_ - 1,
                        ITEM_INDICES * draw_count_, GL::attrib_type<LayoutIndex>,
                        GL::buffer_offset<LayoutIndex>(0));
  }
  return draw_count_ * ITEM_PRIMITIVES;
}

void TextLayoutAtlas::gl_create_shader()
{
  GL::ShaderProgram program;
  program.set_label("textlabel");

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/textlabel.vert"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/textlabel.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_TEXCOORD, "texcoord");
  program.bind_attrib_location(ATTRIB_COLOR,    "color");
  program.link();

  uf_texture_   = program.get_uniform_location("labelTexture");
  uf_intensity_ = program.get_uniform_location("textIntensity");

  shader_ = std::move(program);
}

void TextLayoutAtlas::gl_create_texture()
{
  g_return_if_fail(!tex_name_);

  tex_width_  = 0;
  tex_height_ = 0;

  glActiveTexture(GL_TEXTURE0 + SAMPLER_LAYOUT);

  glGenTextures(1, &tex_name_);
  GL::Error::throw_if_fail(tex_name_ != 0);

  glBindTexture(GL_TEXTURE_2D, tex_name_);

  GL::set_object_label(GL_TEXTURE, tex_name_, "layoutAtlas");

  const GLenum clamp_mode = (GL::extensions().texture_border_clamp)
                            ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp_mode);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp_mode);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void TextLayoutAtlas::gl_create_array()
{
  g_return_if_fail(!vao_ && !buffers_[VERTICES] && !buffers_[INDICES]);

  draw_count_ = 0;

  glGenVertexArrays(1, &vao_);
  GL::Error::throw_if_fail(vao_ != 0);

  glGenBuffers(G_N_ELEMENTS(buffers_), buffers_);
  GL::Error::throw_if_fail(buffers_[VERTICES] && buffers_[INDICES]);

  glBindVertexArray(vao_);
  GL::set_object_label(GL_VERTEX_ARRAY, vao_, "layoutsArray");

  glBindBuffer(GL_ARRAY_BUFFER, buffers_[VERTICES]);
  GL::set_object_label(GL_BUFFER, buffers_[VERTICES], "layoutVertices");

  glBufferData(GL_ARRAY_BUFFER,
               items_.size() * ITEM_VERTICES * sizeof(LayoutVertex),
               nullptr, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(ATTRIB_POSITION,
                        GL::attrib_size<decltype(LayoutVertex::position)>,
                        GL::attrib_type<decltype(LayoutVertex::position)>,
                        GL_FALSE, sizeof(LayoutVertex),
                        GL::buffer_offset(offsetof(LayoutVertex, position)));
  glVertexAttribPointer(ATTRIB_TEXCOORD,
                        GL::attrib_size<decltype(LayoutVertex::texcoord)>,
                        GL::attrib_type<decltype(LayoutVertex::texcoord)>,
                        GL_TRUE, sizeof(LayoutVertex),
                        GL::buffer_offset(offsetof(LayoutVertex, texcoord)));
  glVertexAttribPointer(ATTRIB_COLOR,
                        GL::attrib_size<decltype(LayoutVertex::color)>,
                        GL::attrib_type<decltype(LayoutVertex::color)>,
                        GL_TRUE, sizeof(LayoutVertex),
                        GL::buffer_offset(offsetof(LayoutVertex, color)));

  glEnableVertexAttribArray(ATTRIB_POSITION);
  glEnableVertexAttribArray(ATTRIB_TEXCOORD);
  glEnableVertexAttribArray(ATTRIB_COLOR);

  gl_create_indices();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TextLayoutAtlas::gl_create_indices()
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers_[INDICES]);
  GL::set_object_label(GL_BUFFER, buffers_[INDICES], "layoutIndices");

  const auto indices = std::make_unique<LayoutIndex[]>(items_.size() * ITEM_INDICES);

  // Generate a triangle pair for each text layout item.
  for (std::size_t i = 0; i < items_.size(); ++i)
  {
    indices[ITEM_INDICES*i + 0] = ITEM_VERTICES*i + 0;
    indices[ITEM_INDICES*i + 1] = ITEM_VERTICES*i + 1;
    indices[ITEM_INDICES*i + 2] = ITEM_VERTICES*i + 2;
    indices[ITEM_INDICES*i + 3] = ITEM_VERTICES*i + 3;
    indices[ITEM_INDICES*i + 4] = ITEM_VERTICES*i + 2;
    indices[ITEM_INDICES*i + 5] = ITEM_VERTICES*i + 1;
  }
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               items_.size() * ITEM_INDICES * sizeof(LayoutIndex),
               indices.get(), GL_STATIC_DRAW);
}

void TextLayoutAtlas::gl_update_texture()
{
  std::vector<Glib::RefPtr<Pango::Layout>> layouts;
  layouts.reserve(items_.size());

  int img_width  = 0;
  int img_height = 0;

  for (TextLayout& item : items_)
  {
    layouts.push_back(create_pango_layout(item));

    if (layouts.back())
    {
      if (item.tex_y != img_height)
      {
        item.tex_y  = img_height;
        need_repos_ = true;
      }
      img_height += item.ink_height + PADDING;
      img_width = std::max(img_width, item.ink_width);
    }
  }
  if (img_height <= PADDING)
  {
    draw_count_   = 0;
    need_repaint_ = false;
    need_repos_   = false;
    return;
  }
  // Remove the padding overshoot before adding alignment.
  img_height = Math::align(img_height - PADDING, TILE_HEIGHT);
  img_width  = Math::align(img_width, TILE_WIDTH);

  // Create a Cairo surface for the texture image. Note that the image will be
  // upside-down from the point of view of OpenGL, thus the texture coordinates
  // need to be adjusted accordingly.
  const auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_A8,
                                                   img_width, img_height);
  {
    const auto cairo = Cairo::Context::create(surface);
    cairo->set_operator(Cairo::OPERATOR_SOURCE);

    for (std::size_t i = 0; i < layouts.size(); ++i)
      if (const auto& layout = layouts[i])
      {
        const TextLayout& item = items_[i];

        cairo->move_to(item.surface_x, item.tex_y + item.surface_y);
        layout->show_in_cairo_context(cairo);
      }
  }
  surface->flush();

  glActiveTexture(GL_TEXTURE0 + SAMPLER_LAYOUT);

  g_return_if_fail(tex_name_);
  glBindTexture(GL_TEXTURE_2D, tex_name_);

  if (tex_width_ != img_width || tex_height_ != img_height)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, img_width, img_height,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    tex_width_  = img_width;
    tex_height_ = img_height;
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img_width, img_height,
                  GL_RED, GL_UNSIGNED_BYTE, surface->get_data());

  need_repaint_ = false;
}

void TextLayoutAtlas::gl_update_vertices(int view_width, int view_height)
{
  draw_count_ = 0;

  const std::size_t count = std::count_if(cbegin(items_), cend(items_),
                                          [](const TextLayout& t) { return t.valid(); });
  if (count == 0)
  {
    need_repos_ = false;
    return;
  }
  g_return_if_fail(tex_width_ > 0 && tex_height_ > 0);

  g_return_if_fail(buffers_[VERTICES]);
  glBindBuffer(GL_ARRAY_BUFFER, buffers_[VERTICES]);

  const bool ok = access_mapped_buffer(GL_ARRAY_BUFFER, 0,
                                       count * ITEM_VERTICES * sizeof(LayoutVertex),
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
                                       [=](volatile void* data)
  {
    const float scale_s = 0.5f / tex_width_;
    const float scale_t = 0.5f / tex_height_;
    const float scale_x = 1.f / view_width;
    const float scale_y = 1.f / view_height;

    // Shift coordinates to center of 2x2 block for texture gather.
    const int shadow_offset = (GL::extensions().texture_gather) ? -1 : -2;

    // Shift coordinates into normalized [-1, 1] range (reversed in shader).
    const int s_offset = shadow_offset - tex_width_;
    const int t_offset = shadow_offset - tex_height_;

    auto* pv = static_cast<volatile LayoutVertex*>(data);

    for (const TextLayout& item : items_)
    {
      if (!item.valid())
        continue;

      const int width  = item.ink_width  + 1;
      const int height = item.ink_height + 1;

      const float s0 = scale_s * (s_offset);
      const float s1 = scale_s * (s_offset + 2 * width);
      const float t0 = scale_t * (2 * item.tex_y + t_offset + 2 * height);
      const float t1 = scale_t * (2 * item.tex_y + t_offset);

      const int view_x = 2 * (item.origin_x() + item.ink_x) - view_width;
      const int view_y = 2 * (item.origin_y() + item.ink_y) - view_height;

      const float x0 = scale_x * (view_x);
      const float x1 = scale_x * (view_x + 2 * width);
      const float y0 = scale_y * (view_y);
      const float y1 = scale_y * (view_y + 2 * height);

      const auto color = item.color;

      pv[0].set(x0, y0, pack_2i16_norm(s0, t0), color);
      pv[1].set(x1, y0, pack_2i16_norm(s1, t0), color);
      pv[2].set(x0, y1, pack_2i16_norm(s0, t1), color);
      pv[3].set(x1, y1, pack_2i16_norm(s1, t1), color);

      pv += ITEM_VERTICES;
    }
  });

  if (ok)
  {
    draw_count_ = count;
    need_repos_ = false;
  }
}

Glib::RefPtr<Pango::Layout> TextLayoutAtlas::create_pango_layout(TextLayout& item)
{
  if (item.content.empty())
    return {};

  auto layout = Pango::Layout::create(context_);
  layout->set_text(item.content);

  Pango::Rectangle ink, logical;
  // Measure ink extents to determine the dimensions of the image, but
  // keep the logical extents and the ink offsets around for positioning.
  layout->get_pixel_extents(ink, logical);

  // Make sure the extents are within reasonable boundaries.
  g_return_val_if_fail(ink.get_width() < 4095 && ink.get_height() < 4095, layout);

  item.surface_x = MARGIN - ink.get_x();
  item.surface_y = MARGIN - ink.get_y();

  const int ink_x = ink.get_x() - logical.get_x() - MARGIN;
  const int ink_y = logical.get_y() + logical.get_height()
                  - ink.get_y() - ink.get_height() - MARGIN;

  const int ink_width  = std::max(0, ink.get_width())  + 2 * MARGIN;
  const int ink_height = std::max(0, ink.get_height()) + 2 * MARGIN;

  // Expand the logical rectangle to account for the shadow offset.
  const int log_width  = logical.get_width()  + 1;
  const int log_height = logical.get_height() + 1;

  // Compare to previous dimensions to minimize vertex buffer updates.
  if ((ink_x ^ item.ink_x) | (ink_y ^ item.ink_y)
      | (ink_width ^ item.ink_width) | (ink_height ^ item.ink_height)
      | (log_width ^ item.log_width) | (log_height ^ item.log_height))
  {
    item.ink_x = ink_x;
    item.ink_y = ink_y;
    item.ink_width  = ink_width;
    item.ink_height = ink_height;
    item.log_width  = log_width;
    item.log_height = log_height;

    need_repos_ = true;
  }
  return std::move(layout);
}

} // namespace GL
