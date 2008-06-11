/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_GLSCENEPRIVATE_H_INCLUDED
#define SOMATO_GLSCENEPRIVATE_H_INCLUDED

#include "glscene.h"
#include "glutils.h"

#include <gdk/gdk.h>
#include <pangomm/context.h>
#include <pangomm/layout.h>

#ifdef GDK_WINDOWING_WIN32
# include <windows.h>
#endif
#include <GL/gl.h>
#ifdef GDK_WINDOWING_WIN32
# include <gdk/glext/glext.h>
# include <gdk/glext/wglext.h>
#endif

#ifdef GDK_WINDOWING_X11
// Avoid including glx.h as that would pull in the macro-jammed X headers.
extern "C" { typedef int (*PFNGLXSWAPINTERVALSGIPROC)(int); }
#endif

namespace GL
{

struct Extensions
{
private:
  const GLubyte*  extensions_;
  int             version_;

  void query();

  // noncopyable
  Extensions(const Extensions&);
  Extensions& operator=(const Extensions&);

public:
  bool have_swap_control;
  bool have_texture_rectangle;
  bool have_texture_border_clamp;
  bool have_multitexture;
  bool have_texture_env_combine;
  bool have_vertex_buffer_object;

#if defined(GDK_WINDOWING_X11)
  PFNGLXSWAPINTERVALSGIPROC     SwapIntervalSGI;
#elif defined(GDK_WINDOWING_WIN32)
  PFNWGLSWAPINTERVALEXTPROC     SwapIntervalEXT;
#endif
  PFNGLACTIVETEXTUREPROC        ActiveTexture;
  PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;

  PFNGLGENBUFFERSARBPROC        GenBuffers;
  PFNGLDELETEBUFFERSARBPROC     DeleteBuffers;
  PFNGLBINDBUFFERARBPROC        BindBuffer;
  PFNGLBUFFERDATAARBPROC        BufferData;

  Extensions() : extensions_ (0), version_ (0) { query(); }
  virtual ~Extensions();

  inline bool have_version(int major, int minor) const
    { return (version_ >= GL::make_version(major, minor)); }

  inline bool have_extension(const char* name) const
    { return GL::parse_extensions_string(extensions_, name); }
};

class LayoutTexture
{
private:
  friend class GL::Scene;

  class Invalidate;

  enum { TRIANGLE_COUNT = 2, VERTEX_COUNT = 4 };

  Glib::ustring content_;       // text content of layout
  GLubyte       color_[3];      // text foreground color
  bool          need_update_;   // flag to indicate change of content

  unsigned int  array_offset_;  // offset into geometry arrays

  GLuint        tex_name_;      // GL name of texture object
  int           tex_width_;     // actual width of the GL texture
  int           tex_height_;    // actual height of the GL texture

  int           ink_x_;         // horizontal offset from logical origin to texture origin
  int           ink_y_;         // vertical offset from logical origin to texture origin
  int           ink_width_;     // width of the drawn part of the layout
  int           ink_height_;    // height of the drawn part of the layout

  int           log_width_;     // logical width of layout
  int           log_height_;    // logical height of layout

  int           window_x_;      // window x coordinate of the layout's logical origin
  int           window_y_;      // window y coordinate of the layout's logical origin

  static void prepare_pango_context(const Glib::RefPtr<Pango::Context>& context);

  void gl_set_layout(const Glib::RefPtr<Pango::Layout>& layout,
                     int img_border, GLenum target, GLenum clamp_mode);
  void gl_delete();

  // noncopyable
  LayoutTexture(const LayoutTexture&);
  LayoutTexture& operator=(const LayoutTexture&);

public:
  class IsDrawable;

  LayoutTexture();
  ~LayoutTexture();

  void set_content(const Glib::ustring& content);
  Glib::ustring get_content() const { return content_; }

  inline void set_color(GLubyte red, GLubyte green, GLubyte blue);
  inline void set_color(const GLubyte* color);

  inline GLubyte*       color()       { return color_; }
  inline const GLubyte* color() const { return color_; }

  inline bool need_update() const { return need_update_; }
  inline bool drawable() const { return (tex_name_ != 0); }

  inline int get_width()  const { return log_width_;  }
  inline int get_height() const { return log_height_; }

  inline void set_window_pos(int x, int y) { window_x_ = x; window_y_ = y; }

  inline int get_window_x() const { return window_x_; }
  inline int get_window_y() const { return window_y_; }
};

class LayoutTexture::Invalidate
{
public:
  typedef LayoutTexture*  argument_type;
  typedef void            result_type;

  inline void operator()(LayoutTexture* layout) const { layout->need_update_ = true; }
};

class LayoutTexture::IsDrawable
{
public:
  typedef const LayoutTexture*  argument_type;
  typedef bool                  result_type;

  inline bool operator()(const LayoutTexture* layout) const { return layout->drawable(); }
};

inline
void LayoutTexture::set_color(GLubyte red, GLubyte green, GLubyte blue)
{
  color_[0] = red;
  color_[1] = green;
  color_[2] = blue;
}

inline
void LayoutTexture::set_color(const GLubyte* color)
{
  set_color(color[0], color[1], color[2]);
}

inline
const GL::Extensions* Scene::gl_ext() const
{
  return gl_extensions_.get();
}

} // namespace GL

#endif /* SOMATO_GLSCENEPRIVATE_H_INCLUDED */
