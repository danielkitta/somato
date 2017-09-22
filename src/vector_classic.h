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
 * along with Somato.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(SOMATO_VECTORMATH_H_INCLUDED) && !SOMATO_VECTOR_USE_SSE

namespace Math
{

class Vector4
{
private:
  float v_[4];

  static void rint_(const float* v, float* result);
  static void sign_(const float* v, float* result);
  static void mask_ifzero_   (const float* a, const float* b, float* result);
  static void mask_ifnonzero_(const float* a, const float* b, float* result);

public:
  typedef float         array_type[4];
  typedef float         value_type;
  typedef unsigned int  size_type;

  enum Uninitialized { uninitialized };

  static void  add_  (const float* a, const float* b, float* result);
  static void  sub_  (const float* a, const float* b, float* result);
  static void  mul_  (const float* a, float b, float* result);
  static void  div_  (const float* a, float b, float* result);
  static float dot_  (const float* a, const float* b);
  static void  cross_(const float* a, const float* b, float* result);
  static bool  equal_(const float* a, const float* b);

  explicit Vector4(Uninitialized) {}

  void assign(value_type x_, value_type y_, value_type z_, value_type w_ = 0.0)
    { v_[0] = x_; v_[1] = y_; v_[2] = z_; v_[3] = w_; }

  Vector4() { v_[0] = 0.0; v_[1] = 0.0; v_[2] = 0.0; v_[3] = 0.0; }
  Vector4(const Vector4& b) { assign(b.v_[0], b.v_[1], b.v_[2], b.v_[3]); }
  explicit Vector4(const value_type* b) { assign(b[0], b[1], b[2], b[3]); }

  Vector4(value_type x_, value_type y_, value_type z_, value_type w_ = 0.0)
    { v_[0] = x_; v_[1] = y_; v_[2] = z_; v_[3] = w_; }

  Vector4& operator=(const Vector4& b) { assign(b.v_[0], b.v_[1], b.v_[2], b.v_[3]); return *this; }
  Vector4& operator=(const value_type* b) { assign(b[0], b[1], b[2], b[3]); return *this; }

  value_type*       data()       { return v_; }
  const value_type* data() const { return v_; }

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

  static value_type mag(const value_type* v);
  static value_type mag(const Vector4& v) { return mag(v.v_); }

  static inline Vector4 rint(const Vector4& v);
  static inline Vector4 rint(const value_type* v);
  static inline Vector4 sign(const Vector4& v);
  static inline Vector4 sign(const value_type* v);

  static inline Vector4 mask_ifzero(const Vector4& a,    const Vector4& b);
  static inline Vector4 mask_ifzero(const value_type* a, const Vector4& b);
  static inline Vector4 mask_ifzero(const Vector4& a,    const value_type* b);
  static inline Vector4 mask_ifzero(const value_type* a, const value_type* b);

