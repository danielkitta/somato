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

template <int N> inline int shift_by_axis(int axis);

template <> inline int shift_by_axis<3>(int axis) { return ((2-axis) << (2-axis)) + 1; }
template <> inline int shift_by_axis<4>(int axis) { return 16u >> (2 * axis); }

template <int N>
inline CubeBits<N> cube_rotate_x(CubeBits<N> data)
{
  CubeBits<N> r = 0;

  for (int x = N-1; x >= 0; --x)
    for (int y = N-1; y >= 0; --y)
      for (int z = N-1; z >= 0; --z)
        r = (r << 1) | ((data >> (N*N*x + N*z + (N-1-y))) & CubeBits<N>{1});

  return r;
}

template <int N>
inline CubeBits<N> cube_rotate_y(CubeBits<N> data)
{
  CubeBits<N> r = 0;

  for (int x = N-1; x >= 0; --x)
    for (int y = N-1; y >= 0; --y)
      for (int z = N-1; z >= 0; --z)
        r = (r << 1) | ((data >> (N*N*z + N*y + (N-1-x))) & CubeBits<N>{1});

  return r;
}

template <int N>
inline CubeBits<N> cube_rotate_z(CubeBits<N> data)
{
  CubeBits<N> r = 0;

  for (int x = N-1; x >= 0; --x)
    for (int y = N-1; y >= 0; --y)
      for (int z = N-1; z >= 0; --z)
        r = (r << 1) | ((data >> (N*N*(N-1-y) + N*x + z)) & CubeBits<N>{1});

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
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N*N*N - 1)) * ~(~Bits{0} << (N-1)*N*N),
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N*N   - 1)) * ~(~Bits{0} << (N-1)*N),
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N     - 1)) * ~(~Bits{0} << (N-1))
  };
  const Bits mask = shift_mask[axis];
  const Bits clip_mask = (data & ~mask) ? mask & static_cast<Bits>(clip) : mask;

  return (data & clip_mask) << shift_by_axis<N>(axis);
}

template <int N_>
typename Cube<N_>::Bits Cube<N_>::shift_rev_(Bits data, int axis, ClipMode clip)
{
  static const Bits shift_mask[3] =
  {
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N*N*N - 1)) * (~(~Bits{0} << (N-1)*N*N) << (N*N)),
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N*N   - 1)) * (~(~Bits{0} << (N-1)*N)   << N),
    ~(~Bits{1} << (N*N*N - 1)) / ~(~Bits{1} << (N     - 1)) * (~(~Bits{0} << (N-1))     << 1)
  };
  const Bits mask = shift_mask[axis];
  const Bits clip_mask = (data & ~mask) ? mask & static_cast<Bits>(clip) : mask;

  return (data & clip_mask) >> shift_by_axis<N>(axis);
}

template class Cube<3>;

} // namespace Somato
