/*
 * Copyright (c) 2018  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_PUZZLECUBE_H_INCLUDED
#define SOMATO_PUZZLECUBE_H_INCLUDED

#include "bitcube.h"

#include <cstddef>
#include <array>
#include <iterator>
#include <utility>

namespace Somato
{

constexpr int ilog2p1_(unsigned int a, int i = 0)
{
  return (a == 0) ? i : ilog2p1_(a >> 1, i + 1);
}

/* Compact container for an arrangement of puzzle pieces within a cube.
 */
template <int N, int C>
class PuzzleCube
{
public:
  class iterator;

  typedef std::size_t size_type;
  typedef BitCube<N>  value_type;
  typedef iterator    const_iterator;

  enum { COUNT = C, DEPTH = ilog2p1_(C) };

  static constexpr size_type npos = size_type{0} - 1;

  constexpr PuzzleCube() : planes_ {0,} {}

  // Construct from an array of one-hot puzzle piece bit masks.
  template <typename A> PuzzleCube(const A& pieces)
    : PuzzleCube{pieces, std::make_index_sequence<DEPTH>{}} {}

  // Extract the cell mask of a single puzzle piece.
  value_type operator[](size_type i) const { return begin()[i]; }

  // Extract the index of the puzzle piece occupying a cell, or npos for none.
  size_type piece_at_cell(typename value_type::Index c) const
    { return combine_cell_bits(c, std::make_index_sequence<DEPTH>{}); }

  constexpr size_type size() const { return C; }
  constexpr bool empty() const { return (C == 0); }

  const_iterator begin()  const { return iterator{planes_, 0}; }
  const_iterator end()    const { return iterator{planes_, C}; }
  const_iterator cbegin() const { return iterator{planes_, 0}; }
  const_iterator cend()   const { return iterator{planes_, C}; }

private:
  template <typename A, std::size_t... I>
  explicit PuzzleCube(const A& pieces, std::index_sequence<I...>)
    : planes_ {gather_bitplane<1u << I>(pieces, std::make_index_sequence<C>{})...}
  {}
  template <std::size_t M, typename A, std::size_t... I>
  static CubeBits<N> gather_bitplane(const A& pieces, std::index_sequence<I...>)
  {
    CubeBits<N> r = 0;
    // Fold all puzzle piece masks whose indices match the bit plane index.
    const CubeBits<N> dummy[] = {((M & (I + 1)) ? r |= pieces[I].data_ : r)...};
    static_cast<void>(dummy);
    return r;
  }
  template <std::size_t... I>
  size_type combine_cell_bits(std::size_t s, std::index_sequence<I...>) const
  {
    size_type p = 0;
    // Combine bit planes matching a cell index to a piece index.
    const size_type dummy[] = {(p = 2*p + ((planes_[DEPTH-1-I] >> s) & 1))...};
    static_cast<void>(dummy);
    return p - 1;
  }
  // Store the piece index at each cell in an array of bit planes.
  CubeBits<N> planes_[DEPTH];
};

/* Iterator over puzzle pieces within a cube.
 */
template <int N, int C>
class PuzzleCube<N, C>::iterator
{
public:
  typedef std::random_access_iterator_tag iterator_category;
  typedef BitCube<N>                      value_type;
  typedef std::ptrdiff_t                  difference_type;
  typedef void                            pointer;
  typedef BitCube<N>                      reference;

  constexpr iterator() : planes_ {nullptr}, index_ {0} {}

  value_type operator[](difference_type d) const
    { return extract_piece_mask(index_ + d + 1, std::make_index_sequence<DEPTH>{}); }

  value_type operator*() const { return (*this)[0]; }

  iterator& operator++()    { ++index_; return *this; }
  iterator  operator++(int) { return iterator{planes_, index_++}; }
  iterator& operator--()    { --index_; return *this; }
  iterator  operator--(int) { return iterator{planes_, index_--}; }

  iterator& operator+=(difference_type d) { index_ += d; return *this; }
  iterator& operator-=(difference_type d) { index_ -= d; return *this; }

  friend iterator operator+(iterator a, difference_type d) { return iterator{a.planes_, a.index_ + d}; }
  friend iterator operator+(difference_type d, iterator a) { return iterator{a.planes_, a.index_ + d}; }
  friend iterator operator-(iterator a, difference_type d) { return iterator{a.planes_, a.index_ - d}; }
  friend difference_type operator-(iterator a, iterator b) { return a.index_ - b.index_; }

  friend bool operator==(iterator a, iterator b) { return (a.index_ == b.index_); }
  friend bool operator!=(iterator a, iterator b) { return (a.index_ != b.index_); }
  friend bool operator< (iterator a, iterator b) { return (a.index_ <  b.index_); }
  friend bool operator> (iterator a, iterator b) { return (a.index_ >  b.index_); }
  friend bool operator<=(iterator a, iterator b) { return (a.index_ <= b.index_); }
  friend bool operator>=(iterator a, iterator b) { return (a.index_ >= b.index_); }

private:
  friend class PuzzleCube<N, C>;

  explicit iterator(const CubeBits<N>* p, difference_type i) : planes_ {p}, index_ {i} {}

  template <std::size_t... I>
  value_type extract_piece_mask(CubeBits<N> b, std::index_sequence<I...>) const
  {
    value_type r = ~value_type{};
    // Combine bit planes to isolate the bit mask of a single puzzle piece.
    const BitCube<N> dummy[] = {(r &= value_type{(((b >> I) & 1) - 1) ^ planes_[I]})...};
    static_cast<void>(dummy);
    return r;
  }

  const CubeBits<N>* planes_;
  difference_type    index_;
};

} // namespace Somato

#endif // !SOMATO_PUZZLECUBE_H_INCLUDED
