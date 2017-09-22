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
 * along with Somato; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include "cube.h"

namespace
{

using Somato::Cube;

static
const unsigned char shift_count[3] = { Cube::N * Cube::N, Cube::N, 1 };

} // anonymous namespace

namespace Somato
{

// static
Cube::Bits Cube::from_array(const bool data[N][N][N])
{
  const bool *const source = &data[0][0][0];
  Bits result = 0;

  for (int i = N*N*N - 1; i >= 0; --i)
    result = (result << 1) | Bits(source[i]);

  return result;
}

bool Cube::get(int x, int y, int z) const
{
  const int index = N*N*x + N*y + z;

  return ((data_ >> index) & Bits(1));
}

bool Cube::getsafe(int x, int y, int z) const
{
  const int index = N*N*x + N*y + z;
  // This code is not optimal, but on balance probably better than three
  // consecutive branches within the first few instructions of a function.
  return (x >= 0 && x < N) & (y >= 0 && y < N) & (z >= 0 && z < N) & (data_ >> index);
}

void Cube::put(int x, int y, int z, bool value)
{
  const int index = N*N*x + N*y + z;

  data_ = (data_ & ~(Bits(1) << index)) | (Bits(value) << index);
}

Cube& Cube::rotate(int axis)
{
  // This table is specific to the N = 3 case and requires
  // modification for other values of N.
  static_assert (N == 3, "shuffle_order only valid for N = 3");

  static const unsigned char shuffle_order[3][8 * sizeof(Bits)] =
  {
    {
      20, 23, 26, 19, 22, 25, 18, 21, 24, 11, 14, 17, 10, 13, 16,  9,
      12, 15,  2,  5,  8,  1,  4,  7,  0,  3,  6,  0,  0,  0,  0,  0
    },
    {
       8, 17, 26,  5, 14, 23,  2, 11, 20,  7, 16, 25,  4, 13, 22,  1,
      10, 19,  6, 15, 24,  3, 12, 21,  0,  9, 18,  0,  0,  0,  0,  0
    },
    {
       8,  7,  6, 17, 16, 15, 26, 25, 24,  5,  4,  3, 14, 13, 12, 23,
      22, 21,  2,  1,  0, 11, 10,  9, 20, 19, 18,  0,  0,  0,  0,  0
    }
  };

  Bits result = 0;

  for (int i = 0; i < N*N*N; ++i)
    result = (result << 1) | ((data_ >> shuffle_order[axis][i]) & Bits(1));

  data_ = result;
  return *this;
}

Cube& Cube::shift(int axis, bool clip)
{
  static const Bits shift_mask[3] =
  {
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N*N*N - 1)) * ~(~Bits(0) << (N-1)*N*N),
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N*N   - 1)) * ~(~Bits(0) << (N-1)*N),
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N     - 1)) * ~(~Bits(0) << (N-1))
  };

  const Bits source = shift_mask[axis] & data_;

  if (clip || source == data_)
    data_ = source << shift_count[axis];
  else
    data_ = 0;

  return *this;
}

Cube& Cube::shift_rev(int axis, bool clip)
{
  static const Bits shift_mask[3] =
  {
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N*N*N - 1)) * (~(~Bits(0) << (N-1)*N*N) << (N*N)),
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N*N   - 1)) * (~(~Bits(0) << (N-1)*N) << N),
    ~(~Bits(1) << (N*N*N - 1)) / ~(~Bits(1) << (N     - 1)) * (~(~Bits(0) << (N-1)) << 1)
  };

  const Bits source = shift_mask[axis] & data_;

  if (clip || source == data_)
    data_ = source >> shift_count[axis];
  else
    data_ = 0;

  return *this;
}

} // namespace Somato
