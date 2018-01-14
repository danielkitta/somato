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

#if defined(SOMATO_VECTORMATH_H_INCLUDED) && !SOMATO_VECTOR_USE_SSE

#include <array>
#include <cmath>

namespace Math
{

class Matrix4;

class Vector4
{
private:
  float v_[4];

  friend class Matrix4;
  friend class Quat;
  friend Vector4 operator*(const Matrix4& a, const Vector4& b);
  friend Vector4 operator*(const Matrix4& a, const float* b);
  friend Vector4 operator*(const float a[][4], const Vector4& b);
  friend Vector4 operator*(const Vector4& a, const Matrix4& b);
  friend Vector4 operator*(const float* a, const Matrix4& b);
  friend Vector4 operator*(const Vector4& a, const float b[][4]);

  enum Uninitialized { uninitialized };
  explicit Vector4(Uninitialized) {}

  static void  add_  (const float* a, const float* b, float* result);
  static void  sub_  (const float* a, const float* b, float* result);
  static void  mul_  (const float* a, float b, float* result);
  static void  div_  (const float* a, float b, float* result);
  static float dot_  (const float* a, const float* b);
  static void  cross_(const float* a, const float* b, float* result);
  static bool  equal_(const float* a, const float* b);
  static void  norm_ (const float* v, float* result);

public:
  typedef float         value_type;
  typedef unsigned int  size_type;

  static const std::array<Vector4, 4> basis;

  constexpr Vector4() : v_ {0.f, 0.f, 0.f, 0.f} {}
  explicit Vector4(const value_type* b) : v_ {b[0], b[1], b[2], b[3]} {}
  constexpr Vector4(value_type x_, value_type y_, value_type z_, value_type w_ = 0.f)
    : v_ {x_, y_, z_, w_} {}

  Vector4& operator=(const value_type* b)
    { v_[0] = b[0]; v_[1] = b[1]; v_[2] = b[2]; v_[3] = b[3]; return *this; }

  value_type&       operator[](size_type i)       { return v_[i]; }
  const value_type& operator[](size_type i) const { return v_[i]; }

  value_type x() const { return v_[0]; }
  value_type y() const { return v_[1]; }
  value_type z() const { return v_[2]; }
  value_type w() const { return v_[3]; }

  Vector4& operator+=(const Vector4& b)    { add_(v_, b.v_, v_);   return *this; }
  Vector4& operator+=(const value_type* b) { add_(v_, b, v_);      return *this; }
  Vector4& operator-=(const Vector4& b)    { sub_(v_, b.v_, v_);   return *this; }
  Vector4& operator-=(const value_type* b) { sub_(v_, b, v_);      return *this; }
  Vector4& operator*=(value_type b)        { mul_(v_, b, v_);      return *this; }
  Vector4& operator/=(value_type b)        { div_(v_, b, v_);      return *this; }
  Vector4& operator%=(const Vector4& b)    { cross_(v_, b.v_, v_); return *this; }
  Vector4& operator%=(const value_type* b) { cross_(v_, b, v_);    return *this; }

  friend Vector4 operator+(const Vector4& a, const Vector4& b)
    { Vector4 r (uninitialized); add_(a.v_, b.v_, r.v_); return r; }

  friend Vector4 operator+(const Vector4& a, const value_type* b)
    { Vector4 r (uninitialized); add_(a.v_, b, r.v_); return r; }

  friend Vector4 operator+(const value_type* a, const Vector4& b)
    { Vector4 r (uninitialized); add_(a, b.v_, r.v_); return r; }

  friend Vector4 operator-(const Vector4& a, const Vector4& b)
    { Vector4 r (uninitialized); sub_(a.v_, b.v_, r.v_); return r; }

  friend Vector4 operator-(const Vector4& a, const value_type* b)
    { Vector4 r (uninitialized); sub_(a.v_, b, r.v_); return r; }

  friend Vector4 operator-(const value_type* a, const Vector4& b)
    { Vector4 r (uninitialized); sub_(a, b.v_, r.v_); return r; }

  friend Vector4 operator*(const Vector4& a, value_type b)
    { Vector4 r (uninitialized); mul_(a.v_, b, r.v_); return r; }

  friend Vector4 operator*(value_type a, const Vector4& b)
    { Vector4 r (uninitialized); mul_(b.v_, a, r.v_); return r; }

  friend Vector4 operator/(const Vector4& a, value_type b)
    { Vector4 r (uninitialized); div_(a.v_, b, r.v_); return r; }

