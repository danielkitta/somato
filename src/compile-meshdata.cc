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

#include "meshloader.h"

#include <glib.h>
#include <iostream>
#include <vector>
#include <cstddef>

namespace
{

using namespace Somato;
using MeshNodes = std::vector<MeshLoader::Node>;

/* Generate a grid of solid lines along the cell boundaries.
 * Note that the lines are split at the crossing points in order to avoid
 * gaps, and also to more closely match the tesselation of the cube parts.
 */
void generate_grid_vertices(MeshVertex* vertices)
{
  enum { N = GRID_CUBE_SIZE + 1 };
  float stride[N];

  for (int i = 0; i < N; ++i)
    stride[i] = (2 * i - (N - 1)) * (0.5f * grid_cell_size);

  auto* pv = vertices;

  for (int z = 0; z < N; ++z)
    for (int y = 0; y < N; ++y)
      for (int x = 0; x < N; ++x)
      {
        pv->set(stride[x], stride[y], stride[z]);
        ++pv;
      }
}

void generate_grid_indices(MeshIndex* indices)
{
  enum { N = GRID_CUBE_SIZE + 1 };
  auto* pi = indices;

  for (int i = 0; i < N; ++i)
    for (int k = 0; k < N; ++k)
      for (int m = 0; m < N-1; ++m)
      {
        pi[0] = N*N*i + N*k + m;
        pi[1] = N*N*i + N*k + m + 1;

        pi[2] = N*N*i + N*m + k;
        pi[3] = N*N*i + N*m + k + N;

        pi[4] = N*N*m + N*i + k;
        pi[5] = N*N*m + N*i + k + N*N;

        pi += 6;
      }
}

bool fill_mesh_data(const MeshLoader& loader, const MeshNodes& nodes,
                    std::vector<MeshDesc>&   mesh_desc,
                    std::vector<MeshVertex>& mesh_vertices,
                    std::vector<MeshIndex>&  mesh_indices)
{
  mesh_desc.reserve(nodes.size());

  unsigned int total_vertices = GRID_VERTEX_COUNT;
  unsigned int indices_offset = MeshLoader::aligned_index_count(GRID_LINE_COUNT * 2);

  for (const auto node : nodes)
  {
    const auto counts = loader.count_node_vertices_triangles(node);

    if (counts.first <= 0 || counts.second <= 0)
      return false;

    mesh_desc.push_back({counts.second, indices_offset,
                         total_vertices, total_vertices + counts.first - 1});
    total_vertices += counts.first;
    indices_offset += MeshLoader::aligned_index_count(3 * counts.second);
  }
  mesh_vertices.resize(total_vertices);
  mesh_indices.resize(indices_offset);

  generate_grid_vertices(&mesh_vertices[0]);
  generate_grid_indices(&mesh_indices[0]);

  for (std::size_t i = 0; i < nodes.size(); ++i)
  {
    const auto  node = nodes[i];
    const auto& mesh = mesh_desc[i];

    loader.get_node_vertices(node, &mesh_vertices[mesh.element_first],
                             mesh.element_count());
    loader.get_node_indices(node, mesh.element_first, &mesh_indices[mesh.indices_offset],
                            MeshLoader::aligned_index_count(3 * mesh.triangle_count));
  }
  return true;
}

bool write_raw_data_file(const char* filename, const void* data, std::size_t size)
{
  GError* error = nullptr;

  if (!g_file_set_contents(filename, static_cast<const char*>(data), size, &error))
  {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return false;
  }
  return true;
}

template <typename T>
inline bool write_data_file(const char* filename, const std::vector<T>& data)
{
  return write_raw_data_file(filename, &data[0], data.size() * sizeof(T));
}

} // anonymous namespace

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cerr << "Missing arguments" << std::endl;
    return 1;
  }
  MeshLoader loader;

  if (!loader.read_file(argv[1]))
  {
    std::cerr << loader.get_error_string() << std::endl;
    return 1;
  }
  MeshNodes nodes;
  nodes.reserve(argc - 2);

  for (int i = 2; i < argc; ++i)
  {
    const auto node = loader.lookup_node(argv[i]);
    if (!node)
    {
      std::cerr << "Failed to load mesh of " << argv[i] << std::endl;
      return 1;
    }
    nodes.push_back(node);
  }
  std::vector<MeshDesc>   mesh_desc;
  std::vector<MeshVertex> mesh_vertices;
  std::vector<MeshIndex>  mesh_indices;

  if (!fill_mesh_data(loader, nodes, mesh_desc, mesh_vertices, mesh_indices))
  {
    std::cerr << "Failed to get mesh data" << std::endl;
    return 1;
  }
  if (!write_data_file("ui/mesh-vertices.bin", mesh_vertices) ||
      !write_data_file("ui/mesh-indices.bin",  mesh_indices)  ||
      !write_data_file("ui/mesh-desc.bin",     mesh_desc))
    return 1;

  return 0;
}
