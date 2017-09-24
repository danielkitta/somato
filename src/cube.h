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

namespace Somato
{

class Cube
{
public:
  struct SortPredicate;

  enum : int { N = 3 };
  enum : int { AXIS_X, AXIS_Y, AXIS_Z };

  constexpr Cube() : data_ {0} {}
  constexpr Cube(std::initializer_list<bool> bits)
    : data_ {init_bits(0, bits.begin(), bits.end())} {}

  void clear() { data_ = 0; }
  bool empty() const { return (data_ == 0); }

  bool get(int x, int y, int z) const { return get_(data_, x, y, z); }
  void put(int x, int y, int z, bool value) { data_ = put_(data_, x, y, z, value); }

  Cube& rotate(int axis) // clockwise
    { data_ = rotate_(data_, axis); return *this; }

  Cube& shift(int axis, bool clip = false) // rightward
    { data_ = shift_(data_, axis, clip); return *this; }

  Cube& shift_rev(int axis, bool clip = false) // leftward
    { data_ = shift_rev_(data_, axis, clip); return *this; }

  Cube& operator&=(Cube other) { data_ &= other.data_; return *this; }
  Cube& operator|=(Cube other) { data_ |= other.data_; return *this; }
  Cube operator~() const { return Cube(data_ ^ ~(~Bits{1} << (N*N*N - 1))); }

  friend Cube operator&(Cube a, Cube b) { return Cube(a.data_ & b.data_); }
  friend Cube operator|(Cube a, Cube b) { return Cube(a.data_ | b.data_); }

  friend bool operator==(Cube a, Cube b) { return (a.data_ == b.data_); }
  friend bool operator!=(Cube a, Cube b) { return (a.data_ != b.data_); }

private:
  typedef unsigned int Bits;

  explicit Cube(Bits data) : data_ {data} {}

  static constexpr Bits init_bits(Bits data,
                                  std::initializer_list<bool>::const_iterator start,
                                  std::initializer_list<bool>::const_iterator pos)
  {
    return (pos == start) ? data : init_bits((data << 1) | pos[-1], start, pos - 1);
  }
  static bool get_(Bits data, int x, int y, int z);
  static Bits put_(Bits data, int x, int y, int z, bool value);

  static Bits rotate_(Bits data, int axis);
  static Bits shift_(Bits data, int axis, bool clip);
  static Bits shift_rev_(Bits data, int axis, bool clip);

  Bits data_;
};

struct Cube::SortPredicate
{
  bool operator()(Cube a, Cube b) const { return (a.data_ < b.data_); }
};

} // namespace Somato

#endif // !SOMATO_CUBE_H_INCLUDED
