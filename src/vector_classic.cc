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

#if !SOMATO_VECTOR_USE_SSE

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace
{

inline void set_vector(float* result, float x, float y, float z, float w)
{
  result[0] = x;
  result[1] = y;
  result[2] = z;
  result[3] = w;
}

inline float vector3_mag(const float* v)
{
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

} // anonymous namespace

namespace Math
{

const std::array<Vector4, 4> Vector4::basis =
{{
  {1.f, 0.f, 0.f, 0.f},
  {0.f, 1.f, 0.f, 0.f},
  {0.f, 0.f, 1.f, 0.f},
  {0.f, 0.f, 0.f, 1.f},
}};

void Vector4::sign_(const float* v, float* result)
{
  set_vector(result, int{v[0] > 0.f} - int{v[0] < 0.f},
                     int{v[1] > 0.f} - int{v[1] < 0.f},
                     int{v[2] > 0.f} - int{v[2] < 0.f},
                     int{v[3] > 0.f} - int{v[3] < 0.f});
}

void Vector4::rint_(const float* v, float* result)
{
  // Boldly assume the default rounding style.  This way, shooting yourself
  // into the foot by changing the rounding direction non-temporarily becomes
  // an even more delighting experience than usual.
  set_vector(result, std::lrint(v[0]), std::lrint(v[1]),
                     std::lrint(v[2]), std::lrint(v[3]));
}

void Vector4::mask_ifzero_(const float* a, const float* b, float* result)
{
  set_vector(result, (b[0] == 0.f) ? 0.f : a[0],
                     (b[1] == 0.f) ? 0.f : a[1],
                     (b[2] == 0.f) ? 0.f : a[2],
                     (b[3] == 0.f) ? 0.f : a[3]);
}

void Vector4::mask_ifnonzero_(const float* a, const float* b, float* result)
{
  set_vector(result, (b[0] != 0.f) ? 0.f : a[0],
                     (b[1] != 0.f) ? 0.f : a[1],
                     (b[2] != 0.f) ? 0.f : a[2],
                     (b[3] != 0.f) ? 0.f : a[3]);
}

void Vector4::add_(const float* a, const float* b, float* result)
{
  set_vector(result, a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]);
}

void Vector4::sub_(const float* a, const float* b, float* result)
{
  set_vector(result, a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]);
}

void Vector4::mul_(const float* a, float b, float* result)
{
  set_vector(result, a[0] * b, a[1] * b, a[2] * b, a[3] * b);
}

void Vector4::div_(const float* a, float b, float* result)
{
  set_vector(result, a[0] / b, a[1] / b, a[2] / b, a[3] / b);
}

float Vector4::dot_(const float* a, const float* b)
{
  return (a[0] * b[0] + a[1] * b[1]) + (a[2] * b[2] + a[3] * b[3]);
}

void Vector4::cross_(const float* a, const float* b, float* result)
{
  set_vector(result, a[1] * b[2] - a[2] * b[1],
                     a[2] * b[0] - a[0] * b[2],
                     a[0] * b[1] - a[1] * b[0],
                     0.f);
}

bool Vector4::equal_(const float* a, const float* b)
{
  return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

void Vector4::norm_(const float* v, float* result)
{
  const float s = 1.f / mag(v);
  set_vector(result, v[0] * s, v[1] * s, v[2] * s, v[3] * s);
}

Vector4::value_type Vector4::mag(const value_type* v)
{
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
}

Matrix4::Matrix4()
:
  m_ {{1.f, 0.f, 0.f, 0.f},
      {0.f, 1.f, 0.f, 0.f},
      {0.f, 0.f, 1.f, 0.f},
      {0.f, 0.f, 0.f, 1.f}}
{}

void Matrix4::scale_(const float a[][4], float s, float result[][4])
{
  set_vector(result[0], a[0][0] * s, a[0][1] * s, a[0][2] * s, a[0][3] * s);
  set_vector(result[1], a[1][0] * s, a[1][1] * s, a[1][2] * s, a[1][3] * s);
  set_vector(result[2], a[2][0] * s, a[2][1] * s, a[2][2] * s, a[2][3] * s);
  set_vector(result[3], a[3][0],     a[3][1],     a[3][2],     a[3][3]);
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

Matrix4& Matrix4::operator=(const value_type b[][4])
{
  set_vector(m_[0], b[0][0], b[0][1], b[0][2], b[0][3]);
  set_vector(m_[1], b[1][0], b[1][1], b[1][2], b[1][3]);
  set_vector(m_[2], b[2][0], b[2][1], b[2][2], b[2][3]);
  set_vector(m_[3], b[3][0], b[3][1], b[3][2], b[3][3]);

  return *this;
}

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

void Matrix4::mul_(const float a[][4], const float b[][4], float result[][4])
{
  Matrix4 temp (uninitialized);

  using column_type = value_type[4];
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

void Quat::from_axis_(const float* a, float phi, float* result)
{
  const float s = std::sin(phi * 0.5f);
  const float c = std::cos(phi * 0.5f);

  set_vector(result, a[0] * s, a[1] * s, a[2] * s, c);
}

Quat::value_type Quat::angle() const
{
  return 2.f * std::atan2(vector3_mag(v_), v_[3]);
}

void Quat::to_matrix_(const float* quat, float result[][4])
{
  const float qx = quat[0];
  const float qy = quat[1];
  const float qz = quat[2];
  const float qw = quat[3];

  result[0][0] = 1.f - 2.f * (qy * qy + qz * qz);
  result[0][1] =       2.f * (qx * qy + qz * qw);
  result[0][2] =       2.f * (qz * qx - qy * qw);
  result[0][3] = 0.f;

  result[1][0] =       2.f * (qx * qy - qz * qw);
  result[1][1] = 1.f - 2.f * (qz * qz + qx * qx);
  result[1][2] =       2.f * (qy * qz + qx * qw);
  result[1][3] = 0.f;

  result[2][0] =       2.f * (qz * qx + qy * qw);
  result[2][1] =       2.f * (qy * qz - qx * qw);
  result[2][2] = 1.f - 2.f * (qy * qy + qx * qx);
  result[2][3] = 0.f;

  result[3][0] = 0.f;
  result[3][1] = 0.f;
  result[3][2] = 0.f;
  result[3][3] = 1.f;
}

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

} // namespace Math

#endif // !SOMATO_VECTOR_USE_SSE
