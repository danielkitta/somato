/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_VECTORMATH_H_INCLUDED
# error "This header file should not be included directly"
#endif

#include <xmmintrin.h>

#if SOMATO_VECTOR_USE_SSE2
# include <emmintrin.h>
#elif !defined(_MSC_VER)
# include <cfloat> /* see Math::Quat::mask_xyz_() */
#endif

#if !SOMATO_HAVE__MM_CVTSS_F32 && defined(__GNUC__)
// Missing intrinsic: http://gcc.gnu.org/ml/gcc-patches/2006-01/msg01759.html
static inline float __attribute__((__always_inline__))
_mm_cvtss_f32(__m128 v) { return __builtin_ia32_vec_ext_v4sf(v, 0); }
#endif

#ifdef _MSC_VER
// HACK: Allow build with old CRT headers but more capable compiler
__if_not_exists(_mm_cvtss_f32)
{
# if (_MSC_VER >= 1500)
  extern "C" float _mm_cvtss_f32(__m128);
# else
#  define _mm_cvtss_f32(v) ((v).m128_f32[0])
# endif
}
#endif /* _MSC_VER */

namespace Math
{

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

#if !SOMATO_VECTOR_USE_SSE2
  static __m128 rint_(__m128 v);
#endif
  static __m128 mag_(__m128 v); // magnitude of (x,y,z,w)

public:
  typedef __m128        array_type;
  typedef float         value_type;
  typedef unsigned int  size_type;

  static        __m128 dot_  (__m128 a, __m128 b); // scalar product across (x,y,z,w)
  static inline __m128 cross_(__m128 a, __m128 b); // 3-D vector product, returns w = 0

  inline Vector4();
  Vector4(const Vector4& b) : v_ (b.v_) {}
  Vector4(array_type b) : v_ (b) {}
  explicit inline Vector4(const value_type* b);
  inline Vector4(value_type x_, value_type y_, value_type z_, value_type w_ = 0.0f);

  Vector4& operator=(const Vector4& b) { v_ = b.v_; return *this; }
  inline Vector4& operator=(const value_type* b);

  inline void assign(value_type x_, value_type y_, value_type z_, value_type w_ = 0.0f);

  Vector4& operator+=(const Vector4& b) { v_ = _mm_add_ps(v_, b.v_); return *this; }
  Vector4& operator-=(const Vector4& b) { v_ = _mm_sub_ps(v_, b.v_); return *this; }
  Vector4& operator*=(value_type b) { v_ = _mm_mul_ps(v_, _mm_set1_ps(b)); return *this; }
  Vector4& operator/=(value_type b) { v_ = _mm_div_ps(v_, _mm_set1_ps(b)); return *this; }
  Vector4& operator%=(const Vector4& b);

  static value_type     mag (const Vector4& v) { return _mm_cvtss_f32(mag_(v.v_)); }
  static inline Vector4 sign(const Vector4& v); // returns 1, -1 or 0 depending on sign of input
  static inline Vector4 rint(const Vector4& v); // round to nearest integer, halfway cases to even

  static inline Vector4 mask_ifzero   (const Vector4& a, const Vector4& b);
  static inline Vector4 mask_ifnonzero(const Vector4& a, const Vector4& b);

