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
#include "simd_sse.h"

#include <cmath>
#include <cstdlib>
#include <new>

#if (ALIGNOF_MAX_ALIGN_T < 16)
# define SOMATO_CUSTOM_ALLOC 1
#endif

namespace
{

using Simd::V4f;

#if SOMATO_CUSTOM_ALLOC

void* v4_align_alloc(std::size_t size) noexcept G_GNUC_MALLOC;

# if SOMATO_HAVE_POSIX_MEMALIGN

void* v4_align_alloc(std::size_t size) noexcept
{
  if (size == 0)
    return std::malloc(1);

  // According to the language rules, using the sizeof operator on an
  // aggregate type always returns a multiple of the largest alignment
  // requirement of any aggregate member.  In other words, if the size
  // of an allocation request is not a multiple of 16, it cannot have
  // an alignment requirement of 16 bytes.  Checking for this catches
  // the majority of cases where the special alignment is unnecessary.

  if (size % sizeof(__m128) != 0)
    return std::malloc(size);

  void* p = 0;

  if (posix_memalign(&p, sizeof(__m128), size) == 0)
    return p;

  return 0;
}

inline void v4_align_free(void* p) noexcept
{
  std::free(p);
}

# else /* !SOMATO_HAVE_POSIX_MEMALIGN */

void* v4_align_alloc(std::size_t size) noexcept
{
  if (size == 0)
    size = 1;

  const std::size_t alignment = (size % sizeof(__m128) == 0) ? sizeof(__m128) : sizeof(__m64);

  return _mm_malloc(size, alignment);
}

inline void v4_align_free(void* p) noexcept
{
  _mm_free(p);
}

# endif /* !SOMATO_HAVE_POSIX_MEMALIGN */
#endif /* SOMATO_CUSTOM_ALLOC */

} // anonymous namespace

/* Replace the global new and delete operators to ensure that dynamically
 * allocated memory meets the SSE alignment requirements. This is of course
 * a somewhat heavy-handed approach, but if not done globally, every class
 * or struct that has a data member of type Vector4/Matrix4/Quat would have
 * to provide its own memory allocation operators. Worse yet, this applies
 * even to indirectly contained data members, and inheritance as a possible
 * solution does not apply there!
 *
 * Obviously this is all too easy to get wrong, thus I opted for the global
 * solution. This implies that the current SSE vector implementation should
 * not be included in a library.
 *
 * Note that the GNU C library already uses a minimum alignment of 16 bytes
 * on 64-bit systems, thus the explicit alignment is actually not necessary
 * on these systems.
 */
#if SOMATO_CUSTOM_ALLOC

void* operator new(std::size_t size)
{
  if (void *const p = v4_align_alloc(size))
    return p;
  else
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
  if (void *const p = v4_align_alloc(size))
    return p;
  else
    throw std::bad_alloc();
}

void operator delete(void* p) throw()
{
  v4_align_free(p);
}

void operator delete[](void* p) throw()
{
  v4_align_free(p);
}

void* operator new(std::size_t size, const std::nothrow_t&) throw()
{
  return v4_align_alloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) throw()
{
  return v4_align_alloc(size);
}

void operator delete(void* p, const std::nothrow_t&) throw()
{
  v4_align_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) throw()
{
  v4_align_free(p);
}

#endif /* SOMATO_CUSTOM_ALLOC */

V4f Simd::norm4(V4f v)
{
  const __m128 d = dot4r(v, v);
  return _mm_div_ps(v, _mm_sqrt_ps(d));
}

void Simd::mat4_transpose(const V4f* m, V4f* result)
{
  const __m128 c0 = m[0];
  const __m128 c1 = m[1];
  const __m128 c2 = m[2];
  const __m128 c3 = m[3];

  const __m128 t0 = _mm_unpacklo_ps(c0, c1);
  const __m128 t1 = _mm_unpacklo_ps(c2, c3);
  const __m128 t2 = _mm_unpackhi_ps(c0, c1);
  const __m128 t3 = _mm_unpackhi_ps(c2, c3);

  result[0] = _mm_movelh_ps(t0, t1);
  result[1] = _mm_movehl_ps(t1, t0);
  result[2] = _mm_movelh_ps(t2, t3);
  result[3] = _mm_movehl_ps(t3, t2);
}

V4f Simd::mat4_mul_mv(const V4f* a, V4f b)
{
  const __m128 c0 = _mm_mul_ps(_mm_shuffle_ps(b, b, _MM_SHUFFLE(0,0,0,0)), a[0]);
  const __m128 c1 = _mm_mul_ps(_mm_shuffle_ps(b, b, _MM_SHUFFLE(1,1,1,1)), a[1]);
  const __m128 c2 = _mm_mul_ps(_mm_shuffle_ps(b, b, _MM_SHUFFLE(2,2,2,2)), a[2]);
  const __m128 c3 = _mm_mul_ps(_mm_shuffle_ps(b, b, _MM_SHUFFLE(3,3,3,3)), a[3]);

  const __m128 s0 = _mm_add_ps(c0, c1);
  const __m128 s2 = _mm_add_ps(c2, c3);

  return _mm_add_ps(s0, s2);
}

