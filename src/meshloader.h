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

#include "asynctask.h"
#include "glutils.h"

#include <memory>
#include <string>
#include <cstddef>

struct aiNode;

namespace GL
{

struct MeshVertex
{
  float              position[3];
  Int_2_10_10_10_rev normal;

  void set(float px, float py, float pz, float nx, float ny, float nz) volatile
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = pack_normal(nx, ny, nz);
  }
  void set(float px, float py, float pz) volatile
  {
    position[0] = px;
    position[1] = py;
    position[2] = pz;
    normal = static_cast<Int_2_10_10_10_rev>(0);
  }
};

typedef unsigned short MeshIndex;

class MeshLoader : public Async::Task
{
public:
  class Node
  {
  private:
    const aiNode* node_;

  public:
    explicit Node(const aiNode* node = nullptr) : node_ {node} {}

    explicit operator bool() const { return (node_ != nullptr); }
    const aiNode* operator->() const { return node_; }
  };

  typedef std::pair<unsigned int, unsigned int> VertexTriangleCounts;

  explicit MeshLoader(std::string filename);
  virtual ~MeshLoader();

  Node lookup_node(const char* name) const;
  VertexTriangleCounts count_node_vertices_triangles(Node node) const;

  size_t get_node_vertices(Node node, volatile MeshVertex* buffer,
                           size_t max_vertices) const;
  size_t get_node_indices(Node node, unsigned int base,
                          volatile MeshIndex* buffer, size_t max_indices) const;

  static unsigned int aligned_index_count(unsigned int count)
    { return (count + 3) & ~3u; }

private:
  void execute() override;

  class Impl;
  const std::unique_ptr<Impl> pimpl_;
};

} // namespace GL

#endif // !SOMATO_MESHLOADER_H_INCLUDED
