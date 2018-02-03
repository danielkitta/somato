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

#ifndef SOMATO_BITCUBE_H_INCLUDED
#define SOMATO_BITCUBE_H_INCLUDED

#include <initializer_list>
#include <cstddef>
#include <cstdint>

namespace Somato
{

template <int, int> class PuzzleCube;

enum class ClipMode : int { CULL = 0, SLICE = -1 };
enum Axis : int { AXIS_X, AXIS_Y, AXIS_Z };

template <int N> struct BitCubeTraits {};
template <> struct BitCubeTraits<3> { using BitsType = uint32_t; };
template <> struct BitCubeTraits<4> { using BitsType = uint64_t; };

template <int N> using CubeBits = typename BitCubeTraits<N>::BitsType;

template <int N_>
class BitCube
{
public:
  enum : int { N = N_ };
  class  Index;
  struct SortPredicate;

  constexpr BitCube() : data_ {0} {}
  constexpr BitCube(std::initializer_list<Index> cells)
    : data_ {init_cells(0, begin(cells), end(cells))} {}

  void clear() { data_ = 0; }
  bool empty() const { return (data_ == 0); }
  explicit operator bool() const { return (data_ != 0); }

  void put(int x, int y, int z, bool value)
  {
    const int index = N*N*x + N*y + z;
    data_ = (data_ & ~(Bits{1} << index)) | (Bits{value} << index);
  }
  void put(Index i, bool value)
    { data_ = (data_ & ~(Bits{1} << i)) | (Bits{value} << i); }

  bool get(int x, int y, int z) const
    { return ((data_ >> (N*N*x + N*y + z)) & Bits{1}); }
  bool get(Index i) const
    { return ((data_ >> i) & Bits{1}); }

  BitCube& rotate_x() // counterclockwise
    { data_ = rotate_x_(data_); return *this; }
  BitCube& rotate_y() // counterclockwise
    { data_ = rotate_y_(data_); return *this; }
  BitCube& rotate_z() // counterclockwise
    { data_ = rotate_z_(data_); return *this; }

  BitCube& shift(int axis, ClipMode clip = ClipMode::CULL) // rightward
    { data_ = shift_(data_, axis, clip); return *this; }

  BitCube& shift_rev(int axis, ClipMode clip = ClipMode::CULL) // leftward
    { data_ = shift_rev_(data_, axis, clip); return *this; }

  BitCube& operator&=(BitCube other) { data_ &= other.data_; return *this; }
  BitCube& operator|=(BitCube other) { data_ |= other.data_; return *this; }
  BitCube& operator^=(BitCube other) { data_ ^= other.data_; return *this; }
  BitCube operator~() const { return BitCube{data_ ^ ~(~Bits{1} << (N*N*N - 1))}; }

  friend BitCube operator&(BitCube a, BitCube b) { return BitCube{a.data_ & b.data_}; }
  friend BitCube operator|(BitCube a, BitCube b) { return BitCube{a.data_ | b.data_}; }
  friend BitCube operator^(BitCube a, BitCube b) { return BitCube{a.data_ ^ b.data_}; }

  friend bool operator==(BitCube a, BitCube b) { return (a.data_ == b.data_); }
  friend bool operator!=(BitCube a, BitCube b) { return (a.data_ != b.data_); }

private:
  template <int, int> friend class PuzzleCube;
  typedef CubeBits<N> Bits;

  explicit BitCube(Bits data) : data_ {data} {}

  static constexpr Bits init_cells(Bits data,
                                   typename std::initializer_list<Index>::const_iterator pos,
                                   typename std::initializer_list<Index>::const_iterator pend)
  {
    return (pos == pend) ? data : init_cells(data | (Bits{1} << *pos), pos + 1, pend);
  }
  static Bits rotate_x_(Bits data);
  static Bits rotate_y_(Bits data);
  static Bits rotate_z_(Bits data);
  static Bits shift_(Bits data, std::size_t axis, ClipMode clip);
  static Bits shift_rev_(Bits data, std::size_t axis, ClipMode clip);

  Bits data_;
};

template <int N_>
class BitCube<N_>::Index
{
private:
  unsigned char idx_;

public:
  constexpr Index() : idx_ {0} {}
  constexpr Index(int x, int y, int z) : idx_ (N_*N_*x + N_*y + z) {}
  constexpr operator unsigned int() const { return idx_; }
};

template <int N_>
struct BitCube<N_>::SortPredicate
{
  bool operator()(BitCube<N_> a, BitCube<N_> b) const { return (a.data_ < b.data_); }
};

extern template class BitCube<3>;

using SomaBitCube = BitCube<3>;

} // namespace Somato

#endif // !SOMATO_BITCUBE_H_INCLUDED
