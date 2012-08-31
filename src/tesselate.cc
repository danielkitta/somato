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

/*
 * TODO: The code herein is an ugly mess and badly needs a rewrite.
 */

#include "tesselate.h"
#include "appdata.h"
#include "array.h"
#include "puzzle.h"

#include <glib.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#include <array>

namespace
{

using Math::Matrix4;
using Math::Vector4;

} // anonymous namespace

namespace Somato
{

class CubeTesselator::Impl
{
private:
  Assimp::Importer importer_;
  const aiScene*   scene_;
  std::array<Cube, CUBE_PIECE_COUNT> puzzle_pieces_;

  // noncopyable
  Impl(const CubeTesselator::Impl&) = delete;
  CubeTesselator::Impl& operator=(const CubeTesselator::Impl&) = delete;

  void load_object(int idx, const Matrix4& matrix);
  bool find_piece_position(Cube piece, const Matrix4& rotation);

public:
  CubeElementArray* element_array;
  CubeIndexArray*   index_array;
  float             cellsize;
  int               trianglecount;

  Impl();
  ~Impl();

  void run(Cube piece);
};


CubeTesselator::CubeTesselator()
:
  pimpl_ (new Impl())
{}

CubeTesselator::~CubeTesselator()
{
  delete pimpl_; // exception-safe as it is the only member
}

void CubeTesselator::set_element_array(CubeElementArray* elements)
{
  g_return_if_fail(pimpl_->index_array == 0 || pimpl_->index_array->empty());

  pimpl_->element_array = elements;
}

CubeElementArray* CubeTesselator::get_element_array() const
{
  return pimpl_->element_array;
}

void CubeTesselator::set_range_arrays(RangeStartArray*, RangeCountArray*)
{
  g_return_if_reached();
}

RangeStartArray* CubeTesselator::get_range_start_array() const
{
  g_return_val_if_reached(nullptr);
}

RangeCountArray* CubeTesselator::get_range_count_array() const
{
  g_return_val_if_reached(nullptr);
}

void CubeTesselator::set_index_array(CubeIndexArray* indices)
{
  g_return_if_fail(indices == 0 || indices->empty());

  pimpl_->index_array = indices;
}

void CubeTesselator::set_cellsize(float value)
{
  pimpl_->cellsize = value;
}

float CubeTesselator::get_cellsize() const
{
  return pimpl_->cellsize;
}

int CubeTesselator::reset_triangle_count()
{
  const int value = pimpl_->trianglecount;
  pimpl_->trianglecount = 0;
  return value;
}

int CubeTesselator::get_triangle_count() const
{
  return pimpl_->trianglecount;
}

void CubeTesselator::run(Cube piece)
{
  pimpl_->run(piece);
}

CubeTesselator::Impl::Impl()
:
  importer_     (),
  scene_        (nullptr),
  element_array (nullptr),
  index_array   (nullptr),
  cellsize      (1.0),
  trianglecount (0)
{
  for (int i = 0; i < CUBE_PIECE_COUNT; ++i)
    puzzle_pieces_[i] = puzzle_piece_at_origin(i);

  Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE,
                                aiDefaultLogStream_STDERR);

  importer_.SetPropertyBool(AI_CONFIG_GLOB_MEASURE_TIME, true);

  importer_.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                               aiComponent_TANGENTS_AND_BITANGENTS
                               | aiComponent_COLORS
                               | aiComponent_TEXCOORDS
                               | aiComponent_BONEWEIGHTS);
  importer_.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                               aiPrimitiveType_POINT
                               | aiPrimitiveType_LINE);
  importer_.SetPropertyInteger(AI_CONFIG_PP_ICL_PTCACHE_SIZE, 20);

  scene_ = importer_.ReadFile(Util::locate_data_file("puzzlepieces.dae"),
                              aiProcess_RemoveComponent
                              | aiProcess_JoinIdenticalVertices
                              | aiProcess_Triangulate
                              | aiProcess_SortByPType
                              | aiProcess_GenSmoothNormals
                              | aiProcess_ImproveCacheLocality);

  g_return_if_fail(scene_ != nullptr);
}

CubeTesselator::Impl::~Impl()
{
  Assimp::DefaultLogger::kill();
}