  static inline Vector4 mask_ifnonzero(const Vector4& a,    const Vector4& b);
  static inline Vector4 mask_ifnonzero(const value_type* a, const Vector4& b);
  static inline Vector4 mask_ifnonzero(const Vector4& a,    const value_type* b);
  static inline Vector4 mask_ifnonzero(const value_type* a, const value_type* b);
};

inline Vector4 Vector4::rint(const Vector4& v)
  { Vector4 r (uninitialized); rint_(v.v_, r.v_); return r; }

inline Vector4 Vector4::rint(const Vector4::value_type* v)
  { Vector4 r (uninitialized); rint_(v, r.v_); return r; }

inline Vector4 Vector4::sign(const Vector4& v)
  { Vector4 r (uninitialized); sign_(v.v_, r.v_); return r; }

inline Vector4 Vector4::sign(const Vector4::value_type* v)
  { Vector4 r (uninitialized); sign_(v, r.v_); return r; }

inline Vector4 Vector4::mask_ifzero(const Vector4& a, const Vector4& b)
  { Vector4 r (uninitialized); mask_ifzero_(a.v_, b.v_, r.v_); return r; }

inline Vector4 Vector4::mask_ifzero(const Vector4& a, const Vector4::value_type* b)
  { Vector4 r (uninitialized); mask_ifzero_(a.v_, b, r.v_); return r; }

inline Vector4 Vector4::mask_ifzero(const Vector4::value_type* a, const Vector4& b)
  { Vector4 r (uninitialized); mask_ifzero_(a, b.v_, r.v_); return r; }

inline Vector4 Vector4::mask_ifzero(const Vector4::value_type* a, const Vector4::value_type* b)
  { Vector4 r (uninitialized); mask_ifzero_(a, b, r.v_); return r; }

inline Vector4 Vector4::mask_ifnonzero(const Vector4& a, const Vector4& b)
  { Vector4 r (uninitialized); mask_ifnonzero_(a.v_, b.v_, r.v_); return r; }

inline Vector4 Vector4::mask_ifnonzero(const Vector4& a, const Vector4::value_type* b)
  { Vector4 r (uninitialized); mask_ifnonzero_(a.v_, b, r.v_); return r; }

inline Vector4 Vector4::mask_ifnonzero(const Vector4::value_type* a, const Vector4& b)
  { Vector4 r (uninitialized); mask_ifnonzero_(a, b.v_, r.v_); return r; }

inline Vector4 Vector4::mask_ifnonzero(const Vector4::value_type* a, const Vector4::value_type* b)
  { Vector4 r (uninitialized); mask_ifnonzero_(a, b, r.v_); return r; }

inline Vector4 operator+(const Vector4& v)
  { return Vector4(v); }

inline Vector4 operator-(const Vector4& v)
  { return Vector4(-v[0], -v[1], -v[2], -v[3]); }

inline Vector4 operator+(const Vector4& a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::add_(a.data(), b.data(), r.data()); return r; }

inline Vector4 operator+(const Vector4& a, const Vector4::value_type* b)
  { Vector4 r (Vector4::uninitialized); Vector4::add_(a.data(), b, r.data()); return r; }

inline Vector4 operator+(const Vector4::value_type* a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::add_(a, b.data(), r.data()); return r; }

inline Vector4 operator-(const Vector4& a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::sub_(a.data(), b.data(), r.data()); return r; }

inline Vector4 operator-(const Vector4& a, const Vector4::value_type* b)
  { Vector4 r (Vector4::uninitialized); Vector4::sub_(a.data(), b, r.data()); return r; }

inline Vector4 operator-(const Vector4::value_type* a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::sub_(a, b.data(), r.data()); return r; }

inline Vector4 operator*(const Vector4& a, Vector4::value_type b)
  { Vector4 r (Vector4::uninitialized); Vector4::mul_(a.data(), b, r.data()); return r; }

inline Vector4 operator*(Vector4::value_type a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::mul_(b.data(), a, r.data()); return r; }

inline Vector4 operator/(const Vector4& a, Vector4::value_type b)
  { Vector4 r (Vector4::uninitialized); Vector4::div_(a.data(), b, r.data()); return r; }

inline Vector4::value_type operator*(const Vector4& a, const Vector4& b)
  { return Vector4::dot_(a.data(), b.data()); }

inline Vector4::value_type operator*(const Vector4& a, const Vector4::value_type* b)
  { return Vector4::dot_(a.data(), b); }

inline Vector4::value_type operator*(const Vector4::value_type* a, const Vector4& b)
  { return Vector4::dot_(a, b.data()); }

inline Vector4 operator%(const Vector4& a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::cross_(a.data(), b.data(), r.data()); return r; }

inline Vector4 operator%(const Vector4& a, const Vector4::value_type* b)
  { Vector4 r (Vector4::uninitialized); Vector4::cross_(a.data(), b, r.data()); return r; }

inline Vector4 operator%(const Vector4::value_type* a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Vector4::cross_(a, b.data(), r.data()); return r; }

inline bool operator==(const Vector4& a, const Vector4& b)
  { return Vector4::equal_(a.data(), b.data()); }

inline bool operator==(const Vector4& a, const Vector4::value_type* b)
  { return Vector4::equal_(a.data(), b); }

inline bool operator==(const Vector4::value_type* a, const Vector4& b)
  { return Vector4::equal_(a, b.data()); }

inline bool operator!=(const Vector4& a, const Vector4& b)
  { return !Vector4::equal_(a.data(), b.data()); }

inline bool operator!=(const Vector4& a, const Vector4::value_type* b)
  { return !Vector4::equal_(a.data(), b); }

inline bool operator!=(const Vector4::value_type* a, const Vector4& b)
  { return !Vector4::equal_(a, b.data()); }


class Matrix4
{
private:
  float m_[4][4];

  void assign_(const float* c0, const float* c1, const float* c2, const float* c3);

public:
  typedef Vector4::array_type array_type[4];
  typedef Vector4::array_type column_type;
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  enum Uninitialized { uninitialized };

  static const array_type identity;

  static void mul_(const float a[][4], const float* b, float* result);
  static void mul_(const float a[][4], const float b[][4], float result[][4]);

  explicit Matrix4(Uninitialized) {}

  void assign(const value_type b[][4]);

  Matrix4()                                 { assign(identity); }
  Matrix4(const Matrix4& b)                 { assign(b.m_); }
  explicit Matrix4(const value_type b[][4]) { assign(b); }

  inline Matrix4(const Vector4& c0, const Vector4& c1, const Vector4& c2, const Vector4& c3);

  Matrix4& operator=(const Matrix4& b)        { assign(b.m_); return *this; }
  Matrix4& operator=(const value_type b[][4]) { assign(b);    return *this; }

  Matrix4& operator*=(const Matrix4& b)        { mul_(m_, b.m_, m_); return *this; }
  Matrix4& operator*=(const value_type b[][4]) { mul_(m_, b, m_);    return *this; }

  void transpose();

  column_type*       data()       { return m_; }
  const column_type* data() const { return m_; }

