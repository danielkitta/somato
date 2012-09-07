/*
 * Copyright (c) 2004-2008  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_ARRAY_H_INCLUDED
#define SOMATO_ARRAY_H_INCLUDED

#include <cstddef>
#include <algorithm>
#include <iterator>

namespace Util
{

template <class T>
class MemChunk
{
private:
  T* mbegin_;
  T* mend_;

  // noncopyable
  MemChunk(const MemChunk<T>&);
  MemChunk<T>& operator=(const MemChunk<T>&);

public:
  typedef T                                     value_type;
  typedef T&                                    reference;
  typedef const T&                              const_reference;
  typedef T*                                    iterator;
  typedef const T*                              const_iterator;
  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::size_t                           size_type;
  typedef std::ptrdiff_t                        difference_type;

  explicit MemChunk(size_type s)
    : mbegin_ (static_cast<T*>(operator new(sizeof(T) * s))), mend_ (mbegin_ + s) {}
  inline MemChunk() : mbegin_ (0), mend_ (0) {}
  inline ~MemChunk() { operator delete(mbegin_); }

  inline void swap(MemChunk<T>& b)
    { std::swap(mbegin_, b.mbegin_); std::swap(mend_, b.mend_); }

  inline size_type bytes() const { return size_type(mend_ - mbegin_) * sizeof(T); }
  inline size_type size()  const { return mend_ - mbegin_; }
  inline bool      empty() const { return (mbegin_ == mend_); }

  inline reference       operator[](size_type i)       { return mbegin_[i]; }
  inline const_reference operator[](size_type i) const { return mbegin_[i]; }

  inline reference       front()       { return mbegin_[0]; }
  inline const_reference front() const { return mbegin_[0]; }
  inline reference       back()        { return mend_[-1]; }
  inline const_reference back()  const { return mend_[-1]; }

  inline iterator       begin()       { return mbegin_; }
  inline const_iterator begin() const { return mbegin_; }
  inline iterator       end()         { return mend_; }
  inline const_iterator end()   const { return mend_; }

  inline reverse_iterator       rbegin()       { return reverse_iterator(mend_); }
  inline const_reverse_iterator rbegin() const { return const_reverse_iterator(mend_); }
  inline reverse_iterator       rend()         { return reverse_iterator(mbegin_); }
  inline const_reverse_iterator rend()   const { return const_reverse_iterator(mbegin_); }
};

template <class T> inline
void swap(MemChunk<T>& a, MemChunk<T>& b) { a.swap(b); }

} // namespace Util

#endif /* SOMATO_ARRAY_H_INCLUDED */