V4f Simd::mat4_mul_vm(V4f a, const V4f* b)
{
  const __m128 r0 = _mm_mul_ps(b[0], a);
  const __m128 r1 = _mm_mul_ps(b[1], a);
  const __m128 r2 = _mm_mul_ps(b[2], a);
  const __m128 r3 = _mm_mul_ps(b[3], a);

  const __m128 t0 = _mm_unpacklo_ps(r0, r1);  // 00, 10, 01, 11
  const __m128 t1 = _mm_unpackhi_ps(r0, r1);  // 02, 12, 03, 13
  const __m128 t2 = _mm_unpacklo_ps(r2, r3);  // 20, 30, 21, 31
  const __m128 t3 = _mm_unpackhi_ps(r2, r3);  // 22, 32, 23, 33

  const __m128 s0 = _mm_add_ps(t0, t1);       // 00+02, 10+12, 01+03, 11+13
  const __m128 s2 = _mm_add_ps(t2, t3);       // 20+22, 30+32, 21+23, 31+33

  const __m128 c0 = _mm_movelh_ps(s0, s2);    // 00+02, 10+12, 20+22, 30+32
  const __m128 c2 = _mm_movehl_ps(s2, s0);    // 01+03, 11+13, 21+23, 31+33

  return _mm_add_ps(c0, c2);
}

void Simd::mat4_mul_mm(const V4f* a, const V4f* b, V4f* result)
{
  const __m128 a0 = a[0];
  const __m128 a1 = a[1];
  const __m128 a2 = a[2];
  const __m128 a3 = a[3];

  for (int i = 0; i < 4; ++i)
  {
    // It is assumed that b[] and result[] either refer to the same location
    // in memory or are completely distinct, i.e. not partially overlapping.
    const __m128 bi = b[i];

    const __m128 c0 = _mm_mul_ps(_mm_shuffle_ps(bi, bi, _MM_SHUFFLE(0,0,0,0)), a0);
    const __m128 c1 = _mm_mul_ps(_mm_shuffle_ps(bi, bi, _MM_SHUFFLE(1,1,1,1)), a1);
    const __m128 c2 = _mm_mul_ps(_mm_shuffle_ps(bi, bi, _MM_SHUFFLE(2,2,2,2)), a2);
    const __m128 c3 = _mm_mul_ps(_mm_shuffle_ps(bi, bi, _MM_SHUFFLE(3,3,3,3)), a3);

    const __m128 s0 = _mm_add_ps(c0, c1);
    const __m128 s2 = _mm_add_ps(c2, c3);

    result[i] = _mm_add_ps(s0, s2);
  }
}

float Simd::quat_angle(V4f quat)
{
  const __m128 d = _mm_mul_ps(quat, quat);
  const __m128 e = _mm_add_ps(_mm_unpackhi_ps(_mm_setzero_ps(), d), d);
  const __m128 f = _mm_add_ps(_mm_movehl_ps(d, d), e);
  const __m128 s = _mm_sqrt_ss(_mm_shuffle_ps(f, f, _MM_SHUFFLE(1,1,1,1)));

  return 2.f * std::atan2(_mm_cvtss_f32(s), _mm_cvtss_f32(quat));
}

V4f Simd::quat_from_vectors(V4f a, V4f b)
{
  const __m128 a_yzxw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,0,2,1));
  const __m128 b_yzxw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3,0,2,1));
  const __m128 c_zxy0 = _mm_sub_ps(_mm_mul_ps(a, b_yzxw), _mm_mul_ps(a_yzxw, b));
  const __m128 c_0xyz = _mm_shuffle_ps(c_zxy0, c_zxy0, _MM_SHUFFLE(0,2,1,3));

  // q = (dot(a, b), cross(a, b))
  const __m128 q = _mm_move_ss(c_0xyz, dot4r(a, b));
  const __m128 n = _mm_sqrt_ss(dot4r(q, q));

  // Average with scaled identity to half the rotation angle.
  return _mm_add_ss(q, n); // (q.r + mag(q), q.xyz)
}

V4f Simd::quat_from_axis(const V4f& a, float phi)
{
  const float phi_2  = 0.5f * phi;
  const float sine   = std::sin(phi_2);
  const float cosine = std::cos(phi_2);

  const __m128 v = _mm_mul_ps(_mm_set1_ps(sine), a);
  const __m128 r = _mm_set_ss(cosine);

  return _mm_move_ss(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2,1,0,3)), r);
}

/* Homogeneous expression for the rotation matrix from a quaternion:
 * [ rr + xx - yy - zz | 2 * (xy - rz)     | 2 * (xz + ry)     ]
 * [ 2 * (xy + rz)     | rr - xx + yy - zz | 2 * (yz - rx)     ]
 * [ 2 * (xz - ry)     | 2 * (yz + rx)     | rr - xx - yy + zz ]
 *
 * Matrix elements rearranged to facilitate vectorization:
 * [oo] (rr + rr) - (rr + rr) | [oo] 2*rr - 2*rr | [nn] 2*rr + 2*rr
 * [a0] (rr + xx) - (yy + zz) | [a1] 2*xy - 2*rz | [b0] 2*xy + 2*rz
 * [b1] (rr + yy) - (zz + xx) | [b2] 2*yz - 2*rx | [c1] 2*yz + 2*rx
 * [c2] (rr + zz) - (xx + yy) | [c0] 2*zx - 2*ry | [a2] 2*zx + 2*ry
 */
