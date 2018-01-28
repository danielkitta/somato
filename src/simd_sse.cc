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
#include <cstddef>

namespace
{

using Simd::V4f;

/* C++17 automatically invokes the aligned new operator for objects with
 * extended alignment requirements, provided the implementation supports
 * the extended alignment. Otherwise, insist that the standard alignment
 * is sufficient for the SIMD vector type.
 */
#ifndef __cpp_aligned_new
static_assert(alignof(V4f) <= alignof(std::max_align_t),
              "Memory allocation alignment not sufficient");
#endif

/* Half precision reciprocal square root approximation followed by
 * one Newton-Raphson iteration.
 */
inline V4f rsqrt4(V4f a)
{
  const __m128 x0    = _mm_rsqrt_ps(a);
  const __m128 axx   = _mm_mul_ps(_mm_mul_ps(a, x0), x0);
  const __m128 x0mh  = _mm_mul_ps(x0, _mm_set1_ps(-0.5f));
  const __m128 m3axx = _mm_add_ps(axx, _mm_set1_ps(-3.f));

  return _mm_mul_ps(x0mh, m3axx);
}

} // anonymous namespace

V4f Simd::norm4(V4f v)
{
  const __m128 d = dot4r(v, v);
  return _mm_mul_ps(v, rsqrt4(d));
}

void Simd::mat4_transpose(const V4f* m, V4f* result)
{
  const __m128 c0 = m[0]; // 00, 10, 20, 30
  const __m128 c1 = m[1]; // 01, 11, 21, 31
  const __m128 c2 = m[2]; // 02, 12, 22, 32
  const __m128 c3 = m[3]; // 03, 13, 23, 33

  const __m128 t0 = _mm_unpacklo_ps(c0, c1); // 00, 01, 10, 11
  const __m128 t1 = _mm_unpacklo_ps(c2, c3); // 02, 03, 12, 13
  const __m128 t2 = _mm_unpackhi_ps(c0, c1); // 20, 21, 30, 31
  const __m128 t3 = _mm_unpackhi_ps(c2, c3); // 22, 23, 32, 33

  result[0] = _mm_movelh_ps(t0, t1); // 00, 01, 02, 03
  result[1] = _mm_movehl_ps(t1, t0); // 10, 11, 12, 13
  result[2] = _mm_movelh_ps(t2, t3); // 20, 21, 22, 23
  result[3] = _mm_movehl_ps(t3, t2); // 30, 31, 32, 33
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
  const __m128 r0 = _mm_mul_ps(b[0], a);      // 00, 01, 02, 03
  const __m128 r1 = _mm_mul_ps(b[1], a);      // 10, 11, 12, 13
  const __m128 r2 = _mm_mul_ps(b[2], a);      // 20, 21, 22, 23
  const __m128 r3 = _mm_mul_ps(b[3], a);      // 30, 31, 32, 33

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

/* Input a may alias the result without restrictions. Input b may also alias
 * the result, except that partial overlap is not allowed.
 */
void Simd::mat4_mul_mm(const V4f* a, const V4f* b, V4f* result)
{
  const __m128 a0 = a[0];
  const __m128 a1 = a[1];
  const __m128 a2 = a[2];
  const __m128 a3 = a[3];

  for (int i = 0; i < 4; ++i)
  {
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

/* The rotation between two vectors is simply the quotient of two imaginary
 * quaternions:
 * q = |q| * exp(I(q) * phi) = (0, b) * (0, a)^-1
 * where phi is the angle between a, b and I(q) is the imaginary axis. To
 * obtain a quaternion suitable as a conjugation map, the angle phi needs
 * to be halved:
 * p = |p| * exp(I(q) * phi / 2) = q ^ (1/2)
 *
 * Disregarding any non-unit norms, this can be simplified to:
 * q' = s * q = (0, b) * (0, -a) = (a * b, a X b)
 * p' = t * p = (q'r + |q'|, q'v)
 * where q'r and q'v are the real and vector parts of q'. The scalar factors
 * s, t can be ignored since any normalization applied later on will cancel
 * them out.
 */
V4f Simd::quat_from_wedge(V4f a, V4f b)
{
  const __m128 a_yzxw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,0,2,1));
  const __m128 b_yzxw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3,0,2,1));
  const __m128 c_zxy0 = _mm_sub_ps(_mm_mul_ps(a, b_yzxw), _mm_mul_ps(a_yzxw, b));
  const __m128 c_0xyz = _mm_shuffle_ps(c_zxy0, c_zxy0, _MM_SHUFFLE(0,2,1,3));

  const __m128 ab = _mm_mul_ps(a, b);
  const __m128 d2 = _mm_add_ps(_mm_unpacklo_ps(ab, ab), ab); // [2] y+z
  const __m128 d0 = _mm_add_ps(_mm_movehl_ps(d2, d2), ab);   // [0] x+(y+z)

  const __m128 q = _mm_move_ss(c_0xyz, d0);  // (a * b, a X b)
  const __m128 n = _mm_sqrt_ss(dot4r(q, q)); // |q'|

  return _mm_add_ss(q, n); // (q'r + |q'|, q'v)
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
 *
 * It is assumed that 2*(r*r) is finite.
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
