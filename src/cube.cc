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

#include <array>
#include <utility>

namespace
{

using namespace Somato;

template <int N> inline int shift_by_axis(int axis);

template <> inline int shift_by_axis<3>(int axis) { return ((2-axis) << (2-axis)) + 1; }
template <> inline int shift_by_axis<4>(int axis) { return 16u >> (2 * axis); }

template <int A> constexpr
unsigned char make_rotation_index(int n, int x, int y, int z);

template <> constexpr
unsigned char make_rotation_index<AXIS_X>(int n, int x, int y, int z)
{
  return n*n*(n-1-x) + n*z + (n-1-y);
}

template <> constexpr
unsigned char make_rotation_index<AXIS_Y>(int n, int x, int y, int z)
{
  return n*n*z + n*(n-1-y) + (n-1-x);
}

template <> constexpr
unsigned char make_rotation_index<AXIS_Z>(int n, int x, int y, int z)
{
  return n*n*y + n*(n-1-x) + (n-1-z);
}

template <int N, int A, int... I> constexpr
std::array<unsigned char, N*N*N> make_rotation_indices_(std::integer_sequence<int, I...>)
{
  return {make_rotation_index<A>(N, I / (N*N), (I % (N*N)) / N, I % N)...};
}

template <int N, int A> constexpr
std::array<unsigned char, N*N*N> make_rotation_indices()
{
  return make_rotation_indices_<N, A>(std::make_integer_sequence<int, N*N*N>{});
}

} // anonymous namespace

namespace Somato
{

template <int N_>
typename Cube<N_>::Bits Cube<N_>::rotate_(Bits data, int axis)
{
  static const std::array<unsigned char, N*N*N> shuffle_order[3] =
  {
    make_rotation_indices<N, AXIS_X>(),
    make_rotation_indices<N, AXIS_Y>(),
    make_rotation_indices<N, AXIS_Z>()
  };
  Bits result = 0;

  for (int i = 0; i < N*N*N; ++i)
    result = (result << 1) | ((data >> shuffle_order[axis][i]) & Bits{1});

  return result;
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
