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

#include <config.h>
#include "cube.h"

namespace
{

using namespace Somato;

/* Generate a right-aligned mask of consecutive 1-bits.
 */
template <int N>
constexpr CubeBits<N> one_mask(int count)
{
  return (count > 0) ? ~(~CubeBits<N>{1} << (count - 1)) : CubeBits<N>{0};
}

/* Generate a sparse bit mask of count 1-bits at stride intervals.
 */
template <int N>
constexpr CubeBits<N> repeat_mask(CubeBits<N> pattern, int count, int stride)
{
  return one_mask<N>(count * stride) / one_mask<N>(stride) * pattern;
}

/* Derive stride from axis index (x: N*N, y: N, z: 1).
 */
template <int N> inline int axis_stride(int axis);

template <> inline int axis_stride<3>(int axis) { return ((2-axis) << (2-axis)) + 1; }
template <> inline int axis_stride<4>(int axis) { return 16u >> (2 * axis); }

/* Rotate cube cells by 90 degrees counterclockwise.
 */
template <int N>
inline CubeBits<N> cube_rotate_x(CubeBits<N> data)
{
  const CubeBits<N> mask = repeat_mask<N>(1, N, N*N);
  CubeBits<N> r = 0;

  for (int y = 0; y < N; ++y)
    for (int z = N-1; z >= 0; --z)
      r = (r << 1) | ((data >> (N*z + y)) & mask);

  return r;
}

template <int N>
inline CubeBits<N> cube_rotate_y(CubeBits<N> data)
{
  const CubeBits<N> mask = repeat_mask<N>(1, N, N);
  CubeBits<N> r = 0;

  for (int x = 0; x < N; ++x)
  {
    r <<= N*(N-1);

    for (int z = N-1; z >= 0; --z)
      r = (r << 1) | ((data >> (N*N*z + x)) & mask);
  }
  return r;
}

template <int N>
inline CubeBits<N> cube_rotate_z(CubeBits<N> data)
{
  const CubeBits<N> mask = one_mask<N>(N);
  CubeBits<N> r = 0;

  for (int x = N-1; x >= 0; --x)
    for (int y = 0; y < N; ++y)
        r = (r << N) | ((data >> (N*N*y + N*x)) & mask);

  return r;
}

/* Optimized specializations for a 3x3x3 cube.
 */
template <>
inline CubeBits<3> cube_rotate_x<3>(CubeBits<3> data)
{
  return (data & 0020020020)
      | ((data & 0102102102) << 2)
      | ((data & 0204204204) >> 2)
      | ((data & 0010010010) << 4)
      | ((data & 0040040040) >> 4)
      | ((data & 0001001001) << 6)
      | ((data & 0400400400) >> 6);
}

template <>
inline CubeBits<3> cube_rotate_y<3>(CubeBits<3> data)
{
  return (data & 0000222000)
      | ((data & 0111000000) << 2)
      | ((data & 0000000444) >> 2)
      | ((data & 0000000222) << 8)
      | ((data & 0222000000) >> 8)
      | ((data & 0000111000) << 10)
      | ((data & 0000444000) >> 10)
      | ((data & 0000000111) << 18)
      | ((data & 0444000000) >> 18);
}

template <>
inline CubeBits<3> cube_rotate_z<3>(CubeBits<3> data)
{
  return (data & 0000070000)
      | ((data & 0000700007) << 6)
      | ((data & 0700007000) >> 6)
      | ((data & 0000000070) << 12)
      | ((data & 0070000000) >> 12)
      | ((data & 0000000700) << 18)
      | ((data & 0007000000) >> 18);
}

/* Optimized specializations for a 4x4x4 cube.
 */
template <>
inline CubeBits<4> cube_rotate_x<4>(CubeBits<4> data)
{
  return ((data & 0x0200020002000200) << 1)
       | ((data & 0x0040004000400040) >> 1)
       | ((data & 0x0004000400040004) << 2)
       | ((data & 0x2000200020002000) >> 2)
       | ((data & 0x1000100010001000) << 3)
       | ((data & 0x0008000800080008) >> 3)
       | ((data & 0x0020002000200020) << 4)
       | ((data & 0x0400040004000400) >> 4)
       | ((data & 0x0100010001000100) << 6)
       | ((data & 0x0080008000800080) >> 6)
       | ((data & 0x0002000200020002) << 7)
       | ((data & 0x4000400040004000) >> 7)
       | ((data & 0x0010001000100010) << 9)
       | ((data & 0x0800080008000800) >> 9)
       | ((data & 0x0001000100010001) << 12)
       | ((data & 0x8000800080008000) >> 12);
}

template <>
inline CubeBits<4> cube_rotate_y<4>(CubeBits<4> data)
{
  return ((data & 0x0000222200000000) << 1)
       | ((data & 0x0000000044440000) >> 1)
       | ((data & 0x1111000000000000) << 3)
       | ((data & 0x0000000000008888) >> 3)
       | ((data & 0x0000000000004444) << 14)
       | ((data & 0x2222000000000000) >> 14)
       | ((data & 0x0000000022220000) << 16)
       | ((data & 0x0000444400000000) >> 16)
       | ((data & 0x0000111100000000) << 18)
       | ((data & 0x0000000088880000) >> 18)
       | ((data & 0x0000000000002222) << 31)
       | ((data & 0x4444000000000000) >> 31)
       | ((data & 0x0000000011110000) << 33)
       | ((data & 0x0000888800000000) >> 33)
       | ((data & 0x0000000000001111) << 48)
       | ((data & 0x8888000000000000) >> 48);
}

template <>
inline CubeBits<4> cube_rotate_z<4>(CubeBits<4> data)
{
  return ((data & 0x0000000000F00000) << 4)
       | ((data & 0x00000F0000000000) >> 4)
       | ((data & 0x0000F00000000000) << 8)
       | ((data & 0x00000000000F0000) >> 8)
       | ((data & 0x000000000000000F) << 12)
       | ((data & 0xF000000000000000) >> 12)
       | ((data & 0x000000000F000000) << 16)
       | ((data & 0x000000F000000000) >> 16)
       | ((data & 0x00000000000000F0) << 24)
       | ((data & 0x0F00000000000000) >> 24)
       | ((data & 0x00000000F0000000) << 28)
       | ((data & 0x0000000F00000000) >> 28)
       | ((data & 0x0000000000000F00) << 36)
       | ((data & 0x00F0000000000000) >> 36)
       | ((data & 0x000000000000F000) << 48)
       | ((data & 0x000F000000000000) >> 48);
}

} // anonymous namespace

