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

#include "glutils.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkgl.h>
#include <gtk/gtkgl.h>
#include <gtk/gtkwidget.h>
#include <glibmm/convert.h>
#include <gtkmm/widget.h>
#include <cstring>

#ifdef GDK_WINDOWING_WIN32
# include <gdk/win32/gdkglwin32.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkglx.h> /* include last as it pulls in whacky X headers */
#endif

namespace
{

#ifdef GDK_WINDOWING_WIN32
extern "C"
{
// These are Vista-specific, so we will look them up at runtime.
typedef HRESULT (WINAPI* IsCompositionEnabledFunc)(BOOL*);
typedef HRESULT (WINAPI* EnableMMCSSFunc)(BOOL);
}

/*
 * Helper function of GL::configure_widget().  Set up a win32 pixel
 * format descriptor with a configuration matching the mode argument.
 * Also do a bit of special magic to get it all working nicely on
 * Windows Vista, too.
 */
static
void init_win32_pixel_format(PIXELFORMATDESCRIPTOR& pfd, unsigned int mode)
{
  std::memset(&pfd, 0, sizeof(pfd));

  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.cColorBits = 24;

  if ((mode & GDK_GL_MODE_DEPTH) != 0)
    pfd.cDepthBits = 24;

  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;

  if ((mode & GDK_GL_MODE_DOUBLE) != 0)
    pfd.dwFlags |= PFD_DOUBLEBUFFER;

  if (const HMODULE dwmapi = LoadLibrary(TEXT("dwmapi")))
  {
    const IsCompositionEnabledFunc IsCompositionEnabled =
      reinterpret_cast<IsCompositionEnabledFunc>(GetProcAddress(dwmapi, "DwmIsCompositionEnabled"));
    BOOL compositing = FALSE;
    // Disable double buffering on Vista if composition is enabled -- drawing
    // directly into the composite buffer works much smoother.
    if (IsCompositionEnabled && (*IsCompositionEnabled)(&compositing) == S_OK && compositing)
    {
      pfd.dwFlags = pfd.dwFlags & ~DWORD(PFD_DOUBLEBUFFER) | PFD_SUPPORT_COMPOSITION;

      // While we're at it, let's enable multimedia class scheduling as well,
      // so that we get to enjoy more accurate timeouts and shorter intervals.
      if (const EnableMMCSSFunc EnableMMCSS =
            reinterpret_cast<EnableMMCSSFunc>(GetProcAddress(dwmapi, "DwmEnableMMCSS")))
      {
        (*EnableMMCSS)(TRUE);
      }
    }
    FreeLibrary(dwmapi);
  }
}
#endif /* GDK_WINDOWING_WIN32 */

static
Glib::ustring error_message_from_code(unsigned int error_code)
{
  // According to the manual, the error string is always in ISO Latin 1.
  // Although it is quite unlikely that the message will ever contain any
  // characters outside the basic ASCII range, the code should be able to
  // properly handle that eventuality.

  if (const GLubyte *const message = gluErrorString(error_code))
    return Glib::convert(reinterpret_cast<const char*>(message), "UTF-8", "ISO-8859-1");
  else
    return Glib::ustring();
}

static inline
int parse_version_digits(const unsigned char*& version)
{
  const unsigned char* pos = version;
  int value = 0;

  do
  {
    // This code is meant to work with ASCII only.
    const int digit = *pos - 0x30;

    if (digit < 0 || digit > 9)
      break;

    value = 10 * value + digit;
    ++pos;
  }
  while (value < (1 << 15) / 10); // result must fit into 15 bit

  if (pos == version)
    value = -1;

  version = pos;
  return value;
}

} // anonymous namespace

GL::Error::Error(unsigned int error_code)
:
  what_ (error_message_from_code(error_code)),
  code_ (error_code)
{}

GL::Error::Error(const Glib::ustring& message)
:
  what_ (message),
  code_ (0)
{}

GL::Error::~Error() throw()
{}

/*
 * Inlining these expensive error checking functions is more than just
 * pointless -- in fact it will hurt performance due to the reduction in
 * code locality, and by preventing the inliner from expanding other and
 * likely more rewarding code paths.
 */
