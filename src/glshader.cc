/*
 * Copyright (c) 2012-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "glshader.h"
#include "glutils.h"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/ustring.h>
#include <gdkmm/glcontext.h>
#include <epoxy/gl.h>

#include <memory>
#include <cstring>

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

void load_shader_source(GLuint shader, const std::string& filename)
{
  const char* snippets[2];
  int lengths[G_N_ELEMENTS(snippets)];

  if (const auto context = Gdk::GLContext::get_current())
  {
    // Select an appropriate preamble to the shader source text
    // depending on whether we are using OpenGL ES or desktop OpenGL.
    if (context->get_use_es())
      snippets[0] = "#version 300 es\n"
                    "#define noperspective\n"
                    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                    "precision highp float;\n"
                    "#else\n"
                    "precision mediump float;\n"
                    "#endif\n"
                    "#line 0\n";
    else
      snippets[0] = "#version 150\n"
                    "#line 0\n";

    lengths[0] = std::strlen(snippets[0]);
  }
  else
    throw GL::Error{"No current OpenGL context"};

  const std::string source = Glib::file_get_contents(filename);
  lengths[1]  = source.size();
  snippets[1] = source.c_str();

  glShaderSource(shader, G_N_ELEMENTS(snippets), snippets, lengths);
}

GLuint compile_shader(GLenum type, const std::string& filename)
{
  ScopedShader shader {type};

  g_log(GL::log_domain, G_LOG_LEVEL_DEBUG, "Compiling shader %u: %s",
        shader.get(), filename.c_str());

  load_shader_source(shader.get(), filename);
  glCompileShader(shader.get());

  GLint success = GL_FALSE;
  glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &success);

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

    g_log(GL::log_domain, (success) ? G_LOG_LEVEL_INFO : G_LOG_LEVEL_WARNING,
          "%s", buffer.get());
  }
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

  GLint success = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &success);

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

    g_log(GL::log_domain, (success) ? G_LOG_LEVEL_INFO : G_LOG_LEVEL_WARNING,
          "%s", buffer.get());
  }
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
  if (const GLuint program = program_)
  {
    program_ = 0;
    glDeleteProgram(program);
  }
}

} // namespace GL