  value_type*       operator[](size_type i)       { return m_[i]; }
  const value_type* operator[](size_type i) const { return m_[i]; }
};

inline Matrix4::Matrix4(const Vector4& c0, const Vector4& c1, const Vector4& c2, const Vector4& c3)
  { assign_(c0.data(), c1.data(), c2.data(), c3.data()); }

inline Vector4 operator*(const Matrix4& a, const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Matrix4::mul_(a.data(), b.data(), r.data()); return r; }

inline Vector4 operator*(const Matrix4& a, const Vector4::value_type* b)
  { Vector4 r (Vector4::uninitialized); Matrix4::mul_(a.data(), b, r.data()); return r; }

inline Vector4 operator*(const Matrix4::value_type a[][4], const Vector4& b)
  { Vector4 r (Vector4::uninitialized); Matrix4::mul_(a, b.data(), r.data()); return r; }

inline Matrix4 operator*(const Matrix4& a, const Matrix4& b)
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a.data(), b.data(), r.data()); return r; }

inline Matrix4 operator*(const Matrix4& a, const Matrix4::value_type b[][4])
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a.data(), b, r.data()); return r; }

inline Matrix4 operator*(const Matrix4::value_type a[][4], const Matrix4& b)
  { Matrix4 r (Matrix4::uninitialized); Matrix4::mul_(a, b.data(), r.data()); return r; }


class Quat
{
private:
  float v_[4];

  static void from_axis_(const float* a, float phi, float* result);
  static void to_matrix_(const float* quat, float result[][4]);

public:
  typedef Vector4::array_type array_type;
  typedef Vector4::value_type value_type;
  typedef Vector4::size_type  size_type;

  enum Uninitialized { uninitialized };

  static void mul_(const float* a, const float* b, float* result);

  explicit Quat(Uninitialized) {}

  void assign(value_type x_, value_type y_, value_type z_, value_type w_)
    { v_[0] = x_; v_[1] = y_; v_[2] = z_; v_[3] = w_; }

  Quat() { v_[0] = 0.0; v_[1] = 0.0; v_[2] = 0.0; v_[3] = 1.0; }
  Quat(const Quat& b) { assign(b.v_[0], b.v_[1], b.v_[2], b.v_[3]); }
  explicit Quat(const value_type* b) { assign(b[0], b[1], b[2], b[3]); }

  Quat(value_type x_, value_type y_, value_type z_, value_type w_)
    { v_[0] = x_; v_[1] = y_; v_[2] = z_; v_[3] = w_; }

  Quat& operator=(const Quat& b) { assign(b.v_[0], b.v_[1], b.v_[2], b.v_[3]); return *this; }
  Quat& operator=(const value_type* b) { assign(b[0], b[1], b[2], b[3]); return *this; }

  static inline Quat    from_axis(const Vector4& a, value_type phi);
  static inline Quat    from_axis(const Vector4::value_type* a, value_type phi);
  static inline Matrix4 to_matrix(const Quat& quat);
  static inline Matrix4 to_matrix(const value_type* quat);

  inline Vector4 axis()  const; // returns (x,y,z,0)
  value_type     angle() const; // extracts the rotation angle phi

  Quat& operator*=(const Quat& b)       { mul_(v_, b.v_, v_); return *this; }
  Quat& operator*=(const value_type* b) { mul_(v_, b, v_);    return *this; }

  // Renormalize if the norm is off by more than epsilon.
  void renormalize(value_type epsilon);

  value_type*       data()       { return v_; }
  const value_type* data() const { return v_; }

  value_type&       operator[](size_type i)       { return v_[i]; }
  const value_type& operator[](size_type i) const { return v_[i]; }

  value_type x() const { return v_[0]; }
  value_type y() const { return v_[1]; }
  value_type z() const { return v_[2]; }
  value_type w() const { return v_[3]; }
};

// static
inline Quat Quat::from_axis(const Vector4& a, Quat::value_type phi)
  { Quat r (uninitialized); from_axis_(a.data(), phi, r.v_); return r; }

// static
inline Quat Quat::from_axis(const Vector4::value_type* a, Quat::value_type phi)
  { Quat r (uninitialized); from_axis_(a, phi, r.v_); return r; }

// static
inline Matrix4 Quat::to_matrix(const Quat& quat)
  { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat.v_, r.data()); return r; }

// static
inline Matrix4 Quat::to_matrix(const Quat::value_type* quat)
  { Matrix4 r (Matrix4::uninitialized); to_matrix_(quat, r.data()); return r; }

inline Vector4 Quat::axis() const
  { return Vector4(v_[0], v_[1], v_[2]); }

inline Quat operator*(const Quat& a, const Quat& b)
  { Quat r (Quat::uninitialized); Quat::mul_(a.data(), b.data(), r.data()); return r; }

inline Quat operator*(const Quat& a, const Quat::value_type* b)
  { Quat r (Quat::uninitialized); Quat::mul_(a.data(), b, r.data()); return r; }

inline Quat operator*(const Quat::value_type* a, const Quat& b)
  { Quat r (Quat::uninitialized); Quat::mul_(a, b.data(), r.data()); return r; }

} // namespace Math

#else /* !SOMATO_VECTORMATH_H_INCLUDED || SOMATO_VECTOR_USE_SSE */
# error "This header file should not be included directly"
#endif
