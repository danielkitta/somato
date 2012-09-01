/*
 * Copyright (c) 2012  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_MESHLOADER_H_INCLUDED
#define SOMATO_MESHLOADER_H_INCLUDED

#include <sigc++/sigc++.h>
#include <string>
#include <utility>
#include <cstdint>

#include <config.h>

struct aiNode;

namespace GL
{

struct MeshVertex
{
  float texcoord[2];
  float normal[3];
  float vertex[3];
};

typedef uint16_t MeshIndex;

class MeshLoader
{
public:
  typedef const aiNode* Node;
  typedef std::pair<unsigned int, unsigned int> VertexTriangleCounts;

  explicit MeshLoader(std::string filename);
  virtual ~MeshLoader();

  void run();
  sigc::signal<void>& signal_done();

  Node lookup_node(const char* name) const;
  VertexTriangleCounts count_node_vertices_triangles(Node node) const;

  size_t get_node_vertices(Node node, MeshVertex* buffer, size_t max_vertices) const;
  size_t get_node_indices(Node node, unsigned int base,
                          MeshIndex* buffer, size_t max_indices) const;

private:
  class Impl;

  Impl *const pimpl_;

  // noncopyable
  MeshLoader(const MeshLoader&) = delete;
  MeshLoader& operator=(const MeshLoader&) = delete;

  void execute();
  void on_thread_exit();
};

} // namespace GL

#endif /* SOMATO_MESHLOADER_H_INCLUDED */
