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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace
{

#if !SOMATO_HAVE_LRINTF
/*
 * For systems without C99 rounding functions, emulate rint() as it would
 * behave if the default rounding direction is selected.  That is, round to
 * the nearest integer, with halfway cases to nearest even.
 */
static
float round_nearest(float r)
{
  const float ri = std::floor(r + 0.5f);

  if (ri == r + 0.5f)
    return 2.0f * std::floor(ri / 2.0f);
  else
    return ri;
}
#endif /* !SOMATO_HAVE_LRINTF */

static inline
void set_vector(float* result, float x, float y, float z, float w)
{
  result[0] = x;
  result[1] = y;
  result[2] = z;
  result[3] = w;
}

static inline
float vector3_mag(const float* v)
{
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

} // anonymous namespace


namespace Math
{

// static
void Vector4::sign_(const float* v, float* result)
{
  set_vector(result, int(v[0] > 0.0f) - int(v[0] < 0.0f),
                     int(v[1] > 0.0f) - int(v[1] < 0.0f),
                     int(v[2] > 0.0f) - int(v[2] < 0.0f),
                     int(v[3] > 0.0f) - int(v[3] < 0.0f));
}

// static
void Vector4::rint_(const float* v, float* result)
{
#if SOMATO_HAVE_LRINTF
  // Boldly assume the default rounding style.  This way, shooting yourself
  // into the foot by changing the rounding direction non-temporarily becomes
  // an even more delighting experience than usual.
  set_vector(result, lrintf(v[0]), lrintf(v[1]), lrintf(v[2]), lrintf(v[3]));
#else
  set_vector(result, round_nearest(v[0]), round_nearest(v[1]),
                     round_nearest(v[2]), round_nearest(v[3]));
#endif
}

// static
void Vector4::mask_ifzero_(const float* a, const float* b, float* result)
{
  set_vector(result, (b[0] == 0.0f) ? 0.0f : a[0],
                     (b[1] == 0.0f) ? 0.0f : a[1],
                     (b[2] == 0.0f) ? 0.0f : a[2],
                     (b[3] == 0.0f) ? 0.0f : a[3]);
}

// static
void Vector4::mask_ifnonzero_(const float* a, const float* b, float* result)
{
  set_vector(result, (b[0] != 0.0f) ? 0.0f : a[0],
                     (b[1] != 0.0f) ? 0.0f : a[1],
                     (b[2] != 0.0f) ? 0.0f : a[2],
                     (b[3] != 0.0f) ? 0.0f : a[3]);
}

// static
void Vector4::add_(const float* a, const float* b, float* result)
{
  set_vector(result, a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]);
}

// static
void Vector4::sub_(const float* a, const float* b, float* result)
{
  set_vector(result, a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]);
}

// static
void Vector4::mul_(const float* a, float b, float* result)
{
  set_vector(result, a[0] * b, a[1] * b, a[2] * b, a[3] * b);
}

// static
void Vector4::div_(const float* a, float b, float* result)
{
  set_vector(result, a[0] / b, a[1] / b, a[2] / b, a[3] / b);
}

// static
float Vector4::dot_(const float* a, const float* b)
{
  return (a[0] * b[0] + a[1] * b[1]) + (a[2] * b[2] + a[3] * b[3]);
}

// static
void Vector4::cross_(const float* a, const float* b, float* result)
{
  set_vector(result, a[1] * b[2] - a[2] * b[1],
                     a[2] * b[0] - a[0] * b[2],
                     a[0] * b[1] - a[1] * b[0],
                     0.0);
}

