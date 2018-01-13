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

#if defined(SOMATO_VECTORMATH_H_INCLUDED) && SOMATO_VECTOR_USE_SSE

#include <array>
#include <cmath>

#if SOMATO_VECTOR_USE_SSE2
# include <emmintrin.h>
#endif
#include <xmmintrin.h>

namespace Math
{

class Matrix4;

/*
 * Math::Vector4 represents a vector of four single-precision floating point
 * scalars.  Most of the common vector operations are made available via
 * operators.  If a and b are both vectors, a * b denotes the dot product
 * (scalar product) and a % b denotes the cross product.
 *
 * Note that for performance reasons, the result of the rint() method
 * is undefined if the rounded value exceeds the range [-2^31, 2^31-1].
 */
class Vector4
{
private:
  __m128 v_;

  friend class Matrix4;
  friend class Quat;
  friend Vector4 operator*(const Matrix4& a, const Vector4& b);
  friend Vector4 operator*(const Vector4& a, const Matrix4& b);

  explicit Vector4(__m128 v) : v_ {v} {}

  static inline __m128 dot_  (__m128 a, __m128 b); // scalar product across (x,y,z,w)
  static inline __m128 cross_(__m128 a, __m128 b); // 3-D vector product, returns w = 0
#if !SOMATO_VECTOR_USE_SSE2
  static __m128 rint_(__m128 v);
#endif
  static __m128 mag_(__m128 v);  // magnitude of (x,y,z,w)
  static __m128 norm_(__m128 v); // normalize

public:
  typedef float         value_type;
  typedef unsigned int  size_type;

  static const std::array<Vector4, 4> basis;

  constexpr Vector4() : v_ {0.f, 0.f, 0.f, 0.f} {}
  explicit Vector4(const value_type* b) : v_ (_mm_loadu_ps(b)) {}
  constexpr Vector4(value_type x_, value_type y_, value_type z_, value_type w_ = 0.f)
    : v_ {x_, y_, z_, w_} {}

  Vector4& operator=(const value_type* b) { v_ = _mm_loadu_ps(b); return *this; }

  Vector4& operator+=(const Vector4& b) { v_ = _mm_add_ps(v_, b.v_); return *this; }
  Vector4& operator-=(const Vector4& b) { v_ = _mm_sub_ps(v_, b.v_); return *this; }
  Vector4& operator*=(value_type b) { v_ = _mm_mul_ps(v_, _mm_set1_ps(b)); return *this; }
  Vector4& operator/=(value_type b) { v_ = _mm_div_ps(v_, _mm_set1_ps(b)); return *this; }
  Vector4& operator%=(const Vector4& b) { v_ = cross_(v_, b.v_); return *this; }

  friend Vector4 operator+(const Vector4& a, const Vector4& b)
    { return Vector4(_mm_add_ps(a.v_, b.v_)); }

  friend Vector4 operator-(const Vector4& a, const Vector4& b)
    { return Vector4(_mm_sub_ps(a.v_, b.v_)); }

  friend Vector4 operator*(const Vector4& a, value_type b)
    { return Vector4(_mm_mul_ps(a.v_, _mm_set1_ps(b))); }

  friend Vector4 operator*(value_type a, const Vector4& b)
    { return Vector4(_mm_mul_ps(_mm_set1_ps(a), b.v_)); }

  friend Vector4 operator/(const Vector4& a, value_type b)
    { return Vector4(_mm_div_ps(a.v_, _mm_set1_ps(b))); }

  friend value_type operator*(const Vector4& a, const Vector4& b)
    { return _mm_cvtss_f32(dot_(a.v_, b.v_)); }

  friend Vector4 operator%(const Vector4& a, const Vector4& b)
    { return Vector4(cross_(a.v_, b.v_)); }

  friend Vector4 operator+(const Vector4& v) { return Vector4(v.v_); }
  friend Vector4 operator-(const Vector4& v)
    { return Vector4(_mm_xor_ps(_mm_set1_ps(-0.f), v.v_)); }

  friend bool operator==(const Vector4& a, const Vector4& b)
    { return (_mm_movemask_ps(_mm_cmpneq_ps(a.v_, b.v_)) == 0); }

  friend bool operator!=(const Vector4& a, const Vector4& b)
    { return (_mm_movemask_ps(_mm_cmpneq_ps(a.v_, b.v_)) != 0); }

  static value_type mag(const Vector4& v) { return _mm_cvtss_f32(mag_(v.v_)); }
  static inline Vector4 sign(const Vector4& v); // returns 1, -1 or 0 depending on sign of input
  static inline Vector4 rint(const Vector4& v); // round to nearest integer, halfway cases to even

