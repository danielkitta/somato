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

#include "meshtypes.h"

#include <memory>
#include <utility>
#include <cstddef>

namespace Assimp { class Importer; }
struct aiNode;

namespace Somato
{

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
private:
  const std::unique_ptr<Assimp::Importer> importer_;
};

} // namespace Somato

#endif // !SOMATO_MESHLOADER_H_INCLUDED
