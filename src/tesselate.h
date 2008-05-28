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

#ifndef SOMATO_TESSELATE_H_INCLUDED
#define SOMATO_TESSELATE_H_INCLUDED

#include "cube.h"
#include "vectormath.h"

#include <vector>
#include <config.h>

// We only need a few trivial type and constant definitions from OpenGL.
// The tesselator only generates the geometry, but doesn't render it.
#include <GL/gl.h>

namespace Somato
{

typedef Math::Vector233 CubeElement;
typedef GLushort        CubeIndex;

#if SOMATO_USE_UNCHECKEDVECTOR
typedef Util::UncheckedVector<CubeElement>  CubeElementArray;
typedef Util::UncheckedVector<CubeIndex>    CubeIndexArray;
typedef Util::UncheckedVector<GLint>        RangeStartArray;
typedef Util::UncheckedVector<GLsizei>      RangeCountArray;
#else
typedef std::vector<CubeElement>            CubeElementArray;
typedef std::vector<CubeIndex>              CubeIndexArray;
typedef std::vector<GLint>                  RangeStartArray;
typedef std::vector<GLsizei>                RangeCountArray;
#endif /* !SOMATO_USE_UNCHECKEDVECTOR */

enum
{
  CUBE_ELEMENT_TYPE = GL_T2F_N3F_V3F,
  CUBE_INDEX_TYPE   = GL_UNSIGNED_SHORT
};

/*
 * This class provides a tesselator specialized for Cube surfaces.
 */
class CubeTesselator
{
public:
  CubeTesselator();
  virtual ~CubeTesselator();

  void set_element_array(CubeElementArray* elements);
  CubeElementArray* get_element_array() const;

  void set_range_arrays(RangeStartArray* start, RangeCountArray* count);
  RangeStartArray* get_range_start_array() const;
  RangeCountArray* get_range_count_array() const;

  void set_index_array(CubeIndexArray* indices);
  CubeIndexArray* get_index_array() const;

  void set_cellsize(float value);
  float get_cellsize() const;

  int reset_triangle_count();
  int get_triangle_count() const;

  void run(Cube piece);

private:
  // Being essentially a state machine, CubeTesselator is literally begging
  // to be implemented using the pimpl (pointer to implementation) idiom.
  class Impl;

  Impl *const pimpl_;

  // noncopyable
  CubeTesselator(const CubeTesselator&);
  CubeTesselator& operator=(const CubeTesselator&);
};

} // namespace Somato

#endif /* SOMATO_TESSELATE_H_INCLUDED */
