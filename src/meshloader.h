/*
 * Copyright (c) 2012-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_MESHLOADER_H_INCLUDED
#define SOMATO_MESHLOADER_H_INCLUDED

#include <memory>
#include <utility>
#include <cmath>
#include <cstddef>

namespace Assimp { class Importer; }
struct aiNode;

namespace Somato
{

/* Vector packed as 32-bit unsigned integer.
 */
enum Int_2_10_10_10_rev : unsigned int {};

/* Convert a floating-point normal into a packed 10-bit integer format.
 */
inline Int_2_10_10_10_rev pack_normal(float x, float y, float z)
{
  const float scale = 511.f;
  const int ix = std::lrint(x * scale);
  const int iy = std::lrint(y * scale);
  const int iz = std::lrint(z * scale);

  return static_cast<Int_2_10_10_10_rev>((ix & 0x3FFu)
                                      | ((iy & 0x3FFu) << 10)
                                      | ((iz & 0x3FFu) << 20));
}

struct MeshVertex
{
  float              position[3];
  Int_2_10_10_10_rev normal;

  void set(float px, float py, float pz, float nx, float ny, float nz)
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = pack_normal(nx, ny, nz);
  }
  void set(float px, float py, float pz)
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = static_cast<Int_2_10_10_10_rev>(0);
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

class MeshLoader
{
public:
  class Node
  {
  private:
    const aiNode* node_;

  public:
    explicit constexpr Node(const aiNode* node = nullptr) : node_ {node} {}

    explicit operator bool() const { return (node_ != nullptr); }
    const aiNode* operator->() const { return node_; }
  };

  typedef std::pair<unsigned int, unsigned int> VertexTriangleCounts;

  MeshLoader();
  ~MeshLoader();

  bool read_file(const char* filename);
  const char* get_error_string() const;

  Node lookup_node(const char* name) const;
  VertexTriangleCounts count_node_vertices_triangles(Node node) const;

  std::size_t get_node_vertices(Node node, MeshVertex* buffer,
                                std::size_t max_vertices) const;
  std::size_t get_node_indices(Node node, unsigned int base,
                               MeshIndex* buffer, std::size_t max_indices) const;

  static unsigned int aligned_index_count(unsigned int count)
    { return (count + 7) & ~7u; }

private:
  const std::unique_ptr<Assimp::Importer> importer_;
};

} // namespace Somato

#endif // !SOMATO_MESHLOADER_H_INCLUDED