  friend value_type operator*(const Vector4& a, const Vector4& b)
    { return dot_(a.v_, b.v_); }

  friend value_type operator*(const Vector4& a, const value_type* b)
    { return dot_(a.v_, b); }

  friend value_type operator*(const value_type* a, const Vector4& b)
    { return dot_(a, b.v_); }

  friend Vector4 operator%(const Vector4& a, const Vector4& b)
    { Vector4 r (uninitialized); cross_(a.v_, b.v_, r.v_); return r; }

  friend Vector4 operator%(const Vector4& a, const value_type* b)
    { Vector4 r (uninitialized); cross_(a.v_, b, r.v_); return r; }

  friend Vector4 operator%(const value_type* a, const Vector4& b)
    { Vector4 r (uninitialized); cross_(a, b.v_, r.v_); return r; }

  friend bool operator==(const Vector4& a, const Vector4& b)
    { return equal_(a.v_, b.v_); }

  friend bool operator==(const Vector4& a, const value_type* b)
    { return equal_(a.v_, b); }

  friend bool operator==(const value_type* a, const Vector4& b)
    { return equal_(a, b.v_); }

  friend bool operator!=(const Vector4& a, const Vector4& b)
    { return !equal_(a.v_, b.v_); }

  friend bool operator!=(const Vector4& a, const value_type* b)
    { return !equal_(a.v_, b); }

  friend bool operator!=(const value_type* a, const Vector4& b)
    { return !equal_(a, b.v_); }

  static value_type mag(const value_type* v);
  static value_type mag(const Vector4& v) { return mag(v.v_); }

  void normalize() { norm_(v_, v_); }
  Vector4 normalized() const
    { Vector4 r (uninitialized); norm_(v_, r.v_); return r; }
};

inline Vector4 operator+(const Vector4& v)
  { return v; }

inline Vector4 operator-(const Vector4& v)
  { return {-v[0], -v[1], -v[2], -v[3]}; }

class Matrix4
{
private:
  float m_[4][4];

  friend class Quat;

  enum Uninitialized { uninitialized };
  explicit Matrix4(Uninitialized) {}

  static void transpose_(const float m[][4], float result[][4]);
  static void scale_(const float a[][4], float s, float result[][4]);
  static void mul_(const float a[][4], const float* b, float* result);
  static void mul_(const float* a, const float b[][4], float* result);
  static void mul_(const float a[][4], const float b[][4], float result[][4]);

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  Matrix4(); // load identity matrix
  constexpr Matrix4(const Vector4& c0, const Vector4& c1,
                    const Vector4& c2, const Vector4& c3 = {0.f, 0.f, 0.f, 1.f})
    : m_ {{c0.v_[0], c0.v_[1], c0.v_[2], c0.v_[3]},
          {c1.v_[0], c1.v_[1], c1.v_[2], c1.v_[3]},
          {c2.v_[0], c2.v_[1], c2.v_[2], c2.v_[3]},
          {c3.v_[0], c3.v_[1], c3.v_[2], c3.v_[3]}} {}

  explicit Matrix4(const value_type b[][4]) { *this = b; }
  Matrix4& operator=(const value_type b[][4]);

  Matrix4& operator*=(const Matrix4& b)        { mul_(m_, b.m_, m_); return *this; }
  Matrix4& operator*=(const value_type b[][4]) { mul_(m_, b, m_);    return *this; }

  friend Vector4 operator*(const Matrix4& a, const Vector4& b)
    { Vector4 r (Vector4::uninitialized); mul_(a.m_, b.v_, r.v_); return r; }

  friend Vector4 operator*(const Matrix4& a, const Vector4::value_type* b)
    { Vector4 r (Vector4::uninitialized); mul_(a.m_, b, r.v_); return r; }

  friend Vector4 operator*(const value_type a[][4], const Vector4& b)
    { Vector4 r (Vector4::uninitialized); mul_(a, b.v_, r.v_); return r; }

  friend Vector4 operator*(const Vector4& a, const Matrix4& b)
    { Vector4 r (Vector4::uninitialized); mul_(a.v_, b.m_, r.v_); return r; }

  friend Vector4 operator*(const value_type* a, const Matrix4& b)
    { Vector4 r (Vector4::uninitialized); mul_(a, b.m_, r.v_); return r; }

