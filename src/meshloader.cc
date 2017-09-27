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

#include <config.h>

#include "meshloader.h"
#include "assetresourceio.h"
#include "glutils.h"

#include <glib.h>
#include <glibmm/ustring.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <chrono>

namespace GL
{

class MeshLoader::Impl
{
private:
  std::string                       filename_;
  std::unique_ptr<Assimp::Importer> importer_;

  // noncopyable
  Impl(const MeshLoader::Impl&) = delete;
  MeshLoader::Impl& operator=(const MeshLoader::Impl&) = delete;

public:
  const aiScene* scene = nullptr;

  explicit Impl(std::string filename);
  ~Impl();

  void execute();
};

MeshLoader::Impl::Impl(std::string filename)
:
  filename_ {std::move(filename)}
{}

MeshLoader::Impl::~Impl()
{}

void MeshLoader::Impl::execute()
{
  importer_.reset(new Assimp::Importer{});
  importer_->SetIOHandler(new Util::AssetResourceIoSystem{});

  importer_->SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                                aiComponent_TANGENTS_AND_BITANGENTS
                                | aiComponent_COLORS
                                | aiComponent_TEXCOORDS
                                | aiComponent_BONEWEIGHTS);
  importer_->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                                aiPrimitiveType_POINT
                                | aiPrimitiveType_LINE);
  importer_->SetPropertyInteger(AI_CONFIG_PP_ICL_PTCACHE_SIZE, 20);

  scene = importer_->ReadFile(filename_,
                              aiProcess_RemoveComponent
                              | aiProcess_JoinIdenticalVertices
                              | aiProcess_Triangulate
                              | aiProcess_SortByPType
                              | aiProcess_GenSmoothNormals
                              | aiProcess_ImproveCacheLocality);
  if (!scene)
    throw GL::Error{importer_->GetErrorString()};
}

MeshLoader::MeshLoader(std::string filename)
:
  pimpl_ {new Impl{std::move(filename)}}
{}

MeshLoader::~MeshLoader()
{
  wait_finish();
}

MeshLoader::Node MeshLoader::lookup_node(const char* name) const
{
  g_return_val_if_fail(!running(), Node{});
  rethrow_any_error();
  g_return_val_if_fail(pimpl_->scene, Node{});

  return Node{pimpl_->scene->mRootNode->FindNode(name)};
}

MeshLoader::VertexTriangleCounts
MeshLoader::count_node_vertices_triangles(Node node) const
{
  VertexTriangleCounts counts {0, 0};

  g_return_val_if_fail(node, counts);
  g_return_val_if_fail(pimpl_->scene, counts);

  const aiMesh *const *const scene_meshes = pimpl_->scene->mMeshes;

  for (unsigned int i = 0; i < node->mNumMeshes; ++i)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[i]];

    counts.first  += mesh->mNumVertices;
    counts.second += mesh->mNumFaces;
  }
  return counts;
}

size_t MeshLoader::get_node_vertices(Node node, volatile MeshVertex* buffer,
                                     size_t max_vertices) const
{
  size_t n_written = 0;

  g_return_val_if_fail(node, n_written);
  g_return_val_if_fail(buffer, n_written);

  const aiMesh *const *const scene_meshes = pimpl_->scene->mMeshes;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->HasNormals(), n_written);

    const auto *const vertices = mesh->mVertices;
    const auto *const normals  = mesh->mNormals;
    const size_t n_vertices = std::min<size_t>(mesh->mNumVertices,
                                               max_vertices - n_written);
    for (size_t i = 0; i < n_vertices; ++i)
    {
      buffer[n_written + i].set(vertices[i].x, vertices[i].y, vertices[i].z,
                                normals [i].x, normals [i].y, normals [i].z);
    }
    n_written += n_vertices;
  }
  return n_written;
}

size_t MeshLoader::get_node_indices(Node node, unsigned int base,
                                    volatile MeshIndex* buffer, size_t max_indices) const
{
  size_t n_written = 0;

  g_return_val_if_fail(node, n_written);
  g_return_val_if_fail(buffer, n_written);

  const aiMesh *const *const scene_meshes = pimpl_->scene->mMeshes;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const aiMesh *const mesh = scene_meshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE, n_written);

    const aiFace *const faces = mesh->mFaces;
    const size_t n_faces = std::min<size_t>(mesh->mNumFaces,
                                            (max_indices - n_written) / 3);
    for (size_t i = 0; i < n_faces; ++i)
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
  for (size_t n = n_written; n < max_indices; ++n)
    buffer[n] = ~MeshIndex{0};

  return n_written;
}

void MeshLoader::execute()
{
  const auto start = std::chrono::steady_clock::now();

  pimpl_->execute();

  const auto stop = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::milli> elapsed = stop - start;

  g_info("Mesh load time: %0.1f ms", elapsed.count());
}

} // namespace GL
