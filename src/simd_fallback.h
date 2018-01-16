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

#ifndef SOMATO_SIMD_FALLBACK_H_INCLUDED
#define SOMATO_SIMD_FALLBACK_H_INCLUDED

#include <array>

namespace Simd
{

typedef std::array<float, 4> V4f;

inline V4f add4(const V4f& a, const V4f& b)
{
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]};
}

inline V4f sub4(const V4f& a, const V4f& b)
{
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]};
}

inline V4f mul4s(const V4f& a, float b)
{
  return {a[0] * b, a[1] * b, a[2] * b, a[3] * b};
}

inline V4f div4s(const V4f& a, float b)
{
  return {a[0] / b, a[1] / b, a[2] / b, a[3] / b};
}

inline V4f neg4(const V4f& v)
{
  return {-v[0], -v[1], -v[2], -v[3]};
}

V4f cross3(const V4f& a, const V4f& b);

inline float dot4s(const V4f& a, const V4f& b)
{
  return (a[0] * b[0] + a[2] * b[2]) + (a[1] * b[1] + a[3] * b[3]);
}

float mag4s(const V4f& v);
float mag3s(const V4f& v);
V4f   norm4(const V4f& v);

inline bool cmp4eq(const V4f& a, const V4f& b)
{
  return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

template <int I> inline float ext4s(const V4f& v)
{
  return v[I];
}

inline float& ref4s(V4f& v, std::size_t i)
{
  return v[i];
}

inline const float& ref4s(const V4f& v, std::size_t i)
{
  return v[i];
}

void mat4_transpose(const V4f* m, V4f* result);
V4f  mat4_mul_mv(const V4f* a, const V4f& b);
V4f  mat4_mul_vm(const V4f& a, const V4f* b);
void mat4_mul_mm(const V4f* a, const V4f* b, V4f* result);

inline V4f quat_axis(const V4f& quat)
{
  return {quat[0], quat[1], quat[2], 0.f};
}

V4f  quat_from_axis(const V4f& a, float phi);
void quat_to_matrix(const V4f& quat, V4f* result);
V4f  quat_mul(const V4f& a, const V4f& b);

} // namespace Simd

#endif // !SOMATO_SIMD_FALLBACK_H_INCLUDED
