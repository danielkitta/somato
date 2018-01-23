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

#include <config.h>
#include "simd_fallback.h"

#include <cmath>
#include <cstring>

namespace
{

using Simd::V4f;

inline void set_v4f(V4f& result, float x, float y, float z, float w)
{
  result[0] = x;
  result[1] = y;
  result[2] = z;
  result[3] = w;
}

} // anonymous namespace

V4f Simd::cross3(const V4f& a, const V4f& b)
{
  return {a[1] * b[2] - a[2] * b[1],
          a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0],
          0.f};
}

float Simd::mag4s(const V4f& v)
{
  return std::sqrt(dot4s(v, v));
}

V4f Simd::norm4(const V4f& v)
{
  const float d = std::sqrt(dot4s(v, v));
  return mul4s(v, 1.f / d);
}

void Simd::mat4_transpose(const V4f* m, V4f* result)
{
  const float m01 = m[0][1];
  const float m02 = m[0][2];
  const float m03 = m[0][3];
  set_v4f(result[0], m[0][0], m[1][0], m[2][0], m[3][0]);

  const float m12 = m[1][2];
  const float m13 = m[1][3];
  set_v4f(result[1], m01, m[1][1], m[2][1], m[3][1]);

  const float m23 = m[2][3];
  set_v4f(result[2], m02, m12, m[2][2], m[3][2]);

  set_v4f(result[3], m03, m13, m23, m[3][3]);
}

V4f Simd::mat4_mul_mv(const V4f* a, const V4f& b)
{
  const float b0 = b[0];
  const float b1 = b[1];
  const float b2 = b[2];
  const float b3 = b[3];

  return {(a[0][0] * b0 + a[1][0] * b1) + (a[2][0] * b2 + a[3][0] * b3),
          (a[0][1] * b0 + a[1][1] * b1) + (a[2][1] * b2 + a[3][1] * b3),
          (a[0][2] * b0 + a[1][2] * b1) + (a[2][2] * b2 + a[3][2] * b3),
          (a[0][3] * b0 + a[1][3] * b1) + (a[2][3] * b2 + a[3][3] * b3)};
}

V4f Simd::mat4_mul_vm(const V4f& a, const V4f* b)
{
  return {dot4s(a, b[0]), dot4s(a, b[1]), dot4s(a, b[2]), dot4s(a, b[3])};
}

/* Either input may alias the result, except that partial overlap is not
 * allowed. In the rare case of input b aliasing result, an internal copy
 * will be made. This should only ever be needed when a matrix is squared
 * in place.
 */
void Simd::mat4_mul_mm(const V4f* a, const V4f* b, V4f* result)
{
  V4f temp[4];

  if (b == result)
  {
    std::memcpy(temp, b, sizeof temp);
    b = temp;
  }

  for (int i = 0; i < 4; ++i)
  {
    const float a0i = a[0][i];
    const float a1i = a[1][i];
    const float a2i = a[2][i];
    const float a3i = a[3][i];

    result[0][i] = (a0i * b[0][0] + a1i * b[0][1]) + (a2i * b[0][2] + a3i * b[0][3]);
    result[1][i] = (a0i * b[1][0] + a1i * b[1][1]) + (a2i * b[1][2] + a3i * b[1][3]);
    result[2][i] = (a0i * b[2][0] + a1i * b[2][1]) + (a2i * b[2][2] + a3i * b[2][3]);
    result[3][i] = (a0i * b[3][0] + a1i * b[3][1]) + (a2i * b[3][2] + a3i * b[3][3]);
  }
}

V4f Simd::quat_from_vectors(const V4f& a, const V4f& b)
{
  V4f q = {a[0] * b[0] + a[1] * b[1] + a[2] * b[2],
           a[1] * b[2] - a[2] * b[1],
           a[2] * b[0] - a[0] * b[2],
           a[0] * b[1] - a[1] * b[0]};
  q[0] += mag4s(q);

  return q;
}

float Simd::quat_angle(const V4f& q)
{
  const float s = std::sqrt(q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  return 2.f * std::atan2(s, q[0]);
}

V4f Simd::quat_from_axis(const V4f& a, float phi)
{
  const float s = std::sin(phi * 0.5f);
  const float c = std::cos(phi * 0.5f);

  return {c, a[0] * s, a[1] * s, a[2] * s};
}

void Simd::quat_to_matrix(const V4f& quat, V4f* result)
{
  const float r = quat[0];
  const float x = quat[1];
  const float y = quat[2];
  const float z = quat[3];

  result[0][0] = (r*r + x*x) - (y*y + z*z);
  result[0][1] = 2.f * (x*y + r*z);
  result[0][2] = 2.f * (x*z - r*y);
  result[0][3] = 0.f;

  result[1][0] = 2.f * (x*y - r*z);
  result[1][1] = (r*r + y*y) - (x*x + z*z);
  result[1][2] = 2.f * (y*z + r*x);
  result[1][3] = 0.f;

  result[2][0] = 2.f * (x*z + r*y);
  result[2][1] = 2.f * (y*z - r*x);
  result[2][2] = (r*r + z*z) - (x*x + y*y);
  result[2][3] = 0.f;

  result[3][0] = 0.f;
  result[3][1] = 0.f;
  result[3][2] = 0.f;
  result[3][3] = 1.f;
}

V4f Simd::quat_mul(const V4f& a, const V4f& b)
{
  return {(a[0] * b[0] - a[1] * b[1]) - (a[2] * b[2] + a[3] * b[3]),
          (a[0] * b[1] + a[1] * b[0]) + (a[2] * b[3] - a[3] * b[2]),
          (a[0] * b[2] + a[2] * b[0]) + (a[3] * b[1] - a[1] * b[3]),
          (a[0] * b[3] + a[3] * b[0]) + (a[1] * b[2] - a[2] * b[1])};
}

V4f Simd::quat_inv(const V4f& q)
{
  const float d = dot4s(q, q);
  return {q[0] / d, -q[1] / d, -q[2] / d, -q[3] / d};
}
