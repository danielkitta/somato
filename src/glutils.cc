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

#define GL_GLEXT_PROTOTYPES 1
#define GLX_GLXEXT_PROTOTYPES 1

#include "glutils.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkgl.h>
#include <gtk/gtkgl.h>
#include <gtk/gtkwidget.h>
#include <glibmm/convert.h>
#include <gtkmm/widget.h>
#include <array>
#include <cstring>

#ifdef GDK_WINDOWING_WIN32
# include <gdk/win32/gdkglwin32.h>
#endif
#include <GL/gl.h>
#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkglx.h> /* include last as it pulls in whacky X headers */
# include <GL/glx.h>
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

#ifdef GDK_WINDOWING_X11
static
GQuark quark_fbconfig()
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string("somato-fbconfig");

  return quark;
}

static
void dump_glx_fbconfig(Display* display, GLXFBConfig config)
{
  int fbconfig_id    = 0;
  int red_size       = 0;
  int green_size     = 0;
  int blue_size      = 0;
  int alpha_size     = 0;
  int depth_size     = 0;
  int stencil_size   = 0;
  int doublebuffer   = False;
  int stereo         = False;
  int sample_buffers = 0;
  int samples        = 0;

  glXGetFBConfigAttrib(display, config, GLX_FBCONFIG_ID,    &fbconfig_id);
  glXGetFBConfigAttrib(display, config, GLX_RED_SIZE,       &red_size);
  glXGetFBConfigAttrib(display, config, GLX_GREEN_SIZE,     &green_size);
  glXGetFBConfigAttrib(display, config, GLX_BLUE_SIZE,      &blue_size);
  glXGetFBConfigAttrib(display, config, GLX_ALPHA_SIZE,     &alpha_size);
  glXGetFBConfigAttrib(display, config, GLX_DEPTH_SIZE,     &depth_size);
  glXGetFBConfigAttrib(display, config, GLX_STENCIL_SIZE,   &stencil_size);
  glXGetFBConfigAttrib(display, config, GLX_DOUBLEBUFFER,   &doublebuffer);
  glXGetFBConfigAttrib(display, config, GLX_STEREO,         &stereo);
  glXGetFBConfigAttrib(display, config, GLX_SAMPLE_BUFFERS, &sample_buffers);
  glXGetFBConfigAttrib(display, config, GLX_SAMPLES,        &samples);

  g_print("FBConfig %3d: "
          "RGBA=%d:%d:%d:%d, depth=%d, stencil=%d, double=%d, stereo=%d, "
          "sbuffers=%d, samples=%d\n",
          fbconfig_id,
          red_size, green_size, blue_size, alpha_size,
          depth_size, stencil_size, doublebuffer, stereo,
          sample_buffers, samples);
}

static
GdkGLConfig* create_glx_fbconfig(GdkScreen* screen, GdkGLConfigMode mode)
{
  int  attrlist[24];
  int* pattr = attrlist;

  *pattr++ = GLX_RED_SIZE;
  *pattr++ = 4;
  *pattr++ = GLX_GREEN_SIZE;
  *pattr++ = 4;
  *pattr++ = GLX_BLUE_SIZE;
  *pattr++ = 4;

  if ((mode & GDK_GL_MODE_ALPHA) != 0)
  {
    *pattr++ = GLX_ALPHA_SIZE;
    *pattr++ = 4;
  }
  if ((mode & GDK_GL_MODE_DEPTH) != 0)
  {
    *pattr++ = GLX_DEPTH_SIZE;
    *pattr++ = 16;
  }
  if ((mode & GDK_GL_MODE_STENCIL) != 0)
  {
    *pattr++ = GLX_STENCIL_SIZE;
    *pattr++ = 4;
  }
  if ((mode & GDK_GL_MODE_DOUBLE) != 0)
  {
    *pattr++ = GLX_DOUBLEBUFFER;
    *pattr++ = True;
  }
  if ((mode & GDK_GL_MODE_STEREO) != 0)
  {
    *pattr++ = GLX_STEREO;
    *pattr++ = True;
  }
  if ((mode & GDK_GL_MODE_MULTISAMPLE) != 0)
  {
    *pattr++ = GLX_SAMPLE_BUFFERS;
    *pattr++ = 1;
    *pattr++ = GLX_SAMPLES;
    *pattr++ = 4;
  }
  *pattr = None;

  int n_fbconfigs = 0;
  GLXFBConfig *const fbconfigs = glXChooseFBConfig(GDK_SCREEN_XDISPLAY(screen),
                                                   GDK_SCREEN_XNUMBER(screen),
                                                   attrlist, &n_fbconfigs);
  g_return_val_if_fail(fbconfigs != nullptr && n_fbconfigs > 0, nullptr);

  g_print("Framebuffer configurations offered:\n");

  for (int i = 0; i < n_fbconfigs; ++i)
    dump_glx_fbconfig(GDK_SCREEN_XDISPLAY(screen), fbconfigs[i]);

  const GLXFBConfig fbconfig = fbconfigs[0];
  XFree(fbconfigs);

  int visual_id = -1;
  int result = glXGetFBConfigAttrib(GDK_SCREEN_XDISPLAY(screen), fbconfig,
                                    GLX_VISUAL_ID, &visual_id);
  g_return_val_if_fail(result == Success && visual_id >= 0, nullptr);

  GdkGLConfig *const config =
    gdk_x11_gl_config_new_from_visualid_for_screen(screen, visual_id);

  g_object_set_qdata(G_OBJECT(config), quark_fbconfig(),
                     reinterpret_cast<void*>(fbconfig));
  return config;
}

