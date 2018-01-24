/*
 * Copyright (c) 2004-2018  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_VECTORMATH_H_INCLUDED
#define SOMATO_VECTORMATH_H_INCLUDED

#if SOMATO_VECTOR_USE_SSE
# include "simd_sse.h"
#else
# include "simd_fallback.h"
#endif

#include <array>
#include <cmath>

namespace Math
{

class Matrix4;
class Quat;

/* Math::Vector4 represents a vector of four single-precision floating point
 * scalars. Basic arithmetic is supported via operators. Other functionality
 * is available via non-member functions.
 */
class Vector4
{
private:
  Simd::V4f v_;

  friend class Matrix4;
  friend class Quat;
  friend Vector4 operator*(const Matrix4& a, const Vector4& b);
  friend Vector4 operator*(const Vector4& a, const Matrix4& b);
  friend Matrix4 translate(const Matrix4& m, const Vector4& t);

  explicit Vector4(const Simd::V4f& v) : v_ {v} {}

public:
  typedef float       value_type;
  typedef std::size_t size_type;

  static const std::array<Vector4, 4> basis;

  constexpr Vector4() : v_ {0.f, 0.f, 0.f, 0.f} {}
  constexpr Vector4(value_type x_, value_type y_, value_type z_, value_type w_ = 0.f)
    : v_ {x_, y_, z_, w_} {}

  Vector4& operator+=(const Vector4& b) { v_ = Simd::add4(v_, b.v_); return *this; }
  Vector4& operator-=(const Vector4& b) { v_ = Simd::sub4(v_, b.v_); return *this; }
  Vector4& operator*=(value_type b) { v_ = Simd::mul4s(v_, b); return *this; }
  Vector4& operator/=(value_type b) { v_ = Simd::div4s(v_, b); return *this; }

  friend Vector4 operator+(const Vector4& a, const Vector4& b)
    { return Vector4(Simd::add4(a.v_, b.v_)); }

  friend Vector4 operator-(const Vector4& a, const Vector4& b)
    { return Vector4(Simd::sub4(a.v_, b.v_)); }

  friend Vector4 operator*(const Vector4& a, value_type b)
    { return Vector4(Simd::mul4s(a.v_, b)); }

  friend Vector4 operator*(value_type a, const Vector4& b)
    { return Vector4(Simd::mul4s(b.v_, a)); }

  friend Vector4 operator/(const Vector4& a, value_type b)
    { return Vector4(Simd::div4s(a.v_, b)); }

  friend Vector4 operator+(const Vector4& v) { return v; }
  friend Vector4 operator-(const Vector4& v) { return Vector4(Simd::neg4(v.v_)); }

  friend bool operator==(const Vector4& a, const Vector4& b)
    { return Simd::cmp4eq(a.v_, b.v_); }

  friend bool operator!=(const Vector4& a, const Vector4& b)
    { return !Simd::cmp4eq(a.v_, b.v_); }

  friend value_type dot(const Vector4& a, const Vector4& b)
    { return Simd::dot4s(a.v_, b.v_); }

  friend Vector4 cross(const Vector4& a, const Vector4& b)
    { return Vector4(Simd::cross3(a.v_, b.v_)); }

  friend value_type magnitude(const Vector4& v) { return Simd::mag4s(v.v_); }

  void normalize() { v_ = Simd::norm4(v_); }
  friend Vector4 normalize(const Vector4& v) { return Vector4(Simd::norm4(v.v_)); }

  value_type&       operator[](size_type i)       { return Simd::ref4s(v_, i); }
  const value_type& operator[](size_type i) const { return Simd::ref4s(v_, i); }

  value_type x() const { return Simd::ext4s<0>(v_); }
  value_type y() const { return Simd::ext4s<1>(v_); }
  value_type z() const { return Simd::ext4s<2>(v_); }
  value_type w() const { return Simd::ext4s<3>(v_); }
};

/* Math::Matrix4 represents a 4x4 square matrix of single-precision
 * floating point scalars in column-major order, compatible with OpenGL.
 * Multiplication with another matrix and multiplication with a vector
 * are supported via operators.
 */
class Matrix4
{
private:
  Simd::V4f m_[4];

  enum Uninitialized { uninitialized };

  explicit Matrix4(Uninitialized) {}
  explicit Matrix4(const Simd::V4f& c0, const Simd::V4f& c1,
                   const Simd::V4f& c2, const Simd::V4f& c3)
    : m_ {c0, c1, c2, c3} {}

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  Matrix4(); // load identity matrix
  constexpr Matrix4(const Vector4& c0, const Vector4& c1,
                    const Vector4& c2, const Vector4& c3 = {0.f, 0.f, 0.f, 1.f})
    : m_ {c0.v_, c1.v_, c2.v_, c3.v_} {}

  // Note: Result will be scaled if the input is not a unit quaternion.
  static inline Matrix4 from_quaternion(const Quat& quat);

  Matrix4& operator*=(const Matrix4& b)
    { Simd::mat4_mul_mm(m_, b.m_, m_); return *this; }

  friend Matrix4 operator*(const Matrix4& a, const Matrix4& b)
    { Matrix4 r (uninitialized); Simd::mat4_mul_mm(a.m_, b.m_, r.m_); return r; }

  friend Vector4 operator*(const Matrix4& a, const Vector4& b)
    { return Vector4(Simd::mat4_mul_mv(a.m_, b.v_)); }

  friend Vector4 operator*(const Vector4& a, const Matrix4& b)
    { return Vector4(Simd::mat4_mul_vm(a.v_, b.m_)); }