void CubeTesselator::Impl::load_object(int idx, const Matrix4& matrix)
{
  static const char object_names[CUBE_PIECE_COUNT][16] =
  {
    "PieceOrange",
    "PieceGreen",
    "PieceRed",
    "PieceYellow",
    "PieceBlue",
    "PieceLavender",
    "PieceCyan"
  };
  g_assert(idx >= 0 && idx < CUBE_PIECE_COUNT);

  aiNode *const node = scene_->mRootNode->FindNode(object_names[idx]);
  g_return_if_fail(node != nullptr);

  for (unsigned int mesh_idx = 0; mesh_idx < node->mNumMeshes; ++mesh_idx)
  {
    aiMesh *const mesh = scene_->mMeshes[node->mMeshes[mesh_idx]];

    g_return_if_fail(mesh->HasFaces() && mesh->HasNormals());
    g_return_if_fail(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);

    const auto start_offset = element_array->size();

    for (unsigned int vert_idx = 0; vert_idx < mesh->mNumVertices; ++vert_idx)
    {
      const aiVector3D& mesh_vertex = mesh->mVertices[vert_idx];
      const aiVector3D& mesh_normal = mesh->mNormals[vert_idx];

      const Vector4 vertex {mesh_vertex.x, mesh_vertex.y, mesh_vertex.z, 1.0f};
      const Vector4 normal {mesh_normal.x, mesh_normal.y, mesh_normal.z, 0.0f};

      const Vector4 texcoord {0.5f * mesh_vertex.x + 0.75f,
                              0.75f - 0.5f * mesh_vertex.z,
                              0.5f * mesh_vertex.y + 0.75f,
                              1.0f};

      element_array->push_back(CubeElement{texcoord, matrix * normal, matrix * vertex});
    }

    for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; ++face_idx)
    {
      const aiFace& face = mesh->mFaces[face_idx];
      g_return_if_fail(face.mNumIndices == 3);

      for (unsigned int i = 0; i < 3; ++i)
      {
        const unsigned int vi = face.mIndices[i];
        g_return_if_fail(vi < mesh->mNumVertices);

        index_array->push_back(start_offset + vi);
      }
    }
    trianglecount += mesh->mNumFaces;
  }
}

bool CubeTesselator::Impl::find_piece_position(Cube piece, const Matrix4& rotation)
{
  int z = 0;

  for (Cube piece_z = piece; piece_z != Cube(); piece_z.shift_rev(Cube::AXIS_Z))
  {
    int y = 0;

    for (Cube piece_y = piece_z; piece_y != Cube(); piece_y.shift_rev(Cube::AXIS_Y))
    {
      int x = 0;

      for (Cube piece_x = piece_y; piece_x != Cube(); piece_x.shift_rev(Cube::AXIS_X))
      {
        for (int idx = 0; idx < CUBE_PIECE_COUNT; ++idx)
          if (piece_x == puzzle_pieces_[idx])
          {
            Matrix4 translation {Matrix4::identity[0],
                                 Matrix4::identity[1],
                                 Matrix4::identity[2],
                                 Vector4{x * cellsize, y * cellsize, z * cellsize, 1.0f}};

            load_object(idx, rotation * translation);
            return true;
          }

        ++x;
      }
      ++y;
    }
    --z;
  }
  return false;
}

void CubeTesselator::Impl::run(Cube piece)
{
  static const Matrix4::array_type rotate90[3] =
  {
    { {1, 0,  0, 0}, { 0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1} }, // 90 deg around x
    { {0, 0, -1, 0}, { 0, 1,  0, 0}, {1, 0, 0, 0}, {0, 0, 0, 1} }, // 90 deg around y
    { {0, 1,  0, 0}, {-1, 0,  0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} }  // 90 deg around z
  };

  g_return_if_fail(element_array != nullptr);
  g_return_if_fail(index_array != nullptr);

  Matrix4 rotation;

  for (unsigned int i = 0;; ++i)
  {
    // Add the 4 possible orientations of each cube side.
    for (int k = 0; k < 4; ++k)
    {
      if (find_piece_position(piece, rotation))
        return;

      piece.rotate(Cube::AXIS_Z);
      rotation *= rotate90[Cube::AXIS_Z];
    }

    if (i == 5)
      break;

    // Due to the zigzagging performed here, only 5 rotations are
    // necessary to move each of the 6 cube sides in turn to the front.
    piece.rotate(i % 2);
    rotation *= rotate90[i % 2];
  }
}

} // namespace Somato