  array_type data() const { return v_; }

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
 * Although the actual function call overhead would be small, inlining
 * cross_() and sign() in fact reduces code size.  And more importantly,
 * it avoids register save and restore overhead in computation chains.
 */
// static
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

// static
inline Vector4 Vector4::sign(const Vector4& v)
{
  const __m128 u = v.v_;
  const __m128 zero = _mm_setzero_ps();

  const __m128 n = _mm_cmplt_ps(u, zero);
  const __m128 p = _mm_cmplt_ps(zero, u);

  static const __m128 neg1 = { -1.0f, -1.0f, -1.0f, -1.0f };

  // a - b calculated as (-b) - (-a) to avoid a register move.
  const __m128 s = _mm_sub_ps(_mm_and_ps(n, neg1), _mm_and_ps(p, neg1));

  return Vector4(s);
}

// static
inline Vector4 Vector4::rint(const Vector4& v)
{
#if SOMATO_VECTOR_USE_SSE2
  return Vector4(_mm_cvtepi32_ps(_mm_cvtps_epi32(v.v_)));
#else
  return Vector4(Vector4::rint_(v.v_));
#endif
}

// static
inline Vector4 Vector4::mask_ifzero(const Vector4& a, const Vector4& b)
  { return Vector4(_mm_and_ps(a.data(), _mm_cmpneq_ps(_mm_setzero_ps(), b.data()))); }

// static
inline Vector4 Vector4::mask_ifnonzero(const Vector4& a, const Vector4& b)
  { return Vector4(_mm_and_ps(a.data(), _mm_cmpeq_ps(_mm_setzero_ps(), b.data()))); }

inline Vector4::Vector4()
  : v_ (_mm_setzero_ps()) {}

inline Vector4::Vector4(const Vector4::value_type* b)
  : v_ (_mm_loadu_ps(b)) {}

inline Vector4::Vector4(Vector4::value_type x_, Vector4::value_type y_,
                        Vector4::value_type z_, Vector4::value_type w_)
  : v_ (_mm_setr_ps(x_, y_, z_, w_)) {}

inline Vector4& Vector4::operator=(const Vector4::value_type* b)
  { v_ = _mm_loadu_ps(b); return *this; }

inline void Vector4::assign(Vector4::value_type x_, Vector4::value_type y_,
                            Vector4::value_type z_, Vector4::value_type w_)
  { v_ = _mm_setr_ps(x_, y_, z_, w_); }

inline Vector4& Vector4::operator%=(const Vector4& b)
  { v_ = Vector4::cross_(v_, b.v_); return *this; }

inline Vector4 operator+(const Vector4& v)
  { return Vector4(v.data());  }

inline Vector4 operator-(const Vector4& v)
{
  static const __m128 signmask = { -0.0f, -0.0f, -0.0f, -0.0f };
  return Vector4(_mm_xor_ps(v.data(), signmask));
}

inline Vector4 operator+(const Vector4& a, const Vector4& b)
  { return Vector4(_mm_add_ps(a.data(), b.data())); }

inline Vector4 operator-(const Vector4& a, const Vector4& b)
  { return Vector4(_mm_sub_ps(a.data(), b.data())); }

inline Vector4 operator*(const Vector4& a, Vector4::value_type b)
  { return Vector4(_mm_mul_ps(a.data(), _mm_set1_ps(b))); }

inline Vector4 operator*(Vector4::value_type a, const Vector4& b)
  { return Vector4(_mm_mul_ps(_mm_set1_ps(a), b.data())); }

inline Vector4 operator/(const Vector4& a, Vector4::value_type b)
  { return Vector4(_mm_div_ps(a.data(), _mm_set1_ps(b))); }

inline Vector4::value_type operator*(const Vector4& a, const Vector4& b)
  { return _mm_cvtss_f32(Vector4::dot_(a.data(), b.data())); }

inline Vector4 operator%(const Vector4& a, const Vector4& b)
  { return Vector4(Vector4::cross_(a.data(), b.data())); }

inline bool operator==(const Vector4& a, const Vector4& b)
  { return (_mm_movemask_ps(_mm_cmpneq_ps(a.data(), b.data())) == 0); }

inline bool operator!=(const Vector4& a, const Vector4& b)
  { return (_mm_movemask_ps(_mm_cmpneq_ps(a.data(), b.data())) != 0); }

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

public:
  typedef Vector4::array_type array_type[4];
  typedef Vector4::array_type column_type;
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  enum Uninitialized { uninitialized };

  static const array_type identity;

  static __m128 mul_(const __m128* a, __m128 b);
  static void   mul_(const __m128* a, const __m128* b, __m128* result);

  explicit Matrix4(Uninitialized) {}

  void assign(const column_type* b);
  void assign(const value_type b[][4]);

  Matrix4()                                 { assign(identity); }
  Matrix4(const Matrix4& b)                 { assign(b.m_); }
  explicit Matrix4(const column_type* b)    { assign(b); }
  explicit Matrix4(const value_type b[][4]) { assign(b); }

  inline Matrix4(const Vector4& c0, const Vector4& c1, const Vector4& c2, const Vector4& c3);

  Matrix4& operator=(const Matrix4& b)        { assign(b.m_); return *this; }
  Matrix4& operator=(const column_type* b)    { assign(b);    return *this; }
  Matrix4& operator=(const value_type b[][4]) { assign(b);    return *this; }

  Matrix4& operator*=(const Matrix4& b)     { mul_(m_, b.m_, m_); return *this; }
  Matrix4& operator*=(const column_type* b) { mul_(m_, b, m_);    return *this; }

  void transpose();

  column_type*       data()       { return m_; }
  const column_type* data() const { return m_; }

  value_type*       operator[](size_type i)       { return reinterpret_cast<float*>(&m_[i]); }
  const value_type* operator[](size_type i) const { return reinterpret_cast<const float*>(&m_[i]); }
};