static
unsigned int debug_flag_if_enabled()
{
  const char *const messages_debug = g_getenv("G_MESSAGES_DEBUG");
  const GDebugKey debug_key {"OpenGL", GLX_CONTEXT_DEBUG_BIT_ARB};

  return g_parse_debug_string(messages_debug, &debug_key, 1) & GLX_CONTEXT_DEBUG_BIT_ARB;
}

static
GdkGLContext* create_glx_core_context(GdkGLConfig* config)
{
  const auto CreateContextAttribs = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>
    (gdk_gl_get_proc_address("glXCreateContextAttribsARB"));

  g_return_val_if_fail(CreateContextAttribs != nullptr, nullptr);

  const auto fbconfig = reinterpret_cast<GLXFBConfig>
    (g_object_get_qdata(G_OBJECT(config), quark_fbconfig()));

  g_return_val_if_fail(fbconfig, nullptr);

  Display *const xdisplay = gdk_x11_gl_config_get_xdisplay(config);

  int attribs[10];

  attribs[0] = GLX_CONTEXT_MAJOR_VERSION_ARB;
  attribs[1] = 3;
  attribs[2] = GLX_CONTEXT_MINOR_VERSION_ARB;
  attribs[3] = 2;
  attribs[4] = GLX_CONTEXT_FLAGS_ARB;
  attribs[5] = debug_flag_if_enabled();
  attribs[6] = GLX_CONTEXT_PROFILE_MASK_ARB;
  attribs[7] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
  attribs[8] = None;
  attribs[9] = 0;

  const GLXContext glx_context =
    (*CreateContextAttribs)(xdisplay, fbconfig, 0, True, attribs);

  g_return_val_if_fail(glx_context, nullptr);

  return gdk_x11_gl_context_foreign_new(config, nullptr, glx_context);
}
#endif /* GDK_WINDOWING_X11 */

static
Glib::ustring error_message_from_code(unsigned int error_code)
{
  const char* message = "unknown error";

  switch (error_code)
  {
    case GL_NO_ERROR:
      message = "no error";
      break;
    case GL_INVALID_ENUM:
      message = "invalid enumerant";
      break;
    case GL_INVALID_VALUE:
      message = "invalid value";
      break;
    case GL_INVALID_OPERATION:
      message = "invalid operation";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      message = "invalid framebuffer operation";
      break;
    case GL_OUT_OF_MEMORY:
      message = "out of memory";
      break;
  }
  return Glib::ustring{message};
}

