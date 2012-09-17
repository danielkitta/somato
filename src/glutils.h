/*
 * Copyright (c) 2004-2007  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_GLUTILS_H_INCLUDED
#define SOMATO_GLUTILS_H_INCLUDED

#include <glib.h>
#include <gdk/gdk.h>
#include <glibmm/ustring.h>
#include <cstddef>

extern "C"
{
  typedef struct _GdkGLContext  GdkGLContext;
  typedef struct _GdkGLDrawable GdkGLDrawable;
}

#ifndef SOMATO_HIDE_FROM_INTELLISENSE
namespace Gtk { class Widget; }
#endif

namespace GL
{

extern "C" { typedef void (* ProcAddress) (void); }

/*
 * Exception class for errors reported by glGetError().  To simplify matters,
 * GL::Error is also thrown by GL::configure_widget() and the constructor of
 * GL::Scene::ScopeContext to indicate failure.  For this usage code() is not
 * meaningful and will always be 0.
 */
class Error
{
private:
  Glib::ustring what_;
  unsigned int  code_;

public:
  explicit Error(unsigned int error_code);
  explicit Error(const Glib::ustring& message);
  virtual ~Error() noexcept;

  Error(const Error& other) : what_ {other.what_}, code_ {other.code_} {}
  Error& operator=(const Error& other) { what_ = other.what_; code_ = other.code_; return *this; }

  unsigned int  code() const { return code_; }
  Glib::ustring what() const { return what_; }

  static void check();                // throw if glGetError() != GL_NO_ERROR
  static void fail() G_GNUC_NORETURN; // like check() but always throws

  static void throw_if_fail(bool condition) { if (G_UNLIKELY(!condition)) fail(); }

protected:
  Error(const Glib::ustring& message, unsigned int error_code);
};

class FramebufferError : public Error
{
public:
  explicit FramebufferError(unsigned int error_code);
  virtual ~FramebufferError() noexcept;
};

/*
 * Try to enable the OpenGL capability on the specified widget.
 * This function should be called after the widget has been added
 * to a toplevel window.  GL::Error is thrown on failure.
 */
void configure_widget(Gtk::Widget& target, unsigned int mode);

GdkGLContext* create_context(GdkGLDrawable* drawable);
void destroy_context(GdkGLContext* context);

/*
 * Combine major and minor version number parts into a single
 * integer for use with the GL version check functions below.
 */
inline int make_version(int major, int minor)
{
  return (major << 16) + minor;
}

/*
 * Parse the version string and return the version number encoded as
 * integer of the form (major << 16) + minor, or -1 if parsing failed.
 * The format of the string should match glGetString(GL_VERSION).
 */
int parse_version_string(const unsigned char* version);

/*
 * Parse the string returned by glGetString(GL_VERSION) and return
 * the version number encoded as (major << 16) + minor.  Requires
 * an active GL context.
 */
int get_gl_version();

/*
 * Check for the presence of an OpenGL extension by name.
 * Requires an active GL context.
 */
bool have_gl_extension(const char* name);

#ifdef GDK_WINDOWING_X11
/*
 * Check for the presence of an extension name in the list returned
 * by glXQueryExtensionsString().  Requires an active GL context.
 */
bool have_glx_extension(const char* name);
#endif

#ifdef GDK_WINDOWING_WIN32
/*
 * Check for the presence of an extension name in the list returned
 * by wglGetExtensionsStringARB().  Requires an active GL context.
 */
bool have_wgl_extension(const char* name);
#endif

ProcAddress get_proc_address_(const char* name);

/*
 * Dynamically retrieve the entry point of a GL function.  Requires an active
 * GL context.  Note that it is necessary to parse the GL extension string as
 * well, in order to determine whether an extension is actually supported.
 */
template <class T>
inline T get_proc_address(T& address, const char* name)
{
  return (address = reinterpret_cast<T>(GL::get_proc_address_(name)));
}

/*
 * Convert a VBO offset to a pointer.
 */
inline void* buffer_offset(std::size_t offset)
{
  return static_cast<char*>(0) + offset;
}

} // namespace GL

#endif /* SOMATO_GLUTILS_H_INCLUDED */
