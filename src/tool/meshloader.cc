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

#include "meshloader.h"

#include <glib.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>

namespace Somato
{

MeshLoader::MeshLoader()
:
  importer_ {new Assimp::Importer{}}
{
  importer_->SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                                aiComponent_TANGENTS_AND_BITANGENTS
                                | aiComponent_COLORS
                                | aiComponent_TEXCOORDS
                                | aiComponent_BONEWEIGHTS);
  importer_->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                                aiPrimitiveType_POINT
                                | aiPrimitiveType_LINE);
  importer_->SetPropertyInteger(AI_CONFIG_PP_ICL_PTCACHE_SIZE, 20);
}

MeshLoader::~MeshLoader()
{}

bool MeshLoader::read_file(const char* filename)
{
  return (importer_->ReadFile(filename,
                              aiProcess_RemoveComponent
                              | aiProcess_JoinIdenticalVertices
                              | aiProcess_Triangulate
                              | aiProcess_SortByPType
                              | aiProcess_GenSmoothNormals
                              | aiProcess_ImproveCacheLocality) != nullptr);
}

const char* MeshLoader::get_error_string() const
{
  return importer_->GetErrorString();
}

MeshLoader::Node MeshLoader::lookup_node(const char* name) const
{
  const aiScene *const scene = importer_->GetScene();
  g_return_val_if_fail(scene, Node{});

  return Node{scene->mRootNode->FindNode(name)};
}

MeshLoader::VertexTriangleCounts
MeshLoader::count_node_vertices_triangles(Node node) const
{
  VertexTriangleCounts counts {0, 0};

  g_return_val_if_fail(node, counts);

  const aiScene *const scene = importer_->GetScene();
  const aiMesh *const *const scene_meshes = scene->mMeshes;

  for (unsigned int i = 0; i < node->mNumMeshes; ++i)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[i]];

    counts.first  += mesh->mNumVertices;
    counts.second += mesh->mNumFaces;
  }
  return counts;
}

std::size_t MeshLoader::get_node_vertices(Node node, MeshVertex* buffer,
                                          std::size_t max_vertices) const
{
  std::size_t n_written = 0;

  g_return_val_if_fail(node, n_written);
  g_return_val_if_fail(buffer, n_written);

  const aiScene *const scene = importer_->GetScene();
  const aiMesh *const *const scene_meshes = scene->mMeshes;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->HasNormals(), n_written);

    const auto *const vertices = mesh->mVertices;
    const auto *const normals  = mesh->mNormals;
    const std::size_t n_vertices =
        std::min<std::size_t>(mesh->mNumVertices, max_vertices - n_written);

    for (std::size_t i = 0; i < n_vertices; ++i)
    {
      buffer[n_written + i].set(vertices[i].x, vertices[i].y, vertices[i].z,
                                normals [i].x, normals [i].y, normals [i].z);
    }
    n_written += n_vertices;
  }
  return n_written;
}

std::size_t MeshLoader::get_node_indices(Node node, unsigned int base,
                                         MeshIndex* buffer, std::size_t max_indices) const
{
  std::size_t n_written = 0;

  g_return_val_if_fail(node, n_written);
  g_return_val_if_fail(buffer, n_written);

  const aiScene *const scene = importer_->GetScene();
  const aiMesh *const *const scene_meshes = scene->mMeshes;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE, n_written);
    g_return_val_if_fail(mesh->mNumFaces < (~MeshIndex{0} - base) / 3, n_written);

    const aiFace *const faces = mesh->mFaces;
    const std::size_t n_faces =
        std::min<std::size_t>(mesh->mNumFaces, (max_indices - n_written) / 3);

    for (std::size_t i = 0; i < n_faces; ++i)
    {
      const unsigned int *const indices = faces[i].mIndices;

      const unsigned int i0 = base + indices[0];
      const unsigned int i1 = base + indices[1];
      const unsigned int i2 = base + indices[2];

      buffer[n_written + 3*i]     = i0;
      buffer[n_written + 3*i + 1] = i1;
      buffer[n_written + 3*i + 2] = i2;
    }
    n_written += 3 * n_faces;
  }
  for (std::size_t n = n_written; n < max_indices; ++n)
    buffer[n] = ~MeshIndex{0};

  return n_written;
}

} // namespace Somato
