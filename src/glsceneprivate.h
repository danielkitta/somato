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

#ifndef SOMATO_GLSCENEPRIVATE_H_INCLUDED
#define SOMATO_GLSCENEPRIVATE_H_INCLUDED

#include "glscene.h"
#include "glutils.h"

#include <glibmm/ustring.h>
#include <pangomm/context.h>
#include <pangomm/layout.h>

namespace GL
{

class LayoutTexView
{
private:
  friend class GL::LayoutAtlas;

  Glib::ustring content_;       // text content of layout
  Packed4u8     color_;         // text foreground color

  bool          text_changed_;  // flag to indicate change of content
  bool          attr_changed_;  // flag to indicate change of attributes

  int           x_offset_;      // horizontal offset in atlas texture

  int           ink_x_;         // horizontal offset from logical origin to texture origin
  int           ink_y_;         // vertical offset from logical origin to texture origin
  int           ink_width_;     // width of the drawn part of the layout
  int           ink_height_;    // height of the drawn part of the layout

  int           log_width_;     // logical width of layout
  int           log_height_;    // logical height of layout

  int           window_x_;      // window x coordinate of the layout's logical origin
  int           window_y_;      // window y coordinate of the layout's logical origin

  // noncopyable
  LayoutTexView(const LayoutTexView&) = delete;
  LayoutTexView& operator=(const LayoutTexView&) = delete;

public:
  enum { TRIANGLE_COUNT = 2, VERTEX_COUNT = 4, INDEX_COUNT = 6 };

  LayoutTexView();
  ~LayoutTexView();

  void set_content(Glib::ustring content);
  Glib::ustring get_content() const { return content_; }

  Packed4u8 get_color() const { return color_; }
  void set_color(Packed4u8 color) { color_ = color; attr_changed_ = true; }
  void set_color(float r, float g, float b, float a = 1.)
    { set_color(pack_4u8_norm(r, g, b, a)); }

  void invalidate() { text_changed_ = true; }
  bool need_update() const { return (text_changed_ || attr_changed_); }
  bool drawable() const { return (ink_width_ > 0 && !need_update()); }

  int get_width()  const { return log_width_;  }
  int get_height() const { return log_height_; }

  void set_window_pos(int x, int y);

  int get_window_x() const { return window_x_; }
  int get_window_y() const { return window_y_; }
};

} // namespace GL

#endif /* SOMATO_GLSCENEPRIVATE_H_INCLUDED */
