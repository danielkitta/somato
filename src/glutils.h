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

#include "meshtypes.h"

#include <glib.h>
#include <glibmm/ustring.h>
#include <gdkmm/glcontext.h>

#include <type_traits>
#include <utility>
#include <cmath>
#include <cstddef>

#include <epoxy/gl.h>

namespace GL
{

constexpr const char* log_domain = "OpenGL";

/* Record of available GL extensions and implementation limits.
 */
struct Extensions
{
  bool  is_gles                    = false;
  bool  debug                      = false;
  bool  debug_output               = false;
  bool  vertex_type_2_10_10_10_rev = false;
  bool  texture_border_clamp       = false;
  bool  texture_filter_anisotropic = false;
  float max_anisotropy             = 0.;

  // Query GL extensions after initial context setup.
  static void query(bool use_es, int major, int minor)
    { instance_.query_(use_es, (major << 8) | minor); }

  friend inline const Extensions& extensions();

private:
  void query_(bool use_es, int version);

  static Extensions instance_;
};

/* Access GL extensions record.
 */
inline const Extensions& extensions() { return Extensions::instance_; }

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
                  std::size_t length, unsigned int access)
    : data_ {map_checked(target, offset, length, access)}, target_ {target}
  {}
  ~ScopedMapBuffer() { if (data_) unmap_checked(target_); }

  ScopedMapBuffer(const ScopedMapBuffer& other) = delete;
  ScopedMapBuffer& operator=(const ScopedMapBuffer& other) = delete;

  void* data() const { return data_; }
  bool unmap() { data_ = nullptr; return unmap_checked(target_); }

private:
  static void* map_checked(unsigned int target, std::size_t offset,
                           std::size_t length, unsigned int access);
  static bool unmap_checked(unsigned int target);

  void*        data_;
  unsigned int target_;
};

/* Encapsulate access to a temporarily mapped buffer object.
 */
template <typename Op>
inline bool access_mapped_buffer(unsigned int target, std::size_t offset,
                                 std::size_t length, unsigned int access,
                                 Op operation)
{
  ScopedMapBuffer buffer {target, offset, length, access};

  if (void *const data = buffer.data())
  {
    operation(data);
    return buffer.unmap();
  }
  return false;
}

enum Packed2i16 : unsigned int {};
enum Packed4u8  : unsigned int {};

inline Packed2i16 pack_2i16(int x, int y)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  return static_cast<Packed2i16>((x & 0xFFFFu) | ((y & 0xFFFFu) << 16));
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  return static_cast<Packed2i16>(((x & 0xFFFFu) << 16) | (y & 0xFFFFu));
#endif
}

inline Packed2i16 pack_2i16_norm(float x, float y)
{
  const float scale = 32767.f;
  return pack_2i16(std::lrint(x * scale), std::lrint(y * scale));
}

inline Packed4u8 pack_4u8(int r, int g, int b, int a)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  return static_cast<Packed4u8>((r & 0xFFu)        | ((g & 0xFFu) << 8)
                             | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24));
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  return static_cast<Packed4u8>(((r & 0xFFu) << 24) | ((g & 0xFFu) << 16)
                              | ((b & 0xFFu) << 8)  |  (a & 0xFFu));
#endif
}

inline Packed4u8 pack_4u8_norm(float r, float g, float b, float a)
{
  const float scale = 255.f;
  return pack_4u8(std::lrint(r * scale), std::lrint(g * scale),
                  std::lrint(b * scale), std::lrint(a * scale));
}

template <typename T> constexpr GLenum attrib_type_;

template <> constexpr GLenum attrib_type_<GLbyte>     = GL_BYTE;
template <> constexpr GLenum attrib_type_<GLubyte>    = GL_UNSIGNED_BYTE;
template <> constexpr GLenum attrib_type_<GLshort>    = GL_SHORT;
template <> constexpr GLenum attrib_type_<GLushort>   = GL_UNSIGNED_SHORT;
template <> constexpr GLenum attrib_type_<GLint>      = GL_INT;
template <> constexpr GLenum attrib_type_<GLuint>     = GL_UNSIGNED_INT;
template <> constexpr GLenum attrib_type_<GLfloat>    = GL_FLOAT;
template <> constexpr GLenum attrib_type_<Packed2i16> = GL_SHORT;
template <> constexpr GLenum attrib_type_<Packed4u8>  = GL_UNSIGNED_BYTE;
template <> constexpr GLenum attrib_type_<Somato::Int_2_10_10_10_rev>
                                                      = GL_INT_2_10_10_10_REV;
template <typename T> constexpr GLenum attrib_type =
    attrib_type_<std::remove_all_extents_t<std::remove_reference_t<T>>>;

template <typename T>           constexpr int attrib_size_ = 1;
template <typename T, size_t N> constexpr int attrib_size_<T[N]> = N;

template <> constexpr int attrib_size_<Packed2i16> = 2;
template <> constexpr int attrib_size_<Packed4u8>  = 4;
template <> constexpr int attrib_size_<Somato::Int_2_10_10_10_rev> = 4;

template <typename T>
constexpr int attrib_size = attrib_size_<std::remove_reference_t<T>>;

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