// static
bool Vector4::equal_(const float* a, const float* b)
{
  return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

// static
Vector4::value_type Vector4::mag(const Vector4::value_type* v)
{
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
}

const Matrix4::array_type Matrix4::identity =
{
  { 1.0, 0.0, 0.0, 0.0 },
  { 0.0, 1.0, 0.0, 0.0 },
  { 0.0, 0.0, 1.0, 0.0 },
  { 0.0, 0.0, 0.0, 1.0 }
};

void Matrix4::assign(const Matrix4::value_type b[][4])
{
  // It is assumed that m_[] and b[] either refer to the same location
  // in memory or are completely distinct, i.e. not partially overlapping.

  if (&m_[0] != &b[0])
    std::memcpy(m_, b, sizeof(m_));
}

void Matrix4::transpose()
{
  std::swap(m_[1][0], m_[0][1]);
  std::swap(m_[2][0], m_[0][2]);
  std::swap(m_[3][0], m_[0][3]);
  std::swap(m_[2][1], m_[1][2]);
  std::swap(m_[3][1], m_[1][3]);
  std::swap(m_[3][2], m_[2][3]);
}

void Matrix4::assign_(const float* c0, const float* c1, const float* c2, const float* c3)
{
  set_vector(m_[0], c0[0], c0[1], c0[2], c0[3]);
  set_vector(m_[1], c1[0], c1[1], c1[2], c1[3]);
  set_vector(m_[2], c2[0], c2[1], c2[2], c2[3]);
  set_vector(m_[3], c3[0], c3[1], c3[2], c3[3]);
}

// static
void Matrix4::mul_(const float a[][4], const float* b, float* result)
{
  const float b0 = b[0];
  const float b1 = b[1];
  const float b2 = b[2];
  const float b3 = b[3];

  result[0] = a[0][0] * b0 + a[1][0] * b1 + a[2][0] * b2 + a[3][0] * b3;
  result[1] = a[0][1] * b0 + a[1][1] * b1 + a[2][1] * b2 + a[3][1] * b3;
  result[2] = a[0][2] * b0 + a[1][2] * b1 + a[2][2] * b2 + a[3][2] * b3;
  result[3] = a[0][3] * b0 + a[1][3] * b1 + a[2][3] * b2 + a[3][3] * b3;
}

// static
void Matrix4::mul_(const float a[][4], const float b[][4], float result[][4])
{
  Matrix4 temp (uninitialized);

  const column_type* pa = a;

  if (pa == &result[0])
  {
    std::memcpy(temp.m_, pa, sizeof(temp.m_));
    pa = temp.m_;
  }

  for (int i = 0; i < 4; ++i)
  {
    // Again, this works only if result[] and b[] are either entirely
    // distinct locations in memory or alternatively exactly congruent.

    const float bi0 = b[i][0];
    const float bi1 = b[i][1];
    const float bi2 = b[i][2];
    const float bi3 = b[i][3];

    result[i][0] = pa[0][0] * bi0 + pa[1][0] * bi1 + pa[2][0] * bi2 + pa[3][0] * bi3;
    result[i][1] = pa[0][1] * bi0 + pa[1][1] * bi1 + pa[2][1] * bi2 + pa[3][1] * bi3;
    result[i][2] = pa[0][2] * bi0 + pa[1][2] * bi1 + pa[2][2] * bi2 + pa[3][2] * bi3;
    result[i][3] = pa[0][3] * bi0 + pa[1][3] * bi1 + pa[2][3] * bi2 + pa[3][3] * bi3;
  }
}

// static
void Quat::from_axis_(const float* a, float phi, float* result)
{
  Vector4::div_(a, vector3_mag(a), result);
  Vector4::mul_(result, std::sin(phi / 2.0f), result);

  result[3] = std::cos(phi / 2.0f);
}

Quat::value_type Quat::angle() const
{
  return 2.0f * std::atan2(vector3_mag(v_), v_[3]);
}

// static
void Quat::to_matrix_(const float* quat, float result[][4])
{
  const float qx = quat[0];
  const float qy = quat[1];
  const float qz = quat[2];
  const float qw = quat[3];

  result[0][0] = 1.0f - 2.0f * (qy * qy + qz * qz);
  result[0][1] =        2.0f * (qx * qy + qz * qw);
  result[0][2] =        2.0f * (qz * qx - qy * qw);
  result[0][3] = 0.0f;

  result[1][0] =        2.0f * (qx * qy - qz * qw);
  result[1][1] = 1.0f - 2.0f * (qz * qz + qx * qx);
  result[1][2] =        2.0f * (qy * qz + qx * qw);
  result[1][3] = 0.0f;

  result[2][0] =        2.0f * (qz * qx + qy * qw);
  result[2][1] =        2.0f * (qy * qz - qx * qw);
  result[2][2] = 1.0f - 2.0f * (qy * qy + qx * qx);
  result[2][3] = 0.0f;

  result[3][0] = 0.0f;
  result[3][1] = 0.0f;
  result[3][2] = 0.0f;
  result[3][3] = 1.0f;
}

// static
void Quat::mul_(const float* a, const float* b, float* result)
{
  const float qx = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  const float qy = a[3] * b[1] + a[1] * b[3] + a[2] * b[0] - a[0] * b[2];
  const float qz = a[3] * b[2] + a[2] * b[3] + a[0] * b[1] - a[1] * b[0];
  const float qw = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];

  result[0] = qx;
  result[1] = qy;
  result[2] = qz;
  result[3] = qw;
}

void Quat::renormalize(Quat::value_type epsilon)
{
  const float norm = Vector4::dot_(v_, v_);

  if (std::abs(1.0f - norm) > epsilon)
  {
    // Renormalize quat to fix accumulated error
    Vector4::div_(v_, std::sqrt(norm), v_);
  }
}

} // namespace Math
