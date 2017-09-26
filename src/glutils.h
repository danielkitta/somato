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

#ifndef SOMATO_GLUTILS_H_INCLUDED
#define SOMATO_GLUTILS_H_INCLUDED

#include <glib.h>
#include <glibmm/ustring.h>
#include <gdkmm/glcontext.h>

#include <type_traits>
#include <cmath>
#include <cstddef>

#include <epoxy/gl.h>

namespace GL
{

constexpr const char* log_domain = "OpenGL";

/* Exception class for errors reported by glGetError() and other OpenGL
 * failure conditions.
 */
class Error : public Gdk::GLError
{
private:
  unsigned int gl_code_;

public:
  explicit Error(unsigned int error_code);
  explicit Error(const Glib::ustring& message, unsigned int error_code = 0);
  virtual ~Error() noexcept;

  Error(const Error& other) = default;
  Error& operator=(const Error& other) = default;

  unsigned int gl_code() const { return gl_code_; }

  static void check();                // throw if glGetError() != GL_NO_ERROR
  static void fail() G_GNUC_NORETURN; // like check() but always throws

  static void throw_if_fail(bool condition) { if (G_UNLIKELY(!condition)) fail(); }
};

class FramebufferError : public Error
{
public:
  explicit FramebufferError(unsigned int error_code);
  virtual ~FramebufferError() noexcept;
};

/* Scoped glMapBufferRange().
 */
class ScopedMapBuffer
{
public:
  ScopedMapBuffer(unsigned int target, std::size_t offset,
                  std::size_t length, unsigned int access);
  ~ScopedMapBuffer();

  operator volatile void*() const { return data_; }
  template <class T> volatile T* get() const { return static_cast<volatile T*>(data_); }

  ScopedMapBuffer(const ScopedMapBuffer& other) = delete;
  ScopedMapBuffer& operator=(const ScopedMapBuffer& other) = delete;

private:
  volatile void* data_;
  unsigned int   target_;
};

/* Vector packed as 32-bit unsigned integer.
 */
enum Int_2_10_10_10_rev : unsigned int {};

template <typename T> constexpr GLenum attrib_type_;

template <> constexpr GLenum attrib_type_<GLbyte>   = GL_BYTE;
template <> constexpr GLenum attrib_type_<GLubyte>  = GL_UNSIGNED_BYTE;
template <> constexpr GLenum attrib_type_<GLshort>  = GL_SHORT;
template <> constexpr GLenum attrib_type_<GLushort> = GL_UNSIGNED_SHORT;
template <> constexpr GLenum attrib_type_<GLint>    = GL_INT;
template <> constexpr GLenum attrib_type_<GLuint>   = GL_UNSIGNED_INT;
template <> constexpr GLenum attrib_type_<GLfloat>  = GL_FLOAT;
template <> constexpr GLenum attrib_type_<Int_2_10_10_10_rev> = GL_INT_2_10_10_10_REV;

template <typename T> constexpr GLenum attrib_type =
    attrib_type_<std::remove_all_extents_t<std::remove_reference_t<T>>>;

template <typename T>           constexpr int attrib_size_ = 1;
template <typename T, size_t N> constexpr int attrib_size_<T[N]> = N;

template <> constexpr int attrib_size_<Int_2_10_10_10_rev> = 4;

template <typename T>
constexpr int attrib_size = attrib_size_<std::remove_reference_t<T>>;

/* Convert a floating-point normal into a packed 10-bit integer format.
 */
inline Int_2_10_10_10_rev pack_normal(float x, float y, float z)
{
  const int ix = std::lrint(x * 511.f);
  const int iy = std::lrint(y * 511.f);
  const int iz = std::lrint(z * 511.f);

  return static_cast<Int_2_10_10_10_rev>((ix & 0x3FFu)
                                      | ((iy & 0x3FFu) << 10)
                                      | ((iz & 0x3FFu) << 20));
}

/* Return whether the user requested OpenGL debug mode.
 */
bool debug_mode_requested();

/* Convert a VBO offset to a pointer.
 */
template <typename T = char>
constexpr void* buffer_offset(std::size_t offset)
{
  return static_cast<T*>(0) + offset;
}

} // namespace GL

#endif // !SOMATO_GLUTILS_H_INCLUDED