namespace Somato
{

template <int N_>
typename Cube<N_>::Bits Cube<N_>::rotate_x_(Bits data)
{
  return cube_rotate_x<N_>(data);
}

template <int N_>
typename Cube<N_>::Bits Cube<N_>::rotate_y_(Bits data)
{
  return cube_rotate_y<N_>(data);
}

template <int N_>
typename Cube<N_>::Bits Cube<N_>::rotate_z_(Bits data)
{
  return cube_rotate_z<N_>(data);
}

template <int N_>
typename Cube<N_>::Bits Cube<N_>::shift_(Bits data, int axis, ClipMode clip)
{
  static const Bits shift_mask[3] =
  {
    one_mask<N>((N-1)*N*N),
    repeat_mask<N>(one_mask<N>((N-1)*N), N, N*N),
    repeat_mask<N>(one_mask<N>((N-1)),   N*N, N)
  };
  const Bits mask = shift_mask[axis];
  const Bits clip_mask = (data & ~mask) ? mask & static_cast<Bits>(clip) : mask;

  return (data & clip_mask) << axis_stride<N>(axis);
}

template <int N_>
typename Cube<N_>::Bits Cube<N_>::shift_rev_(Bits data, int axis, ClipMode clip)
{
  static const Bits shift_mask[3] =
  {
    one_mask<N>((N-1)*N*N) << N*N,
    repeat_mask<N>(one_mask<N>((N-1)*N) << N, N, N*N),
    repeat_mask<N>(one_mask<N>((N-1))   << 1, N*N, N)
  };
  const Bits mask = shift_mask[axis];
  const Bits clip_mask = (data & ~mask) ? mask & static_cast<Bits>(clip) : mask;

  return (data & clip_mask) >> axis_stride<N>(axis);
}

template class Cube<3>;

} // namespace Somato