inline Matrix4::Matrix4(const Vector4& c0, const Vector4& c1, const Vector4& c2, const Vector4& c3)
  { m_[0] = c0.data(); m_[1] = c1.data(); m_[2] = c2.data(); m_[3] = c3.data(); }

inline Vector4 operator*(const Matrix4& a, const Vector4& b)
  { return Vector4(Matrix4::mul_(a.data(), b.data())); }

inline Matrix4 operator*(const Matrix4& a, const Matrix4& b)
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a.data(), b.data(), r.data()); return r; }

inline Matrix4 operator*(const Matrix4& a, const Matrix4::column_type* b)
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a.data(), b, r.data()); return r; }

inline Matrix4 operator*(const Matrix4::column_type* a, const Matrix4& b)
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a, b.data(), r.data()); return r; }

/*
 * Math::Quat represents a unit quaternion.  The interface provides
 * a subset of quaternion operations useful for rotation in 3-D space.
 */
class Quat
{
private:
  __m128 v_;

  static inline __m128 mask_xyz_();
  static __m128 from_axis_(const Vector4& a, __m128 phi);
  static void   to_matrix_(__m128 quat, __m128* result);
  static float  angle_(__m128 quat);
  static __m128 renormalize_(__m128 quat, __m128 epsilon);

public:
  typedef Vector4::array_type array_type;
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  static __m128 mul_(__m128 a, __m128 b);

  inline Quat();
  inline Quat(value_type x_, value_type y_, value_type z_, value_type w_);
  Quat(const Quat& b) : v_ (b.v_) {}
  Quat(array_type b)  : v_ (b) {}
  explicit inline Quat(const value_type* b);

  inline void assign(value_type x_, value_type y_, value_type z_, value_type w_);

  Quat& operator=(const Quat& b) { v_ = b.v_; return *this; }
  inline Quat& operator=(const value_type* b);

  static inline Quat    from_axis(const Vector4& a, value_type phi);
  static inline Matrix4 to_matrix(const Quat& quat);

  inline Vector4    axis()  const; // returns (x,y,z,0)
  inline value_type angle() const; // extracts the rotation angle phi

  Quat& operator*=(const Quat& b) { v_ = mul_(v_, b.v_); return *this; }

  // Renormalize if the norm is off by more than epsilon.
  inline void renormalize(value_type epsilon);

  array_type data() const { return v_; }

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
 * Compute the mask {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000} using
 * only floating point expressions that can be evaluated at compile time.
 *
 * Note that this rules out the mask-returning comparison intrinsics like
 * _mm_cmpneq_ps(), as gcc does not contract them.  Also, although the
 * expression copysignf(nanf("0x7FFFFF"), -1.0f) does yield the desired
 * result with both GCC and ICC, the Intel compiler does not evaluate
 * these C99 function calls at compile time.
 */
// static
inline __m128 Quat::mask_xyz_()
{
#if defined(_MSC_VER)
  static const union { int i[4]; __m128 v; } mask = { { -1, -1, -1, 0 } };
  return mask.v;
#elif SOMATO_VECTOR_USE_SSE2
  return _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0));
#else
  // Even with this, ICC will defer the OR to runtime.  Well duh.
  return _mm_or_ps(_mm_setr_ps(-FLT_MIN, -FLT_MIN, -FLT_MIN, 0.0f),
                   _mm_setr_ps(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f));
#endif
}

inline Quat::Quat()
  : v_ (Matrix4::identity[3]) {}

inline Quat::Quat(Quat::value_type x_, Quat::value_type y_,
                  Quat::value_type z_, Quat::value_type w_)
  : v_ (_mm_setr_ps(x_, y_, z_, w_)) {}

inline Quat::Quat(const Quat::value_type* b)
  : v_ (_mm_loadu_ps(b)) {}

inline void Quat::assign(Quat::value_type x_, Quat::value_type y_,
                         Quat::value_type z_, Quat::value_type w_)
  { v_ = _mm_setr_ps(x_, y_, z_, w_); }

inline Quat& Quat::operator=(const Quat::value_type* b)
  { v_ = _mm_loadu_ps(b); return *this; }

// static
inline Quat Quat::from_axis(const Vector4& a, Quat::value_type phi)
  { return Quat(from_axis_(a, _mm_set_ss(phi))); }

// static
inline Matrix4 Quat::to_matrix(const Quat& quat)
  { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat.v_, r.data()); return r; }

inline Vector4 Quat::axis() const
  { return Vector4(_mm_and_ps(v_, Quat::mask_xyz_())); }

inline Quat::value_type Quat::angle() const
  { return angle_(v_); }