#ifdef _MSC_VER
__declspec(noinline)
#endif
// static
void GL::Error::check()
{
  const GLenum error_code = glGetError();

  if (error_code != GL_NO_ERROR)
    throw GL::Error(error_code);
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
// static
void GL::Error::fail()
{
  check();

  throw GL::Error("operation failed without error code");
}

// static
void GL::ScopeList::new_(unsigned int list, unsigned int mode)
{
  glNewList(list, mode);
}

// static
void GL::ScopeList::end_()
{
  glEndList();
}

// static
void GL::ScopeMatrix::push_(unsigned int mode)
{
  glMatrixMode(mode);
  glPushMatrix();
}

// static
void GL::ScopeMatrix::pop_(unsigned int mode)
{
  glMatrixMode(mode);
  glPopMatrix();
}

void GL::configure_widget(Gtk::Widget& target, unsigned int mode)
{
  GtkWidget *const widget = target.gobj();
  GdkScreen *const screen = gtk_widget_get_screen(widget);

#ifdef GDK_WINDOWING_WIN32
  // Sigh... Looks like gdkglext's win32 implementation is completety
  // broken.  The setup logic always opts for the biggest and baddest
  // framebuffer layout available.  Like 64 bits accumulation buffer,
  // four auxiliary buffers, and of course stencil...  Yep, the whole
  // package.  Yikes.
  // Well, it seems I found the cause of the screen update stalls on Vista.
  // Nonwithstanding these superfluous extras, the actual culprit appears
  // to be plain old double buffering.  See the init_win32_pixel_format()
  // helper for more.
  GdkGLConfig* config = 0;
  {
    PIXELFORMATDESCRIPTOR pfd;
    init_win32_pixel_format(pfd, mode);

    GdkWindow *const rootwindow = gdk_screen_get_root_window(screen);
    const HWND windowhandle = reinterpret_cast<HWND>(GDK_WINDOW_HWND(rootwindow));

    if (const HDC devicecontext = GetDC(windowhandle))
    {
      const int pixelformat = ChoosePixelFormat(devicecontext, &pfd);
      ReleaseDC(windowhandle, devicecontext);

      if (pixelformat > 0)
        config = gdk_win32_gl_config_new_from_pixel_format(pixelformat);
    }
  }
#else /* !GDK_WINDOWING_WIN32 */
  GdkGLConfig* config = gdk_gl_config_new_by_mode_for_screen(screen, GdkGLConfigMode(mode));

  // If no double-buffered visual is available, try a single-buffered one.
  if (!config && (mode & GDK_GL_MODE_DOUBLE) != 0)
  {
    config = gdk_gl_config_new_by_mode_for_screen(screen,
                 GdkGLConfigMode(mode & ~unsigned(GDK_GL_MODE_DOUBLE)));
  }
#endif /* !GDK_WINDOWING_WIN32 */

  if (!config)
    throw GL::Error("could not find OpenGL-capable visual");

  const int type = ((mode & GDK_GL_MODE_INDEX) != 0) ? GDK_GL_COLOR_INDEX_TYPE : GDK_GL_RGBA_TYPE;
  const gboolean success = gtk_widget_set_gl_capability(widget, config, 0, TRUE, type);

  g_object_unref(config);

  if (!success)
    throw GL::Error("could not set GL capability on widget");
}

int GL::parse_version_string(const unsigned char* version)
{
  if (version)
  {
    const int major = parse_version_digits(version);

    if (major >= 0 && *version == '.')
    {
      ++version; // skip period

      const int minor = parse_version_digits(version);

      if (minor >= 0 && (*version == '\0' || *version == ' ' || *version == '.'))
      {
        return GL::make_version(major, minor);
      }
    }
  }

  return -1;
}

int GL::get_gl_version()
{
  if (const GLubyte *const version = glGetString(GL_VERSION))
    return GL::parse_version_string(version);
  else
    GL::Error::check();

  return -1;
}

bool GL::parse_extensions_string(const unsigned char* extensions, const char* name)
{
  const std::size_t name_length = (name) ? std::strlen(name) : 0;

  g_return_val_if_fail(name_length > 0, false);

  if (extensions)
  {
    const char* pos = reinterpret_cast<const char*>(extensions);

    while (const char *const space = std::strchr(pos, ' '))
    {
      const std::size_t length = space - pos;

      if (length == name_length && std::memcmp(pos, name, length) == 0)
        return true;

      pos = space + 1;
    }

    return (std::strcmp(pos, name) == 0);
  }

  return false;
}

bool GL::have_gl_extension(const char* name)
{
  if (const GLubyte *const extensions = glGetString(GL_EXTENSIONS))
    return GL::parse_extensions_string(extensions, name);
  else
    GL::Error::check();

  return false;
}

#ifdef GDK_WINDOWING_X11

bool GL::have_glx_extension(const char* name)
{
  GdkGLDrawable *const drawable = gdk_gl_drawable_get_current();

  g_return_val_if_fail(drawable != 0, false);

  return (gdk_x11_gl_query_glx_extension(gdk_gl_drawable_get_gl_config(drawable), name) != 0);
}

#endif /* GDK_WINDOWING_X11 */

#ifdef GDK_WINDOWING_WIN32

bool GL::have_wgl_extension(const char* name)
{
  GdkGLDrawable *const drawable = gdk_gl_drawable_get_current();

  g_return_val_if_fail(drawable != 0, false);

  return (gdk_win32_gl_query_wgl_extension(gdk_gl_drawable_get_gl_config(drawable), name) != 0);
}

#endif /* GDK_WINDOWING_WIN32 */

GL::ProcAddress GL::get_proc_address_(const char* name)
{
  return gdk_gl_get_proc_address(name);
}
