/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_CUBE_H_INCLUDED
#define SOMATO_CUBE_H_INCLUDED

namespace Somato
{

class Cube
{
public:
  class SortPredicate;

  enum { N = 3 };
  enum { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };

  inline Cube();
  explicit inline Cube(const bool data[N][N][N]);

  inline void clear();
  inline bool empty() const;

  bool get(int x, int y, int z) const;
  bool getsafe(int x, int y, int z) const;
  void put(int x, int y, int z, bool value);

  Cube& rotate(int axis);                   // clockwise rotation
  Cube& shift(int axis, bool clip = false); // rightward shifting
  Cube& shift_rev(int axis, bool clip = false); // leftward shifting
  
  inline Cube& operator&=(Cube other);
  inline Cube& operator|=(Cube other);
  inline Cube operator~() const;

  friend inline Cube operator&(Cube a, Cube b);
  friend inline Cube operator|(Cube a, Cube b);

  friend inline bool operator==(Cube a, Cube b);
  friend inline bool operator!=(Cube a, Cube b);

private:
  typedef unsigned int Bits;

  Bits data_;

  explicit inline Cube(Bits data);
  static Bits from_array(const bool data[N][N][N]);
};

class Cube::SortPredicate
{
public:
  typedef Cube first_argument_type;
  typedef Cube second_argument_type;
  typedef bool result_type;

  inline bool operator()(Cube a, Cube b) const;
};

inline
Cube::Cube(Cube::Bits data)
:
  data_ (data)
{}

inline
Cube::Cube()
:
  data_ (0)
{}

inline
Cube::Cube(const bool data[Cube::N][Cube::N][Cube::N])
:
  data_ (Cube::from_array(data))
{}

inline
void Cube::clear()
{
  data_ = 0;
}

inline
bool Cube::empty() const
{
  return (data_ == 0);
}

inline
Cube& Cube::operator&=(Cube other)
{
  data_ &= other.data_;
  return *this;
}

inline
Cube& Cube::operator|=(Cube other)
{
  data_ |= other.data_;
  return *this;
}

inline
Cube Cube::operator~() const
{
  return Cube(data_ ^ ~(~Bits(1) << (N*N*N - 1)));
}

inline
Cube operator&(Cube a, Cube b)
{
  return Cube(a.data_ & b.data_);
}

inline
Cube operator|(Cube a, Cube b)
{
  return Cube(a.data_ | b.data_);
}

inline
bool operator==(Cube a, Cube b)
{
  return (a.data_ == b.data_);
}

inline
bool operator!=(Cube a, Cube b)
{
  return (a.data_ != b.data_);
}

inline
bool Cube::SortPredicate::operator()(Cube a, Cube b) const
{
  return (a.data_ < b.data_);
}

} // namespace Somato

#endif /* SOMATO_CUBE_H_INCLUDED */