  void scale(value_type s)
    { m_[0] = Simd::mul4s(m_[0], s); m_[1] = Simd::mul4s(m_[1], s); m_[2] = Simd::mul4s(m_[2], s); }
  friend Matrix4 scale(const Matrix4& m, value_type s)
    { return Matrix4(Simd::mul4s(m.m_[0], s), Simd::mul4s(m.m_[1], s), Simd::mul4s(m.m_[2], s), m.m_[3]); }

  void translate(const Vector4& t) { m_[3] = Simd::mat4_mul_mv(m_, t.v_); }
  friend Matrix4 translate(const Matrix4& m, const Vector4& t)
    { return Matrix4(m.m_[0], m.m_[1], m.m_[2], Simd::mat4_mul_mv(m.m_, t.v_)); }

  void transpose() { Simd::mat4_transpose(m_, m_); }
  friend Matrix4 transpose(const Matrix4& m)
    { Matrix4 r (uninitialized); Simd::mat4_transpose(m.m_, r.m_); return r; }

  value_type*       operator[](size_type i)       { return &Simd::ref4s(m_[i], 0); }
  const value_type* operator[](size_type i) const { return &Simd::ref4s(m_[i], 0); }
};

/* Math::Quat represents a quaternion r + xi + yj + zk as vector (r, x, y, z).
 */
class Quat
{
private:
  Simd::V4f v_;

  friend Matrix4 Matrix4::from_quaternion(const Quat &quat);

  explicit Quat(const Simd::V4f& v) : v_ {v} {}

  static constexpr Quat from_axis_(float x, float y, float z, float s, float c)
    { return {c, x * s, y * s, z * s}; }

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  constexpr Quat() : v_ {1.f, 0.f, 0.f, 0.f} {}
  constexpr Quat(value_type r_, value_type x_, value_type y_, value_type z_)
    : v_ {r_, x_, y_, z_} {}

  // Note: Result is a unit quaternion only if (x,y,z) is a unit vector.
  static constexpr Quat from_axis(value_type x, value_type y, value_type z, value_type phi)
    { return from_axis_(x, y, z, std::sin(0.5f * phi), std::cos(0.5f * phi)); }

  // Note: Result is a unit quaternion only if the axis is a unit vector.
  static Quat from_axis(const Vector4& a, value_type phi)
    { return Quat(Simd::quat_from_axis(a.v_, phi)); }

  // Note: Result is not normalized even if a and b are unit vectors.
  static Quat from_vectors(const Vector4& a, const Vector4& b)
    { return Quat(Simd::quat_from_vectors(a.v_, b.v_)); }

  Vector4 axis() const { return Vector4(Simd::quat_axis(v_)); } // not normalized
  value_type angle() const { return Simd::quat_angle(v_); }

  Quat& operator*=(const Quat& b)
    { v_ = Simd::quat_mul(v_, b.v_); return *this; }
  friend Quat operator*(const Quat& a, const Quat& b)
    { return Quat(Simd::quat_mul(a.v_, b.v_)); }

  Quat& operator+=(const Quat& b) { v_ = Simd::add4(v_, b.v_); return *this; }
  Quat& operator-=(const Quat& b) { v_ = Simd::sub4(v_, b.v_); return *this; }
  Quat& operator*=(value_type b) { v_ = Simd::mul4s(v_, b); return *this; }
  Quat& operator/=(value_type b) { v_ = Simd::div4s(v_, b); return *this; }

  friend Quat operator+(const Quat& a, const Quat& b)
    { return Quat(Simd::add4(a.v_, b.v_)); }

  friend Quat operator-(const Quat& a, const Quat& b)
    { return Quat(Simd::sub4(a.v_, b.v_)); }

  friend Quat operator*(const Quat& a, value_type b)
    { return Quat(Simd::mul4s(a.v_, b)); }

  friend Quat operator*(value_type a, const Quat& b)
    { return Quat(Simd::mul4s(b.v_, a)); }

  friend Quat operator/(const Quat& a, value_type b)
    { return Quat(Simd::div4s(a.v_, b)); }

  friend Quat operator+(const Quat& q) { return q; }
  friend Quat operator-(const Quat& q) { return Quat(Simd::neg4(q.v_)); }
  friend Quat operator~(const Quat& q) { return Quat(Simd::quat_conj(q.v_)); }

  friend bool operator==(const Quat& a, const Quat& b)
    { return Simd::cmp4eq(a.v_, b.v_); }

  friend bool operator!=(const Quat& a, const Quat& b)
    { return !Simd::cmp4eq(a.v_, b.v_); }

  friend Quat invert(const Quat& q) { return Quat(Simd::quat_inv(q.v_)); }
  friend value_type magnitude(const Quat& q) { return Simd::mag4s(q.v_); }

  void normalize() { v_ = Simd::norm4(v_); }
  friend Quat normalize(const Quat& q) { return Quat(Simd::norm4(q.v_)); }

  value_type&       operator[](size_type i)       { return Simd::ref4s(v_, i); }
  const value_type& operator[](size_type i) const { return Simd::ref4s(v_, i); }

  value_type r() const { return Simd::ext4s<0>(v_); }
  value_type x() const { return Simd::ext4s<1>(v_); }
  value_type y() const { return Simd::ext4s<2>(v_); }
  value_type z() const { return Simd::ext4s<3>(v_); }
};

inline Matrix4 Matrix4::from_quaternion(const Quat& quat)
{
  Matrix4 result (uninitialized);
  Simd::quat_to_matrix(quat.v_, result.m_);
  return result;
}

} // namespace Math

#endif // !SOMATO_VECTORMATH_H_INCLUDED
