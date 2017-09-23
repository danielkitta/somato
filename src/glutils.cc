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
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      message = "invalid framebuffer operation";
      break;
    case GL_OUT_OF_MEMORY:
      message = "out of memory";
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

GL::ScopedMapBuffer::ScopedMapBuffer(unsigned int target, std::size_t offset,
                                     std::size_t length, unsigned int access)
:
  data_   {glMapBufferRange(target, offset, length, access)},
  target_ {target}
{
  if (!data_)
    g_log(GL::log_domain, G_LOG_LEVEL_WARNING, "glMapBufferRange() failed");
}

GL::ScopedMapBuffer::~ScopedMapBuffer()
{
  if (data_)
  {
    if (!glUnmapBuffer(target_))
      g_log(GL::log_domain, G_LOG_LEVEL_WARNING, "glUnmapBuffer() failed");
  }
}

bool GL::debug_mode_requested()
{
  const char *const messages_debug = g_getenv("G_MESSAGES_DEBUG");
  const GDebugKey debug_key {GL::log_domain, 1};

  // Request a debug OpenGL context if the OpenGL message log domain
  // has been configured for debug output.
  return g_parse_debug_string(messages_debug, &debug_key, 1);
}
