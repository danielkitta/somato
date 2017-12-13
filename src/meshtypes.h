/*
 * Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_MESHTYPES_H_INCLUDED
#define SOMATO_MESHTYPES_H_INCLUDED

#include "gltypes.h"

#include <glib.h>
#include <cstring>

namespace Somato
{

struct MeshVertex
{
  float                   position[3];
  GL::Int_2_10_10_10_rev  normal;

  void set(float px, float py, float pz, float nx, float ny, float nz)
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = GL::pack_3i10rev_norm(nx, ny, nz);
  }
  void set(float px, float py, float pz)
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = static_cast<GL::Int_2_10_10_10_rev>(0);
  }
  void swap_bytes()
  {
    guint32 words[4];
    std::memcpy(words, this, sizeof words);

    words[0] = GUINT32_SWAP_LE_BE(words[0]);
    words[1] = GUINT32_SWAP_LE_BE(words[1]);
    words[2] = GUINT32_SWAP_LE_BE(words[2]);
    words[3] = GUINT32_SWAP_LE_BE(words[3]);

    std::memcpy(this, words, sizeof words);
  }
};

typedef unsigned short MeshIndex;

struct MeshDesc
{
  unsigned int triangle_count; // number of triangles
  unsigned int indices_offset; // offset into element indices array
  unsigned int element_first;  // minimum referenced element index
  unsigned int element_last;   // maximum referenced element index

  unsigned int element_count() const { return element_last - element_first + 1; }

  void swap_bytes()
  {
    triangle_count = GUINT32_SWAP_LE_BE(triangle_count);
    indices_offset = GUINT32_SWAP_LE_BE(indices_offset);
    element_first  = GUINT32_SWAP_LE_BE(element_first);
    element_last   = GUINT32_SWAP_LE_BE(element_last);
  }
};

/* Cube cell grid vertex and primitive counts.
 */
enum
{
  GRID_CUBE_SIZE    = 3,
  GRID_VERTEX_COUNT = (GRID_CUBE_SIZE + 1) * (GRID_CUBE_SIZE + 1) * (GRID_CUBE_SIZE + 1),
  GRID_LINE_COUNT   = (GRID_CUBE_SIZE + 1) * (GRID_CUBE_SIZE + 1) *  GRID_CUBE_SIZE * 3
};

/* Side length of a cube cell in unzoomed model units.
 */
constexpr float grid_cell_size = 1.;

inline unsigned int aligned_index_count(unsigned int count)
  { return (count + 7) & ~7u; }

} // namespace Somato

#endif // !SOMATO_MESHTYPES_H_INCLUDED
