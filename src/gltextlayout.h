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

#ifndef SOMATO_GLTEXTLAYOUT_H_INCLUDED
#define SOMATO_GLTEXTLAYOUT_H_INCLUDED

#include "glshader.h"
#include "glutils.h"

#include <glibmm/ustring.h>
#include <pangomm/context.h>
#include <pangomm/layout.h>

#include <vector>

namespace GL
{

struct TextLayout
{
  enum Anchor : unsigned int { BOTTOM_LEFT, TOP_LEFT, BOTTOM_RIGHT, TOP_RIGHT };

  Glib::ustring content;             // text content
  Packed4u8     color = Packed4u8{}; // text color

  Anchor anchor = BOTTOM_LEFT; // logical rectangle anchor for positioning
  int    pos_x  = 0;           // logical x position within viewport window
  int    pos_y  = 0;           // logical y position within viewport window

  int surface_x  = 0; // x position on Cairo surface
  int surface_y  = 0; // y position on Cairo surface sub-image

  int ink_x      = 0; // x offset from logical origin to ink origin
  int ink_y      = 0; // y offset from logical origin to ink origin
  int ink_width  = 0; // width of ink rectangle
  int ink_height = 0; // height of ink rectangle

  int log_width  = 0; // logical width of layout
  int log_height = 0; // logical height of layout

  int tex_y      = 0; // vertical offset in atlas texture

  int origin_x() const { return pos_x - ((anchor & BOTTOM_RIGHT) ? log_width  : 0); }
  int origin_y() const { return pos_y - ((anchor & TOP_LEFT)     ? log_height : 0); }

  bool valid() const { return (ink_height > 0); }
};

class TextLayoutAtlas
{
public:
  TextLayoutAtlas();
  ~TextLayoutAtlas();

  void set_layout_count(unsigned int count);
  void set_layout_text(unsigned int idx, Glib::ustring text);
  void set_layout_color(unsigned int idx, Packed4u8 color);
  void set_layout_pos(unsigned int idx, TextLayout::Anchor anchor, int x, int y);

  void set_pango_context(Glib::RefPtr<Pango::Context> context);
  void unset_pango_context();
  bool has_pango_context() const { return !!context_; }

  bool update_needed() const { return (need_repaint_ || need_repos_); }
  bool is_drawable() const { return (draw_count_ > 0 && vao_ && shader_); }

  void gl_init();
  void gl_delete();
  void gl_update(int view_width, int view_height);
  int  gl_draw_layouts(bool has_focus);

private:
  void gl_create_shader();
  void gl_create_texture();
  void gl_create_array();
  void gl_create_indices();

  void gl_update_texture();
  void gl_update_vertices(int view_width, int view_height);

  Glib::RefPtr<Pango::Layout> create_pango_layout(TextLayout& item);

  std::vector<TextLayout>       items_;   // list of text layout items
  Glib::RefPtr<Pango::Context>  context_; // Pango context for text layout
  GL::ShaderProgram             shader_;  // text layout shader program object

  int          uf_texture_    = -1;       // texture sampler uniform ID
  int          uf_intensity_  = -1;       // text intensity uniform ID

  unsigned int vao_           = 0;        // vertex array object
  unsigned int buffers_[2]    = {0, 0};   // vertex and index buffer objects
  unsigned int tex_name_      = 0;        // texture ID
  int          tex_width_     = 0;        // layout texture width
  int          tex_height_    = 0;        // layout texture height
  unsigned int draw_count_    = 0;        // number of items ready for drawing

  bool         need_repaint_  = false;    // texture needs to be repainted
  bool         need_repos_    = false;    // vertices need to be regenerated
  bool         had_focus_     = true;     // last remembered focus state
};

} // namespace GL

#endif // !SOMATO_GLTEXTLAYOUT_H_INCLUDED
