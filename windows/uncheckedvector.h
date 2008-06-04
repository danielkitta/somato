/*
 * Copyright (c) 2008  Daniel Elstner  <daniel.kitta@gmail.com>
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

#pragma once

#include "array.h"
#include <memory>

namespace Util
{

/*
 * Microsoft has decided to cripple the utility of the C++ STL for raw
 * computing applications and added fairly involved (and practically mandatory)
 * runtime checks to all STL containers. Any code that makes extensive use of
 * iterators in a manner similar to how plain pointers would be used, with the
 * reasonable assumption of virtually identical performance, will experience
 * significant penalties. The optimizer doesn't appear to have special
 * knowledge of the constructs involved and will happily perform a check with
 * every single dereference, sequential or not. And even where the timing
 * isn't important, the countless redundant checks still add to code size
 * bloat.
 *
 * STL containers like std::vector<T> are not supposed to behave in this
 * manner outside of special debugging setups. As the Man himself once said,
 * you could always implement a slower but safer interface on top of a fast
 * one -- but not the other way around.
 *
 * For this reason, on top of the incompatibility with any element type which
 * imposes an additional alignment restriction, I decided to bite the bullet
 * and reimplement a vector-like STL container. The new substitute container
 * is now available through "uncheckedvector.h" as class UncheckedVector<T>.
 * The allocation semantics and average complexity match std::vector<T>, but
 * the interface is restricted to the small subset of functionality which is
 * actually being used throughout the Somato code base.
 */
template <class T>
class UncheckedVector
{
public:
  typedef typename Util::MemChunk<T>::value_type              value_type;
  typedef typename Util::MemChunk<T>::reference               reference;
  typedef typename Util::MemChunk<T>::const_reference         const_reference;
  typedef typename Util::MemChunk<T>::iterator                iterator;
  typedef typename Util::MemChunk<T>::const_iterator          const_iterator;
  typedef typename Util::MemChunk<T>::reverse_iterator        reverse_iterator;
  typedef typename Util::MemChunk<T>::const_reverse_iterator  const_reverse_iterator;
  typedef typename Util::MemChunk<T>::size_type               size_type;
  typedef typename Util::MemChunk<T>::difference_type         difference_type;

private:
  Util::MemChunk<T> storage_;
  iterator          endused_;

  static const size_type chunksize = 32;
  static const size_type chunkmask = 0x1F;

  static inline iterator destroy_backward_(iterator pbegin, iterator pend);
  void expand_(size_type c);

public:
  inline void swap(UncheckedVector<T>& b);

  UncheckedVector() : storage_ (), endused_ (0) {}
  explicit inline UncheckedVector(size_type s, const T& value = T());
  inline UncheckedVector(const UncheckedVector<T>& b);
  inline UncheckedVector<T>& operator=(const UncheckedVector<T>& b);
  inline ~UncheckedVector();

  size_type capacity() const { return storage_.size(); }
  size_type size()     const { return endused_ - storage_.begin(); }
  bool      empty()    const { return (storage_.begin() == endused_); }

  inline void reserve(size_type c);
  inline void resize(size_type s, const T& value = T());
  inline void erase(iterator pbegin, iterator pend);
  inline void clear();
  inline void push_back(const T& value);

  reference       operator[](size_type i)       { return storage_[i]; }
  const_reference operator[](size_type i) const { return storage_[i]; }

  reference       front()       { return storage_.front(); }
  const_reference front() const { return storage_.front(); }
  reference       back()        { return endused_[-1]; }
  const_reference back()  const { return endused_[-1]; }

  iterator       begin()       { return storage_.begin(); }
  const_iterator begin() const { return storage_.begin(); }
  iterator       end()         { return endused_; }
  const_iterator end()   const { return endused_; }

  reverse_iterator       rbegin()       { return reverse_iterator(endused_); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(endused_); }
  reverse_iterator       rend()         { return reverse_iterator(storage_.begin()); }
  const_reverse_iterator rend()   const { return const_reverse_iterator(storage_.begin()); }
};

template <class T> inline // static
typename UncheckedVector<T>::iterator
UncheckedVector<T>::destroy_backward_(iterator pbegin, iterator pend)
{
  while (pend != pbegin)
  {
    --pend;
    pend->~T();
  }
  return pbegin;
}

template <class T> inline
void UncheckedVector<T>::clear()
{
  endused_ = destroy_backward_(storage_.begin(), endused_);
}

template <class T> inline
void UncheckedVector<T>::erase(iterator pbegin, iterator pend)
{
  endused_ = destroy_backward_(stdext::unchecked_copy(pend, endused_, pbegin), endused_);
}

template <class T> inline
UncheckedVector<T>::~UncheckedVector()
{
  destroy_backward_(storage_.begin(), endused_);
}

template <class T> inline
UncheckedVector<T>::UncheckedVector(size_type s, const T& value)
:
  storage_ ((s + chunkmask) & ~chunkmask),
  endused_ (storage_.begin() + s)
{
  stdext::unchecked_uninitialized_fill_n(storage_.begin(), s, value);
}

template <class T> inline
UncheckedVector<T>::UncheckedVector(const UncheckedVector<T>& b)
:
  storage_ ((b.size() + chunkmask) & ~chunkmask),
  endused_ (stdext::unchecked_uninitialized_copy(b.begin(), b.end(), storage_.begin()))
{}

template <class T> inline
void UncheckedVector<T>::swap(UncheckedVector<T>& b)
{
  storage_.swap(b.storage_);
  std::swap(endused_, b.endused_);
}

template <class T> inline
UncheckedVector<T>& UncheckedVector<T>::operator=(const UncheckedVector<T>& b)
{
  UncheckedVector<T> temp (b);
  this->swap(temp);
  return *this;
}

// Calls to expand_() are excellent points to stop inline expansion.
// Apart from reducing code size, this will nudge the compiler to pursue
// other, more rewarding inlining opportunities.
template <class T> __declspec(noinline)
void UncheckedVector<T>::expand_(size_type c)
{
  Util::MemChunk<T> temp ((c + chunkmask) & ~chunkmask);

  const iterator pend = endused_;
  endused_ = stdext::unchecked_uninitialized_copy(storage_.begin(), pend, temp.begin());
  storage_.swap(temp);
  destroy_backward_(temp.begin(), pend);
}

template <class T> inline
void UncheckedVector<T>::reserve(size_type c)
{
  if (c > storage_.size())
    expand_(c);
}

template <class T> inline
void UncheckedVector<T>::resize(size_type s, const T& value)
{
  const size_type oldsize = endused_ - storage_.begin();

  if (s > oldsize)
  {
    if (s > storage_.size())
      expand_(s);
    stdext::unchecked_uninitialized_fill_n(endused_, s - oldsize, value);
    endused_ = storage_.begin() + s;
  }
  else
    endused_ = destroy_backward_(storage_.begin() + s, endused_);
}

template <class T> inline
void UncheckedVector<T>::push_back(const T& value)
{
  void* dest = endused_;

  if (dest == storage_.end())
  {
    expand_(storage_.size() / 2 * 3 + 2);
    dest = endused_;
  }
  __assume(dest != 0);
  endused_ = new(dest) T(value) + 1;
}

template <class T> inline
void swap(UncheckedVector<T>& a, UncheckedVector<T>& b)
{
  a.swap(b);
}

} // namespace Util
