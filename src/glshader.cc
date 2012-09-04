/*
 * Copyright (c) 2012  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "glshader.h"
#include "glutils.h"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/ustring.h>
#include <GL/gl.h>

#include <memory>

namespace GL
{

ShaderObject::ShaderObject(unsigned int type, std::string filename)
:
  shader_ {glCreateShader(type)}
{
  GL::Error::throw_if_fail(shader_ != 0);
  {
    const std::string source = Glib::file_get_contents(filename);
    const int   len = source.size();
    const char* str = source.c_str();

    glShaderSource(shader_, 1, &str, &len);
  }
  glCompileShader(shader_);

  GLint bufsize = 0;
  glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &bufsize);

  if (bufsize > 0)
  {
    const std::unique_ptr<char[]> buffer {new char[bufsize + 1]};

    GLsizei length = 0;
    glGetShaderInfoLog(shader_, bufsize, &length, buffer.get());

    buffer[length] = '\0';
    g_printerr("%s\n", buffer.get());
  }

  GLint success = GL_FALSE;
  glGetShaderiv(shader_, GL_COMPILE_STATUS, &success);

  if (!success)
  {
    glDeleteShader(shader_);
    shader_ = 0;

    throw GL::Error{Glib::ustring::compose("Compiling %1 failed", filename)};
  }
}

ShaderObject::~ShaderObject()
{
  if (shader_)
    glDeleteShader(shader_);
}

ShaderProgram::~ShaderProgram()
{
  if (program_)
    glDeleteProgram(program_);
}

void ShaderProgram::attach(const ShaderObject& shader)
{
  g_return_if_fail(shader);

  if (!program_)
  {
    program_ = glCreateProgram();
    GL::Error::throw_if_fail(program_ != 0);
  }
  glAttachShader(program_, shader.get());
}

void ShaderProgram::bind_attrib_location(unsigned int idx, const char* name)
{
  g_return_if_fail(program_ != 0);

  glBindAttribLocation(program_, idx, name);
}

void ShaderProgram::link()
{
  g_return_if_fail(program_ != 0);

  glLinkProgram(program_);

  GLint bufsize = 0;
  glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &bufsize);

  if (bufsize > 0)
  {
    const std::unique_ptr<char[]> buffer {new char[bufsize + 1]};

    GLsizei length = 0;
    glGetProgramInfoLog(program_, bufsize, &length, buffer.get());

    buffer[length] = '\0';
    g_printerr("%s\n", buffer.get());
  }

  GLint success = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &success);

  if (!success)
    throw GL::Error{"Linking of shader program failed"};
}

int ShaderProgram::get_uniform_location(const char* name) const
{
  g_return_val_if_fail(program_ != 0, -1);

  return glGetUniformLocation(program_, name);
}

void ShaderProgram::use()
{
  g_return_if_fail(program_ != 0);

  glUseProgram(program_);
}

// static
void ShaderProgram::unuse()
{
  glUseProgram(0);
}

void ShaderProgram::reset()
{
  if (program_)
  {
    glDeleteProgram(program_);
    program_ = 0;
  }
}

} // namespace GL