inline void Quat::renormalize(Quat::value_type epsilon)
  { v_ = renormalize_(v_, _mm_set_ss(epsilon)); }

inline Quat operator*(const Quat& a, const Quat& b)
  { return Quat(Quat::mul_(a.data(), b.data())); }

/*
 * Vector233 encapsulates one float vector with two elements plus two float
 * vectors with three elements each in a compact 32-byte structure.  This is
 * the SSE implementation, thus the internal representation is an array of
 * two 128-bit vectors.
 *
 * Unfortunately, the std::vector<> implementation of Microsoft Visual C++ 9
 * defines the default value parameter to std::vector<T>::replace(n, value)
 * as by-value parameter instead of a const reference.  As a result, trying to
 * instantiate std::vector<Vector233> produces an error message because the
 * by-value parameter cannot be 128-bit aligned.
 *
 * Now, I'm pretty certain that aggregate types, or at least those exceeding
 * machine register size, are internally passed by reference anyway, thus the
 * ABI constraint shouldn't apply here.  Unfortunately I wasn't able to
 * convince the compiler of this, and thus had to adapt the implementation of
 * Vector233 for MSVC targets to avoid the alignment constraint.  Naturally,
 * this will impact performance, but I couldn't be bothered to reimplement the
 * whole of std::vector<> just for MSVC.
 */
struct Vector233
{
#ifdef _MSC_VER
  Vector4::value_type data[8];
#else
  Vector4::array_type data[2];
#endif
  inline void set(Vector4::array_type a, Vector4::array_type b, Vector4::array_type c);

  inline Vector233();
  inline Vector233(const Vector4& a, const Vector4& b, const Vector4& c);
  inline Vector233(const Vector233& b);
  inline Vector233& operator=(const Vector233& b);
};

inline void Vector233::set(Vector4::array_type a, Vector4::array_type b, Vector4::array_type c)
{
  const __m128 v = _mm_movelh_ps(a, b);
  const __m128 u = _mm_shuffle_ps(_mm_move_ss(b, c), c, _MM_SHUFFLE(2,1,0,2));
#ifdef _MSC_VER
  _mm_storeu_ps(&data[0], v);
  _mm_storeu_ps(&data[4], u);
#else
  data[0] = v;
  data[1] = u;
#endif /* !_MSC_VER */
}

inline Vector233::Vector233()
{
  const __m128 zero = _mm_setzero_ps();
#ifdef _MSC_VER
  _mm_storeu_ps(&data[0], zero);
  _mm_storeu_ps(&data[4], zero);
#else
  data[0] = zero;
  data[1] = zero;
#endif /* !_MSC_VER */
}

inline
Vector233::Vector233(const Vector4& a, const Vector4& b, const Vector4& c)
{
  set(a.data(), b.data(), c.data());
}

inline
Vector233::Vector233(const Vector233& b)
{
#ifdef _MSC_VER
  const __m128 v = _mm_loadu_ps(&b.data[0]);
  const __m128 u = _mm_loadu_ps(&b.data[4]);

  _mm_storeu_ps(&data[0], v);
  _mm_storeu_ps(&data[4], u);
#else
  const __m128 v = b.data[0];
  const __m128 u = b.data[1];

  data[0] = v;
  data[1] = u;
#endif /* !_MSC_VER */
}

inline
Vector233& Vector233::operator=(const Vector233& b)
{
#ifdef _MSC_VER
  const __m128 v = _mm_loadu_ps(&b.data[0]);
  const __m128 u = _mm_loadu_ps(&b.data[4]);

  _mm_storeu_ps(&data[0], v);
  _mm_storeu_ps(&data[4], u);
#else
  const __m128 v = b.data[0];
  const __m128 u = b.data[1];

  data[0] = v;
  data[1] = u;
#endif /* !_MSC_VER */
  return *this;
}

inline
bool operator==(const Vector233& a, const Vector233& b)
{
#ifdef _MSC_VER
  const __m128 m0 = _mm_cmpneq_ps(_mm_loadu_ps(&a.data[0]), _mm_loadu_ps(&b.data[0]));
  const __m128 m1 = _mm_cmpneq_ps(_mm_loadu_ps(&a.data[4]), _mm_loadu_ps(&b.data[4]));
#else
  const __m128 m0 = _mm_cmpneq_ps(a.data[0], b.data[0]);
  const __m128 m1 = _mm_cmpneq_ps(a.data[1], b.data[1]);
#endif
  return (_mm_movemask_ps(_mm_or_ps(m0, m1)) == 0);
}

inline
bool operator!=(const Vector233& a, const Vector233& b)
{
  return !(a == b);
}

} // namespace Math