static
Glib::ustring framebuffer_message_from_code(unsigned int status_code)
{
  const char* message = "unknown status";

  switch (status_code)
  {
    case GL_FRAMEBUFFER_COMPLETE:
      message = "framebuffer complete";
      break;
    case GL_FRAMEBUFFER_UNDEFINED:
      message = "framebuffer undefined";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      message = "incomplete attachment";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      message = "missing attachment";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      message = "no draw buffer";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      message = "no read buffer";
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      message = "unsupported configuration";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      message = "inconsistent multisample setup";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
      message = "inconsistent layer targets";
      break;
  }
  return Glib::ustring{message};
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
  what_ {error_message_from_code(error_code)},
  code_ {error_code}
{}

GL::Error::Error(const Glib::ustring& message)
:
  what_ {message},
  code_ {0}
{}

GL::Error::Error(const Glib::ustring& message, unsigned int error_code)
:
  what_ {message},
  code_ {error_code}
{}

GL::Error::~Error() noexcept
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
    throw GL::Error{error_code};
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
// static
void GL::Error::fail()
{
  check();

  throw GL::Error{"operation failed without error code"};
}

GL::FramebufferError::FramebufferError(unsigned int error_code)
:
  GL::Error{framebuffer_message_from_code(error_code), error_code}
{}

GL::FramebufferError::~FramebufferError() noexcept
{}

void GL::configure_widget(Gtk::Widget& target, unsigned int mode)
{
  GtkWidget *const widget = target.gobj();
  GdkScreen *const screen = gtk_widget_get_screen(widget);

#if defined(GDK_WINDOWING_WIN32)
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
#elif defined(GDK_WINDOWING_X11)

  GdkGLConfig* config = create_glx_fbconfig(screen, GdkGLConfigMode(mode));

#else /* !GDK_WINDOWING_X11 */

  GdkGLConfig* config = gdk_gl_config_new_by_mode_for_screen(screen, GdkGLConfigMode(mode));

  // If no double-buffered visual is available, try a single-buffered one.
  if (!config && (mode & GDK_GL_MODE_DOUBLE) != 0)
  {
    config = gdk_gl_config_new_by_mode_for_screen(screen,
                 GdkGLConfigMode(mode & ~unsigned(GDK_GL_MODE_DOUBLE)));
  }
#endif /* !GDK_WINDOWING_X11 */

  if (!config)
    throw GL::Error{"could not find OpenGL-capable visual"};

  const gboolean success = gtk_widget_set_gl_capability(widget, config, nullptr,
                                                        TRUE, GDK_GL_RGBA_TYPE);
  g_object_unref(config);

  if (!success)
    throw GL::Error{"could not set GL capability on widget"};
}

GdkGLContext* GL::create_context(GdkGLDrawable* drawable)
{
  GdkGLContext* context;

#ifdef GDK_WINDOWING_X11
  GdkGLConfig *const config = gdk_gl_drawable_get_gl_config(drawable);

  if (gdk_x11_gl_query_glx_extension(config, "GLX_ARB_create_context_profile"))
    context = create_glx_core_context(config);
  else
#endif
    context = gdk_gl_context_new(drawable, nullptr, TRUE, GDK_GL_RGBA_TYPE);

  g_return_val_if_fail(context != nullptr, nullptr);

  return context;
}

void GL::destroy_context(GdkGLContext* context)
{
#ifdef GDK_WINDOWING_X11
  Display* xdisplay = nullptr;
  GLXContext glx_context = 0;

  if (GdkGLConfig *const config = gdk_gl_context_get_gl_config(context))
    if (gdk_x11_gl_query_glx_extension(config, "GLX_ARB_create_context_profile"))
    {
      xdisplay = gdk_x11_gl_config_get_xdisplay(config);
      glx_context = gdk_x11_gl_context_get_glxcontext(context);
    }
#endif
  g_object_unref(context);

#ifdef GDK_WINDOWING_X11
  if (glx_context)
  {
    g_return_if_fail(xdisplay != nullptr);
    glXDestroyContext(xdisplay, glx_context);
  }
#endif
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
        return GL::make_version(major, minor);
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

bool GL::have_gl_extension(const char* name)
{
  g_return_val_if_fail(name != nullptr, false);

  GLint num_extensions = -1;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

  if (num_extensions < 0)
    GL::Error::check();

  for (int i = 0; i < num_extensions; ++i)
  {
    const GLubyte *const ext = glGetStringi(GL_EXTENSIONS, i);

    if (ext && std::strcmp(reinterpret_cast<const char*>(ext), name) == 0)
      return true;
  }
  return false;
}

#ifdef GDK_WINDOWING_X11

bool GL::have_glx_extension(const char* name)
{
  GdkGLDrawable *const drawable = gdk_gl_drawable_get_current();

  g_return_val_if_fail(drawable != nullptr, false);

  return (gdk_x11_gl_query_glx_extension(gdk_gl_drawable_get_gl_config(drawable), name) != FALSE);
}

#endif /* GDK_WINDOWING_X11 */

#ifdef GDK_WINDOWING_WIN32

bool GL::have_wgl_extension(const char* name)
{
  GdkGLDrawable *const drawable = gdk_gl_drawable_get_current();
  
  g_return_val_if_fail(drawable != nullptr, false);

  return (gdk_win32_gl_query_wgl_extension(gdk_gl_drawable_get_gl_config(drawable), name) != FALSE);
}

#endif /* GDK_WINDOWING_WIN32 */

GL::ProcAddress GL::get_proc_address_(const char* name)
{
  return gdk_gl_get_proc_address(name);
}
