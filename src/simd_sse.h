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

#ifndef SOMATO_SIMD_SSE_H_INCLUDED
#define SOMATO_SIMD_SSE_H_INCLUDED

#include <cmath>
#include <xmmintrin.h>

namespace Simd
{

typedef __m128 V4f;

inline V4f add4(V4f a, V4f b)
{
  return _mm_add_ps(a, b);
}

inline V4f sub4(V4f a, V4f b)
{
  return _mm_sub_ps(a, b);
}

inline V4f mul4(V4f a, V4f b)
{
  return _mm_mul_ps(a, b);
}

inline V4f mul4s(V4f a, float b)
{
  return _mm_mul_ps(a, _mm_set1_ps(b));
}

inline V4f div4(V4f a, V4f b)
{
  return _mm_div_ps(a, b);
}

inline V4f div4s(V4f a, float b)
{
  return _mm_div_ps(a, _mm_set1_ps(b));
}

inline V4f neg4(V4f v)
{
  return _mm_xor_ps(_mm_set1_ps(-0.f), v);
}

inline V4f cross3(V4f a, V4f b)
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

inline V4f dot4r(V4f a, V4f b)
{
  const __m128 c = _mm_mul_ps(a, b);
  const __m128 d = _mm_add_ps(c, _mm_shuffle_ps(c, c, _MM_SHUFFLE(2,3,0,1)));

  return _mm_add_ps(d, _mm_shuffle_ps(d, d, _MM_SHUFFLE(1,0,3,2)));
}

inline float dot4s(V4f a, V4f b)
{
  return _mm_cvtss_f32(dot4r(a, b));
}

inline V4f mag4r(V4f v)
{
  return _mm_sqrt_ps(dot4r(v, v));
}

inline float mag4s(V4f v)
{
  return _mm_cvtss_f32(_mm_sqrt_ss(dot4r(v, v)));
}

inline float mag3s(V4f v)
{
  __m128 c = _mm_mul_ps(v, v);
  __m128 d = _mm_shuffle_ps(c, c, _MM_SHUFFLE(2,3,0,1));

  d = _mm_add_ss(d, c);
  c = _mm_add_ss(_mm_movehl_ps(c, c), d);

  return _mm_cvtss_f32(_mm_sqrt_ss(c));
}

V4f norm4(V4f v);

inline bool cmp4eq(V4f a, V4f b)
{
  return (_mm_movemask_ps(_mm_cmpneq_ps(a, b)) == 0);
}

template <int I> inline float ext4s(V4f v)
{
  return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(I,I,I,I)));
}

template <> inline float ext4s<0>(V4f v)
{
  return _mm_cvtss_f32(v);
}

template <> inline float ext4s<2>(V4f v)
{
  return _mm_cvtss_f32(_mm_movehl_ps(v, v));
}

inline float& ref4s(V4f& v, std::size_t i)
{
  return reinterpret_cast<float*>(&v)[i];
}

inline const float& ref4s(const V4f& v, std::size_t i)
{
  return reinterpret_cast<const float*>(&v)[i];
}

void mat4_transpose(const V4f* m, V4f* result);
V4f  mat4_mul_mv(const V4f* a, V4f b);
V4f  mat4_mul_vm(V4f a, const V4f* b);
void mat4_mul_mm(const V4f* a, const V4f* b, V4f* result);

inline V4f quat_axis_mask()
{
  const float m = -std::nanf("0x3FFFFF");
  return _mm_setr_ps(m, m, m, 0.f);
}

inline V4f quat_axis(V4f quat)
{
  return _mm_and_ps(quat, quat_axis_mask());
}

V4f  quat_from_axis(const V4f& a, float phi);
void quat_to_matrix(V4f quat, V4f* result);
V4f  quat_mul(V4f a, V4f b);

} // namespace Simd

#endif // !SOMATO_SIMD_SSE_H_INCLUDED
