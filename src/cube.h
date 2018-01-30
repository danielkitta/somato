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

#ifndef SOMATO_CUBE_H_INCLUDED
#define SOMATO_CUBE_H_INCLUDED

#include <initializer_list>
#include <cstdint>

namespace Somato
{

enum class ClipMode : int { CULL = 0, SLICE = -1 };
enum Axis : int { AXIS_X, AXIS_Y, AXIS_Z };

template <int N> struct CubeTraits {};
template <> struct CubeTraits<3> { using BitsType = uint32_t; };
template <> struct CubeTraits<4> { using BitsType = uint64_t; };

template <int N> using CubeBits = typename CubeTraits<N>::BitsType;

template <int N_>
class Cube
{
public:
  enum : int { N = N_ };
  struct SortPredicate;

  constexpr Cube() : data_ {0} {}
  constexpr Cube(std::initializer_list<bool> bits)
    : data_ {init_bits(0, begin(bits), end(bits))} {}

  void clear() { data_ = 0; }
  bool empty() const { return (data_ == 0); }
  explicit operator bool() const { return (data_ != 0); }

  void put(int x, int y, int z, bool value)
  {
    const int index = N*N*x + N*y + z;
    data_ = (data_ & ~(Bits{1} << index)) | (Bits{value} << index);
  }
  bool get(int x, int y, int z) const
    { return ((data_ >> (N*N*x + N*y + z)) & Bits{1}); }

  Cube& rotate(int axis) // counterclockwise
    { data_ = rotate_(data_, axis); return *this; }

  Cube& shift(int axis, ClipMode clip = ClipMode::CULL) // rightward
    { data_ = shift_(data_, axis, clip); return *this; }

  Cube& shift_rev(int axis, ClipMode clip = ClipMode::CULL) // leftward
    { data_ = shift_rev_(data_, axis, clip); return *this; }

  Cube& operator&=(Cube other) { data_ &= other.data_; return *this; }
  Cube& operator|=(Cube other) { data_ |= other.data_; return *this; }
  Cube operator~() const { return Cube(data_ ^ ~(~Bits{1} << (N*N*N - 1))); }

  friend Cube operator&(Cube a, Cube b) { return Cube(a.data_ & b.data_); }
  friend Cube operator|(Cube a, Cube b) { return Cube(a.data_ | b.data_); }

  friend bool operator==(Cube a, Cube b) { return (a.data_ == b.data_); }
  friend bool operator!=(Cube a, Cube b) { return (a.data_ != b.data_); }

private:
  typedef CubeBits<N> Bits;

  explicit Cube(Bits data) : data_ {data} {}

  static constexpr Bits init_bits(Bits data,
                                  std::initializer_list<bool>::const_iterator start,
                                  std::initializer_list<bool>::const_iterator pos)
  {
    return (pos == start) ? data : init_bits((data << 1) | pos[-1], start, pos - 1);
  }
  static Bits rotate_(Bits data, int axis);
  static Bits shift_(Bits data, int axis, ClipMode clip);
  static Bits shift_rev_(Bits data, int axis, ClipMode clip);

  Bits data_;
};

template <int N_>
struct Cube<N_>::SortPredicate
{
  bool operator()(Cube<N_> a, Cube<N_> b) const { return (a.data_ < b.data_); }
};

extern template class Cube<3>;

using SomaCube = Cube<3>;

} // namespace Somato

#endif // !SOMATO_CUBE_H_INCLUDED