  static Vector4 mask_ifzero(const Vector4& a, const Vector4& b)
    { return Vector4(_mm_and_ps(a.v_, _mm_cmpneq_ps(_mm_setzero_ps(), b.v_))); }

  static Vector4 mask_ifnonzero(const Vector4& a, const Vector4& b)
    { return Vector4(_mm_and_ps(a.v_, _mm_cmpeq_ps(_mm_setzero_ps(), b.v_))); }

  void normalize() { v_ = norm_(v_); }
  Vector4 normalized() const { return Vector4(norm_(v_)); }

  // Use the element accessors below instead of operator[] whereever
  // possible in order to avoid penalties due to partial reads or writes.
  value_type&       operator[](size_type i)       { return reinterpret_cast<float*>(&v_)[i]; }
  const value_type& operator[](size_type i) const { return reinterpret_cast<const float*>(&v_)[i]; }

  value_type x() const { return _mm_cvtss_f32(v_); }
  value_type y() const { return _mm_cvtss_f32(_mm_shuffle_ps(v_, v_, _MM_SHUFFLE(1,1,1,1))); }
  value_type z() const { return _mm_cvtss_f32(_mm_unpackhi_ps(v_, v_)); }
  value_type w() const { return _mm_cvtss_f32(_mm_shuffle_ps(v_, v_, _MM_SHUFFLE(3,3,3,3))); }
};

/*
 * Although the actual function call overhead would be small, inlining dot_(),
 * cross_() and sign() in fact reduces code size. And more importantly, it
 * avoids register save and restore overhead in computation chains.
 */
inline __m128 Vector4::dot_(__m128 a, __m128 b)
{
  __m128 c = _mm_mul_ps(a, b);
  __m128 d = _mm_shuffle_ps(c, c, _MM_SHUFFLE(2,3,0,1));

  c = _mm_add_ps(c, d);
  d = _mm_movehl_ps(d, c);

  return _mm_add_ss(c, d);
}

inline __m128 Vector4::cross_(__m128 a, __m128 b)
{
  // x = ay * bz - az * by
  // y = az * bx - ax * bz
  // z = ax * by - ay * bx
  // w = 0

  const __m128 a1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,0,2,1));
  const __m128 b1 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3,0,2,1));

  const __m128 c2 = _mm_sub_ps(_mm_mul_ps(a, b1), _mm_mul_ps(a1, b)); // (z,x,y,w)

  return _mm_shuffle_ps(c2, c2, _MM_SHUFFLE(3,0,2,1));
}

inline Vector4 Vector4::sign(const Vector4& v)
{
  const __m128 u = v.v_;
  const __m128 zero = _mm_setzero_ps();

  const __m128 n = _mm_cmplt_ps(u, zero);
  const __m128 p = _mm_cmplt_ps(zero, u);

  const __m128 neg1 = _mm_set1_ps(-1.f);

  // a - b calculated as (-b) - (-a) to avoid a register move.
  const __m128 s = _mm_sub_ps(_mm_and_ps(n, neg1), _mm_and_ps(p, neg1));

  return Vector4(s);
}

inline Vector4 Vector4::rint(const Vector4& v)
{
#if SOMATO_VECTOR_USE_SSE2
  return Vector4(_mm_cvtepi32_ps(_mm_cvtps_epi32(v.v_)));
#else
  return Vector4(Vector4::rint_(v.v_));
#endif
}

/*
 * Math::Matrix4 represents a 4x4 square matrix of single-precision
 * floating point scalars in column-major order, compatible with OpenGL.
 * Multiplication with another matrix and multiplication with a vector
 * are supported via operators.
 */
class Matrix4
{
private:
  __m128 m_[4];

  friend class Quat;

  enum Uninitialized { uninitialized };
  explicit Matrix4(Uninitialized) {}

  static void transpose_(const __m128* m, __m128* result);
  static void scale_(const __m128* a, __m128 s, __m128* result);
  static __m128 mul_(const __m128* a, __m128 b);
  static __m128 mul_(__m128 a, const __m128* b);
  static void   mul_(const __m128* a, const __m128* b, __m128* result);

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  Matrix4(); // load identity matrix
  constexpr Matrix4(const Vector4& c0, const Vector4& c1,
                    const Vector4& c2, const Vector4& c3 = {0.f, 0.f, 0.f, 1.f})
    : m_ {c0.v_, c1.v_, c2.v_, c3.v_} {}

  explicit Matrix4(const value_type b[][4]) { *this = b; }
  Matrix4& operator=(const value_type b[][4]);

