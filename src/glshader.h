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

#ifndef SOMATO_GLSHADER_H_INCLUDED
#define SOMATO_GLSHADER_H_INCLUDED

#include <algorithm>
#include <string>

#include <config.h>

namespace GL
{

class ShaderObject
{
public:
  ShaderObject() : shader_ {0} {}
  ShaderObject(unsigned int type, const std::string& filename);
  ~ShaderObject();

  ShaderObject(const ShaderObject&) = delete;
  ShaderObject& operator=(const ShaderObject&) = delete;

  ShaderObject(ShaderObject&& other)
    : shader_ {other.shader_} { other.shader_ = 0; }
  ShaderObject& operator=(ShaderObject&& other)
    { std::swap(shader_, other.shader_); return *this; }

  friend void swap(ShaderObject& a, ShaderObject& b)
    { std::swap(a.shader_, b.shader_); }

  explicit operator bool() const { return (shader_ != 0); }

  unsigned int get() const { return shader_; }

private:
  unsigned int shader_;
};

class ShaderProgram
{
public:
  ShaderProgram() : program_ {0} {}
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  ShaderProgram(ShaderProgram&& other)
    : program_ {other.program_} { other.program_ = 0; }
  ShaderProgram& operator=(ShaderProgram&& other)
    { std::swap(program_, other.program_); return *this; }

  friend void swap(ShaderProgram& a, ShaderProgram& b)
    { std::swap(a.program_, b.program_); }

  explicit operator bool() const { return (program_ != 0); }

  void attach(const ShaderObject& shader);
  void bind_attrib_location(unsigned int idx, const char* name);
  void link();

  int get_uniform_location(const char* name) const;

  void use();
  static void unuse();

  void reset();

private:
  unsigned int program_;
};

} // namespace GL

#endif /* !SOMATO_GLSHADER_H_INCLUDED */
