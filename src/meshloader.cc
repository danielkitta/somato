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

#include "meshloader.h"
#include "mathutils.h"

#include <glib.h>
#include <sigc++/sigc++.h>
#include <glibmm/dispatcher.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#if 0
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#endif
#include <thread>

namespace GL
{

class MeshLoader::Impl
{
private:
  std::string                       filename_;
  Glib::Dispatcher                  signal_exit_;
  sigc::connection                  thread_exit_;
  std::unique_ptr<Assimp::Importer> importer_;

  void on_thread_exit();

  // noncopyable
  Impl(const MeshLoader::Impl&) = delete;
  MeshLoader::Impl& operator=(const MeshLoader::Impl&) = delete;

public:
  std::function<void ()> done_func;
  std::thread            thread;
  const aiScene*         scene;

  explicit Impl(std::string filename);
  ~Impl();

  void execute();
};

MeshLoader::Impl::Impl(std::string filename)
:
  filename_    {std::move(filename)},
  thread_exit_ {signal_exit_.connect(sigc::mem_fun(*this, &MeshLoader::Impl::on_thread_exit))},
  scene        {nullptr}
{}

MeshLoader::Impl::~Impl()
{
  thread_exit_.disconnect();

  // Normally, the thread should not be running anymore at this point,
  // but in case it is we have to wait in order to ensure proper cleanup.
  if (thread.joinable())
    thread.join();
}

void MeshLoader::Impl::execute()
{
  try
  {
    importer_.reset(new Assimp::Importer{});
#if 0
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE,
                                  aiDefaultLogStream_STDERR);

    importer_->SetPropertyBool(AI_CONFIG_GLOB_MEASURE_TIME, true);
#endif
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
  }
  catch (...)
  {
    signal_exit_(); // emit
    throw;
  }
  signal_exit_(); // emit
}

void MeshLoader::Impl::on_thread_exit()
{
  thread.join();

  if (done_func)
    done_func();
}

MeshLoader::MeshLoader(std::string filename)
:
  pimpl_ {new Impl{std::move(filename)}}
{}

MeshLoader::~MeshLoader()
{}

void MeshLoader::set_on_done(std::function<void ()> func)
{
  pimpl_->done_func = std::move(func);
}

void MeshLoader::run()
{
  g_return_if_fail(!pimpl_->thread.joinable());

  pimpl_->thread = std::thread{std::bind(&MeshLoader::Impl::execute, pimpl_.get())};
}

MeshLoader::Node MeshLoader::lookup_node(const char* name) const
{
  g_return_val_if_fail(!pimpl_->thread.joinable(), Node{});
  g_return_val_if_fail(pimpl_->scene != nullptr, Node{});

  return Node{pimpl_->scene->mRootNode->FindNode(name)};
}

MeshLoader::VertexTriangleCounts
MeshLoader::count_node_vertices_triangles(Node node) const
{
  VertexTriangleCounts counts {0, 0};

  g_return_val_if_fail(node, counts);
  g_return_val_if_fail(pimpl_->scene != nullptr, counts);

  for (unsigned int i = 0; i < node->mNumMeshes; ++i)
  {
    aiMesh *const mesh = pimpl_->scene->mMeshes[node->mMeshes[i]];

    counts.first  += mesh->mNumVertices;
    counts.second += mesh->mNumFaces;
  }

  return counts;
}

size_t MeshLoader::get_node_vertices(Node node, MeshVertex* buffer,
                                     size_t max_vertices) const
{
  size_t n_written = 0;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const auto mesh = pimpl_->scene->mMeshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->HasNormals(), n_written);

    const size_t n_vertices = Math::min<size_t>(mesh->mNumVertices, max_vertices - n_written);

    for (size_t vert_idx = 0; vert_idx < n_vertices; ++vert_idx)
    {
      const auto& mesh_vertex = mesh->mVertices[vert_idx];
      const auto& mesh_normal = mesh->mNormals[vert_idx];

      auto& element = buffer[n_written++];

      element.vertex[0] = mesh_vertex.x;
      element.vertex[1] = mesh_vertex.y;
      element.vertex[2] = mesh_vertex.z;

      element.normal[0] = mesh_normal.x;
      element.normal[1] = mesh_normal.y;
      element.normal[2] = mesh_normal.z;
    }
  }
  return n_written;
}

size_t MeshLoader::get_node_indices(Node node, unsigned int base,
                                    MeshIndex* buffer, size_t max_indices) const
{
  size_t n_written = 0;

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    const auto mesh = pimpl_->scene->mMeshes[node->mMeshes[mesh_idx]];

    g_return_val_if_fail(mesh->HasFaces(), n_written);
    g_return_val_if_fail(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE, n_written);

    const size_t n_faces = Math::min<size_t>(mesh->mNumFaces, (max_indices - n_written) / 3);

    for (size_t face_idx = 0; face_idx < n_faces; ++face_idx)
    {
      const auto& face = mesh->mFaces[face_idx];
      g_return_val_if_fail(face.mNumIndices == 3, n_written);

      const unsigned int i0 = face.mIndices[0];
      const unsigned int i1 = face.mIndices[1];
      const unsigned int i2 = face.mIndices[2];

      buffer[n_written]     = base + i0;
      buffer[n_written + 1] = base + i1;
      buffer[n_written + 2] = base + i2;

      n_written += 3;
    }
  }
  return n_written;
}

} // namespace GL