  Matrix4& operator*=(const Matrix4& b) { mul_(m_, b.m_, m_); return *this; }

  friend Matrix4 operator*(const Matrix4& a, const Matrix4& b)
    { Matrix4 r (uninitialized); mul_(a.m_, b.m_, r.m_); return r; }

  friend Vector4 operator*(const Matrix4& a, const Vector4& b)
    { return Vector4(mul_(a.m_, b.v_)); }

  friend Vector4 operator*(const Vector4& a, const Matrix4& b)
    { return Vector4(mul_(a.v_, b.m_)); }

  void scale(float s) { scale_(m_, _mm_set_ss(s), m_); }
  Matrix4 scaled(float s) const
    { Matrix4 r (uninitialized); scale_(m_, _mm_set_ss(s), r.m_); return r; }

  void translate(const Vector4& t) { m_[3] = mul_(m_, t.v_); }
  Matrix4 translated(const Vector4& t) const
    { return {Vector4(m_[0]), Vector4(m_[1]), Vector4(m_[2]), Vector4(mul_(m_, t.v_))}; }

  void transpose() { transpose_(m_, m_); }
  Matrix4 transposed() const
    { Matrix4 r (uninitialized); transpose_(m_, r.m_); return r; }

  value_type*       operator[](size_type i)       { return reinterpret_cast<float*>(&m_[i]); }
  const value_type* operator[](size_type i) const { return reinterpret_cast<const float*>(&m_[i]); }
};

/*
 * Math::Quat represents a unit quaternion.  The interface provides
 * a subset of quaternion operations useful for rotation in 3-D space.
 */
class Quat
{
private:
  __m128 v_;

  explicit Quat(__m128 v) : v_ {v} {}

  static __m128 mul_(__m128 a, __m128 b);
  static __m128 from_axis_(const Vector4& a, __m128 phi);
  static void   to_matrix_(__m128 quat, __m128* result);
  static float  angle_(__m128 quat);

  static constexpr Quat from_axis_(float x, float y, float z, float s, float c)
    { return {x * s, y * s, z * s, c}; }
  static __m128 mask_xyz_()
    { const float m = -std::nanf("0x3FFFFF"); return _mm_setr_ps(m, m, m, 0.f); }

public:
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  constexpr Quat() : v_ {0.f, 0.f, 0.f, 1.f} {}
  constexpr Quat(value_type x_, value_type y_, value_type z_, value_type w_)
    : v_ {x_, y_, z_, w_} {}

  explicit Quat(const value_type* b) : v_ {_mm_loadu_ps(b)} {}
  Quat& operator=(const value_type* b) { v_ = _mm_loadu_ps(b); return *this; }

  static constexpr Quat from_axis(value_type x, value_type y, value_type z, value_type phi)
    { return from_axis_(x, y, z, std::sin(0.5f * phi), std::cos(0.5f * phi)); }

  static Quat from_axis(const Vector4& a, value_type phi)
    { return Quat(from_axis_(a, _mm_set_ss(phi))); }

  static Matrix4 to_matrix(const Quat& quat)
    { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat.v_, r.m_); return r; }

  Vector4 axis() const { return Vector4(_mm_and_ps(v_, mask_xyz_())); }
  value_type angle() const { return angle_(v_); }

  Quat& operator*=(const Quat& b) { v_ = mul_(v_, b.v_); return *this; }
  friend Quat operator*(const Quat& a, const Quat& b) { return Quat(mul_(a.v_, b.v_)); }

  Quat renormalized() const { return Quat(Vector4::norm_(v_)); }

  // Use the element accessors below instead of operator[] whereever
  // possible in order to avoid penalties due to partial reads or writes.
  value_type&       operator[](size_type i)       { return reinterpret_cast<float*>(&v_)[i]; }
  const value_type& operator[](size_type i) const { return reinterpret_cast<const float*>(&v_)[i]; }

  value_type x() const { return _mm_cvtss_f32(v_); }
  value_type y() const { return _mm_cvtss_f32(_mm_shuffle_ps(v_, v_, _MM_SHUFFLE(1,1,1,1))); }
  value_type z() const { return _mm_cvtss_f32(_mm_unpackhi_ps(v_, v_)); }
  value_type w() const { return _mm_cvtss_f32(_mm_shuffle_ps(v_, v_, _MM_SHUFFLE(3,3,3,3))); }
};

} // namespace Math

#else /* !SOMATO_VECTORMATH_H_INCLUDED || !SOMATO_VECTOR_USE_SSE */
# error "This header file should not be included directly"
#endif