void Simd::quat_to_matrix(V4f quat, V4f* result)
{
  const __m128 rxyz_t2 = _mm_add_ps(quat, quat);
  const __m128 rxyz_p2 = _mm_mul_ps(quat, quat);

  const __m128 ryzx    = _mm_shuffle_ps(quat, quat, _MM_SHUFFLE(1,3,2,0));
  const __m128 rzxy    = _mm_shuffle_ps(quat, quat, _MM_SHUFFLE(2,1,3,0));
  const __m128 rrrr_t2 = _mm_shuffle_ps(rxyz_t2, rxyz_t2, _MM_SHUFFLE(0,0,0,0));

  const __m128 r2r_x2y_y2z_z2x = _mm_mul_ps(rxyz_t2, ryzx);
  const __m128 r2r_r2z_r2x_r2y = _mm_mul_ps(rrrr_t2, rzxy);

  const __m128 rrrr_p2 = _mm_shuffle_ps(rxyz_p2, rxyz_p2, _MM_SHUFFLE(0,0,0,0));
  const __m128 ryzx_p2 = _mm_shuffle_ps(rxyz_p2, rxyz_p2, _MM_SHUFFLE(1,3,2,0));
  const __m128 rzxy_p2 = _mm_shuffle_ps(rxyz_p2, rxyz_p2, _MM_SHUFFLE(2,1,3,0));

  const __m128 rrrr2_rxyz2 = _mm_add_ps(rrrr_p2, rxyz_p2);
  const __m128 ryzx2_rzxy2 = _mm_add_ps(ryzx_p2, rzxy_p2);
  const __m128 oo_a1_b2_c0 = _mm_sub_ps(r2r_x2y_y2z_z2x, r2r_r2z_r2x_r2y);
  const __m128 nn_b0_c1_a2 = _mm_add_ps(r2r_x2y_y2z_z2x, r2r_r2z_r2x_r2y);
  const __m128 oo_a0_b1_c2 = _mm_sub_ps(rrrr2_rxyz2, ryzx2_rzxy2);

  result[3] = _mm_setr_ps(0.f, 0.f, 0.f, 1.f);

  const __m128 b1_c2_oo_a1 = _mm_shuffle_ps(oo_a0_b1_c2, oo_a1_b2_c0, _MM_SHUFFLE(1,0,3,2));
  const __m128 b0_c1_oo_a0 = _mm_shuffle_ps(nn_b0_c1_a2, oo_a0_b1_c2, _MM_SHUFFLE(1,0,2,1));
  const __m128 b2_c0_c1_a2 = _mm_movehl_ps(nn_b0_c1_a2, oo_a1_b2_c0);

  result[1] = _mm_shuffle_ps(b1_c2_oo_a1, b0_c1_oo_a0, _MM_SHUFFLE(2,1,0,3));
  result[0] = _mm_shuffle_ps(b0_c1_oo_a0, oo_a1_b2_c0, _MM_SHUFFLE(0,3,0,3));
  result[2] = _mm_shuffle_ps(b2_c0_c1_a2, oo_a0_b1_c2, _MM_SHUFFLE(0,3,0,3));
}

/* Quaternion multiplication:
 * r = ar * br - ax * bx - ay * by - az * bz
 * x = ar * bx + ax * br + ay * bz - az * by
 * y = ar * by + ay * br + az * bx - ax * bz
 * z = ar * bz + az * br + ax * by - ay * bx
 */
V4f Simd::quat_mul(V4f a, V4f b)
{
  const __m128 a0 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0,0,0,0));
  const __m128 a1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,2,1,1));
  const __m128 a2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1,3,2,2));
  const __m128 a3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2,1,3,3));

  const __m128 b1 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(0,0,0,1));
  const __m128 b2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2,1,3,2));
  const __m128 b3 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(1,3,2,3));

  const __m128 c0 = _mm_mul_ps(a0, b);
  const __m128 c1 = _mm_mul_ps(a1, b1);
  const __m128 c2 = _mm_mul_ps(a2, b2);
  const __m128 c3 = _mm_mul_ps(a3, b3);

  // Partially negate intermediate sum in order to compute r
  // along with x, y, z using only vertical operations:
  // r = ar * br + (-(ax * bx + ay * by)) - az * bz
  const __m128 rneg = _mm_set_ss(-0.f);

  const __m128 c12 = _mm_add_ps(c1, c2);
  const __m128 c03 = _mm_sub_ps(c0, c3);

  return _mm_add_ps(_mm_xor_ps(c12, rneg), c03);
}

V4f Simd::quat_inv(V4f quat)
{
  const __m128 d = dot4r(quat, quat);
  const __m128 c = quat_conj(quat);

  return _mm_div_ps(c, d);
}
