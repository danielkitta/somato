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

namespace
{

class ScopedShader
{
private:
  GLuint shader_;

public:
  explicit ScopedShader(GLenum type)
    : shader_ {glCreateShader(type)} { GL::Error::throw_if_fail(shader_ != 0); }
  ~ScopedShader() { if (shader_) glDeleteShader(shader_); }

  ScopedShader(const ScopedShader&) = delete;
  ScopedShader& operator=(const ScopedShader&) = delete;

  GLuint get() const { return shader_; }
  GLuint release() { const GLuint shader = shader_; shader_ = 0; return shader; }
};

static
GLuint compile_shader(GLenum type, const std::string& filename)
{
  ScopedShader shader {type};
  {
    const std::string source = Glib::file_get_contents(filename);
    const int   len = source.size();
    const char* str = source.c_str();

    glShaderSource(shader.get(), 1, &str, &len);
  }
  glCompileShader(shader.get());

  GLint bufsize = 0;
  glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &bufsize);

  if (bufsize > 0)
  {
    const std::unique_ptr<char[]> buffer {new char[bufsize + 1]};

    GLsizei length = 0;
    glGetShaderInfoLog(shader.get(), bufsize, &length, buffer.get());

    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\0'))
      --length;

    buffer[length] = '\0';
    g_log("OpenGL", G_LOG_LEVEL_INFO, "%s", buffer.get());
  }

  GLint success = GL_FALSE;
  glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &success);

  if (!success)
    throw GL::Error{Glib::ustring::compose("Compiling %1 failed", filename)};

  return shader.release();
}

} // anonymous namespace

namespace GL
{

ShaderObject::ShaderObject(unsigned int type, const std::string& filename)
:
  shader_ {compile_shader(type, filename)}
{}

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

    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\0'))
      --length;

    buffer[length] = '\0';
    g_log("OpenGL", G_LOG_LEVEL_INFO, "%s", buffer.get());
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