  friend Vector4 operator*(const Vector4& a, const value_type b[][4])
    { Vector4 r (Vector4::uninitialized); mul_(a.v_, b, r.v_); return r; }

  friend Matrix4 operator*(const Matrix4& a, const Matrix4& b)
    { Matrix4 r (uninitialized); mul_(a.m_, b.m_, r.m_); return r; }

  friend Matrix4 operator*(const Matrix4& a, const value_type b[][4])
    { Matrix4 r (uninitialized); mul_(a.m_, b, r.m_); return r; }

  friend Matrix4 operator*(const value_type a[][4], const Matrix4& b)
    { Matrix4 r (uninitialized); mul_(a, b.m_, r.m_); return r; }

  void scale(float s) { scale_(m_, s, m_); }
  Matrix4 scaled(float s) const
    { Matrix4 r (uninitialized); scale_(m_, s, r.m_); return r; }

  void translate(const Vector4& t) { mul_(m_, t.v_, m_[3]); }
  Matrix4 translated(const Vector4& t) const
    { return {Vector4(m_[0]), Vector4(m_[1]), Vector4(m_[2]), *this * t}; }

  void transpose() { transpose_(m_, m_); }
  Matrix4 transposed() const
    { Matrix4 r (uninitialized); transpose_(m_, r.m_); return r; }

  value_type*       operator[](size_type i)       { return m_[i]; }
  const value_type* operator[](size_type i) const { return m_[i]; }
};

class Quat
{
private:
  float v_[4];

  enum Uninitialized { uninitialized };
  explicit Quat(Uninitialized) {}

  static void from_axis_(const float* a, float phi, float* result);
  static void to_matrix_(const float* quat, float result[][4]);
  static void mul_(const float* a, const float* b, float* result);

  static constexpr Quat from_axis_(float x, float y, float z, float s, float c)
    { return {x * s, y * s, z * s, c}; }

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  constexpr Quat() : v_ {0.f, 0.f, 0.f, 1.f} {}
  constexpr Quat(value_type x_, value_type y_, value_type z_, value_type w_)
    : v_ {x_, y_, z_, w_} {}

  explicit Quat(const value_type* b) : v_ {b[0], b[1], b[2], b[3]} {}
  Quat& operator=(const value_type* b)
    { v_[0] = b[0]; v_[1] = b[1]; v_[2] = b[2]; v_[3] = b[3]; return *this; }

  static constexpr Quat from_axis(value_type x, value_type y, value_type z, value_type phi)
    { return from_axis_(x, y, z, std::sin(0.5f * phi), std::cos(0.5f * phi)); }

  static Quat from_axis(const Vector4& a, value_type phi)
    { Quat r (uninitialized); from_axis_(a.v_, phi, r.v_); return r; }

  static Quat from_axis(const Vector4::value_type* a, value_type phi)
    { Quat r (uninitialized); from_axis_(a, phi, r.v_); return r; }

  static Matrix4 to_matrix(const Quat& quat)
    { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat.v_, r.m_); return r; }

  static Matrix4 to_matrix(const value_type* quat)
    { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat, r.m_); return r; }

  Vector4 axis() const { return {v_[0], v_[1], v_[2], 0.f}; }
  value_type angle() const; // extracts the rotation angle phi

  Quat& operator*=(const Quat& b)       { mul_(v_, b.v_, v_); return *this; }
  Quat& operator*=(const value_type* b) { mul_(v_, b, v_);    return *this; }

  friend Quat operator*(const Quat& a, const Quat& b)
    { Quat r (uninitialized); mul_(a.v_, b.v_, r.v_); return r; }

  friend Quat operator*(const Quat& a, const value_type* b)
    { Quat r (uninitialized); mul_(a.v_, b, r.v_); return r; }

  friend Quat operator*(const value_type* a, const Quat& b)
    { Quat r (uninitialized); mul_(a, b.v_, r.v_); return r; }

  Quat renormalized() const
    { Quat r (uninitialized); Vector4::norm_(v_, r.v_); return r; }

  value_type&       operator[](size_type i)       { return v_[i]; }
  const value_type& operator[](size_type i) const { return v_[i]; }

  value_type x() const { return v_[0]; }
  value_type y() const { return v_[1]; }
  value_type z() const { return v_[2]; }
  value_type w() const { return v_[3]; }
};

} // namespace Math

#else /* !SOMATO_VECTORMATH_H_INCLUDED || SOMATO_VECTOR_USE_SSE */
# error "This header file should not be included directly"
#endif
