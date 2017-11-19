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
#include "glutils.h"

#include <algorithm>
#include <epoxy/gl.h>

namespace
{

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
    case GL_OUT_OF_MEMORY:
      message = "out of memory";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      message = "invalid framebuffer operation";
      break;
    case GL_CONTEXT_LOST:
      message = "context lost";
      break;
  }
  return Glib::ustring{message};
}

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

} // anonymous namespace

GL::Extensions GL::Extensions::instance_;

void GL::Extensions::query_(bool use_es, int version)
{
  g_log(GL::log_domain, G_LOG_LEVEL_INFO, "OpenGL version: %s, GLSL version: %s",
        glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

  if (version < ((use_es) ? 0x0300 : 0x0302))
  {
    g_log(GL::log_domain, G_LOG_LEVEL_WARNING,
          "At least OpenGL 3.2 or OpenGL ES 3.0 is required");
  }
  is_gles = use_es;

  debug = (!use_es && version >= 0x0403)
      || epoxy_has_gl_extension("GL_KHR_debug");

  debug_output = debug
      || epoxy_has_gl_extension("GL_ARB_debug_output");

  vertex_type_2_10_10_10_rev = (version >= ((use_es) ? 0x0300 : 0x0303))
      || epoxy_has_gl_extension("GL_ARB_vertex_type_2_10_10_10_rev");

  texture_border_clamp = (!use_es || version >= 0x0302)
      || epoxy_has_gl_extension("GL_OES_texture_border_clamp");

  texture_filter_anisotropic = (!use_es && version >= 0x0406)
      || epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic");

  texture_gather = (version >= ((use_es) ? 0x0301 : 0x0400))
      || epoxy_has_gl_extension("GL_ARB_texture_gather");

  max_anisotropy = 1.;

  if (texture_filter_anisotropic)
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
}

GL::Error::Error(unsigned int error_code)
:
  Gdk::GLError{NOT_AVAILABLE, error_message_from_code(error_code)},
  gl_code_ {error_code}
{}

GL::Error::Error(const Glib::ustring& message, unsigned int error_code)
:
  Gdk::GLError{NOT_AVAILABLE, message},
  gl_code_ {error_code}
{}

GL::Error::~Error() noexcept
{}

void GL::Error::check()
{
  const GLenum error_code = glGetError();

  if (error_code != GL_NO_ERROR)
    throw GL::Error{error_code};
}

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

void* GL::ScopedMapBuffer::map_checked(unsigned int target, std::size_t offset,
                                       std::size_t length, unsigned int access)
{
  void *const data = glMapBufferRange(target, offset, length, access);

  if (!data)
    g_log(GL::log_domain, G_LOG_LEVEL_WARNING, "glMapBufferRange() failed");

  return data;
}

bool GL::ScopedMapBuffer::unmap_checked(unsigned int target)
{
  if (glUnmapBuffer(target))
    return true;

  g_log(GL::log_domain, G_LOG_LEVEL_WARNING, "glUnmapBuffer() failed");
  return false;
}

void GL::tex_image_from_ktx(const guint32* ktx, unsigned int ktx_size)
{
  const unsigned int magic0 = GUINT32_TO_BE(0xAB4B5458);
  const unsigned int magic1 = GUINT32_TO_BE(0x203131BB);
  const unsigned int magic2 = GUINT32_TO_BE(0x0D0A1A0A);
  const unsigned int host_endian = 0x04030201;

  g_return_if_fail(ktx_size > 16);
  g_return_if_fail(ktx[0] == magic0 && ktx[1] == magic1 && ktx[2] == magic2);
  g_return_if_fail(ktx[3] == host_endian);
  g_return_if_fail(ktx[4] == 0 && ktx[5] == 1 && ktx[6] == 0); // compressed?

  const unsigned int int_format  = ktx[7];
  const unsigned int base_width  = ktx[9];
  const unsigned int base_height = ktx[10];
  const unsigned int num_mipmaps = ktx[14];

  g_return_if_fail(base_width > 0 && base_height > 0 && num_mipmaps > 0);
  g_return_if_fail(ktx[11] == 0 && ktx[12] == 0 && ktx[13] == 1); // 2D?

  unsigned int offset = 16 + ktx[15] / 4;

  for (unsigned int level = 0; level < num_mipmaps; ++level)
  {
    g_return_if_fail(offset < ktx_size);

    const unsigned int width  = std::max(1u, base_width  >> level);
    const unsigned int height = std::max(1u, base_height >> level);
    const unsigned int size   = ktx[offset++] / 4;

    g_return_if_fail(offset + size <= ktx_size);

    glCompressedTexImage2D(GL_TEXTURE_2D, level, int_format,
                           width, height, 0, size * 4, &ktx[offset]);
    offset += size;
  }
}

void GL::set_object_label(GLenum identifier, GLuint name, const char* label)
{
  if (extensions().debug)
    glObjectLabel(identifier, name, -1, label);
}

bool GL::debug_mode_requested()
{
  const char *const messages_debug = g_getenv("G_MESSAGES_DEBUG");
  const GDebugKey debug_key {GL::log_domain, 1};

  // Request a debug OpenGL context if the OpenGL message log domain
  // has been configured for debug output.
  return g_parse_debug_string(messages_debug, &debug_key, 1);
}
