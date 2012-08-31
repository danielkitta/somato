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
#include "array.h"
#include "puzzle.h"

#include <glib.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#include <cmath>
#include <algorithm>
#include <memory>
#include <iostream>

namespace
{

using Math::Matrix4;
using Math::Vector4;

void dump_cube(Somato::Cube cube)
{
  std::cout << ' ' << (cube.get(0, 2, 0) ? '#' : '.')
                   << (cube.get(1, 2, 0) ? '#' : '.')
                   << (cube.get(2, 2, 0) ? '#' : '.')
            << ' ' << (cube.get(0, 2, 1) ? '#' : '.')
                   << (cube.get(1, 2, 1) ? '#' : '.')
                   << (cube.get(2, 2, 1) ? '#' : '.')
            << ' ' << (cube.get(0, 2, 2) ? '#' : '.')
                   << (cube.get(1, 2, 2) ? '#' : '.')
                   << (cube.get(2, 2, 2) ? '#' : '.')
            << '\n'
            << ' ' << (cube.get(0, 1, 0) ? '#' : '.')
                   << (cube.get(1, 1, 0) ? '#' : '.')
                   << (cube.get(2, 1, 0) ? '#' : '.')
            << ' ' << (cube.get(0, 1, 1) ? '#' : '.')
                   << (cube.get(1, 1, 1) ? '#' : '.')
                   << (cube.get(2, 1, 1) ? '#' : '.')
            << ' ' << (cube.get(0, 1, 2) ? '#' : '.')
                   << (cube.get(1, 1, 2) ? '#' : '.')
                   << (cube.get(2, 1, 2) ? '#' : '.')
            << '\n'
            << ' ' << (cube.get(0, 0, 0) ? '#' : '.')
                   << (cube.get(1, 0, 0) ? '#' : '.')
                   << (cube.get(2, 0, 0) ? '#' : '.')
            << ' ' << (cube.get(0, 0, 1) ? '#' : '.')
                   << (cube.get(1, 0, 1) ? '#' : '.')
                   << (cube.get(2, 0, 1) ? '#' : '.')
            << ' ' << (cube.get(0, 0, 2) ? '#' : '.')
                   << (cube.get(1, 0, 2) ? '#' : '.')
                   << (cube.get(2, 0, 2) ? '#' : '.')
            << "\n\n";
}

void dump_transform(const Matrix4& matrix)
{
  std::cout << '\t' << matrix[0][0] << '\t' << matrix[1][0]
            << '\t' << matrix[2][0] << '\t' << matrix[3][0]
            << '\n'
            << '\t' << matrix[0][1] << '\t' << matrix[1][1]
            << '\t' << matrix[2][1] << '\t' << matrix[3][1]
            << '\n'
            << '\t' << matrix[0][2] << '\t' << matrix[1][2]
            << '\t' << matrix[2][2] << '\t' << matrix[3][2]
            << '\n'
            << '\t' << matrix[0][3] << '\t' << matrix[1][3]
            << '\t' << matrix[2][3] << '\t' << matrix[3][3]
            << "\n\n";
}

void dump_transform(const Matrix4& matrix, Somato::Cube cube)
{
  using Somato::Cube;

  Cube trans;

  for (int x = 0; x < Cube::N; ++x)
    for (int y = 0; y < Cube::N; ++y)
      for (int z = 0; z < Cube::N; ++z)
        if (cube.get(x, y, z))
        {
          const Vector4 vec = matrix * Vector4{x - 1.0f, y - 1.0f, 1.0f - z, 1.0f};
          const int dx = int(vec.x()) + 1;
          const int dy = int(vec.y()) + 1;
          const int dz = 1 - int(vec.z());

          if (dx >= 0 && dx < Cube::N
              && dy >= 0 && dy < Cube::N
              && dz >= 0 && dz < Cube::N)
            trans.put(dx, dy, dz, true);
        }

  dump_cube(trans);
}

} // anonymous namespace

namespace Somato
{

class CubeTesselator::Impl
{
private:
  Assimp::Importer  importer_;
  const aiScene*    scene_;

  // noncopyable
  Impl(const CubeTesselator::Impl&);
  CubeTesselator::Impl& operator=(const CubeTesselator::Impl&);

  void load_object(int idx, const Matrix4& matrix);
  bool compute_rotations(int idx, Cube cube, Cube piece, Vector4 translation);

public:
  CubeElementArray* element_array;
  RangeStartArray*  range_start_array;
  RangeCountArray*  range_count_array;
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
  g_return_if_fail(pimpl_->range_start_array == 0 || pimpl_->range_start_array->empty());
  g_return_if_fail(pimpl_->range_count_array == 0 || pimpl_->range_count_array->empty());
  g_return_if_fail(pimpl_->index_array == 0 || pimpl_->index_array->empty());

  pimpl_->element_array = elements;
}

CubeElementArray* CubeTesselator::get_element_array() const
{
  return pimpl_->element_array;
}

void CubeTesselator::set_range_arrays(RangeStartArray* start, RangeCountArray* count)
{
  g_return_if_fail((start == 0 && count == 0) ||
                   (start != 0 && count != 0 && start->empty() && count->empty()));
  g_return_if_fail(pimpl_->index_array == 0);

  pimpl_->range_start_array = start;
  pimpl_->range_count_array = count;
}

RangeStartArray* CubeTesselator::get_range_start_array() const
{
  return pimpl_->range_start_array;
}

RangeCountArray* CubeTesselator::get_range_count_array() const
{
  return pimpl_->range_count_array;
}

void CubeTesselator::set_index_array(CubeIndexArray* indices)
{
  g_return_if_fail(indices == 0 || indices->empty());
  g_return_if_fail(pimpl_->range_start_array == 0 && pimpl_->range_count_array == 0);

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
  importer_         (),
  scene_            (nullptr),
  element_array     (nullptr),
  range_start_array (nullptr),
  range_count_array (nullptr),
  index_array       (nullptr),
  cellsize          (1.0),
  trianglecount     (0)
{
  Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE,
                                aiDefaultLogStream_STDERR);

  scene_ = importer_.ReadFile("ui/puzzlepieces.dae",
                              aiProcess_JoinIdenticalVertices
                              | aiProcess_Triangulate
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
  static const char object_names[7][16] =
  {
    "PieceOrange",
    "PieceGreen",
    "PieceRed",
    "PieceYellow",
    "PieceBlue",
    "PieceLavender",
    "PieceCyan"
  };
  g_return_if_fail(idx >= 0 && unsigned(idx) < G_N_ELEMENTS(object_names));

  aiNode *const node = scene_->mRootNode->FindNode(object_names[idx]);
  g_return_if_fail(node != nullptr);
#if 0
  std::cout << "Final transform (" << object_names[idx] << ")\n";
  dump_transform(matrix, puzzle_piece_at_origin(idx));
#endif
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

      const Vector4 vertex {0.5f * mesh_vertex.x - 1.5f,
                            0.5f * mesh_vertex.y - 1.5f,
                            0.5f * mesh_vertex.z + 1.5f,
                            1.0f};

      const Vector4 normal {mesh_normal.x, mesh_normal.y, mesh_normal.z, 0.0f};

      const Vector4 texcoord {0.25f * mesh_vertex.x,
                              -0.25f * mesh_vertex.z,
                              0.25f * mesh_vertex.y,
                              1.0f};

      element_array->push_back(CubeElement{texcoord,
                                           matrix * normal,
                                           matrix * vertex});
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
      ++trianglecount;
    }
  }
}

/*
 * Rotate the cube.  This takes care of all orientations possible.
 */
bool CubeTesselator::Impl::compute_rotations(int idx, Cube cube, Cube piece,
                                             Vector4 translation)
{
  static const Matrix4::array_type rotate90[3] =
  {
    { {1, 0,  0, 0}, { 0, 0, 1, 0}, {0, -1, 0, 0}, {0, 0, 0, 1} }, // 90 deg around x
    //{ {0, 0, -1, 0}, { 0, 1,  0, 0}, {1, 0, 0, 0}, {0, 0, 0, 1} }, // 90 deg around y
    { {0, 0, 1, 0}, { 0, 1,  0, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1} }, // 90 deg around y
    { {0, -1,  0, 0}, {1, 0,  0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} }  // 90 deg around z
  };

  Matrix4 matrix {Matrix4::identity[0], Matrix4::identity[1],
                 Matrix4::identity[2], translation};

  for (unsigned int i = 0;; ++i)
  {
    Cube temp = cube;

    // Add the 4 possible orientations of each cube side.
    for (int k = 0; k < 4; ++k)
    {
#if 0
      std::cout << "Cube content:\n";
      dump_cube(temp);
      std::cout << "Transform:\n";
      dump_transform(matrix, puzzle_piece_at_origin(idx));
#endif
      if (temp == piece)
      {
        load_object(idx, matrix);
        return true;
      }
#if 0
      std::cout << "Rotate Z\n";
#endif
      temp.rotate(Cube::AXIS_Z);
      matrix = rotate90[Cube::AXIS_Z] * matrix;
    }

    if (i == 5)
      break;

    // Due to the zigzagging performed here, only 5 rotations are
    // necessary to move each of the 6 cube sides in turn to the front.
#if 0
    std::cout << "Rotate " << ((i % 2 == 0) ? 'X' : 'Y') << '\n';
#endif
    cube.rotate(i % 2);
    matrix = rotate90[i % 2] * matrix;
  }

  return false;
}

void CubeTesselator::Impl::run(Cube piece)
{
  g_return_if_fail(element_array != 0);
  g_return_if_fail((range_start_array != 0) == (range_count_array != 0));
  g_return_if_fail((index_array != 0) != (range_start_array != 0));
  g_return_if_fail(range_start_array == 0 || range_start_array->size() == range_count_array->size());

  for (int idx = 0; idx < CUBE_PIECE_COUNT; ++idx)
  {
    Cube cube = puzzle_piece_at_origin(idx);

    // Make sure the piece is positioned where we expect it to be.
    g_return_if_fail(cube.get(0, 0, 0));

    Cube z, y, x;
    int xi, yi, zi;

    for (z = cube, zi = 0; z != Cube(); z.shift(Cube::AXIS_Z), ++zi)
      for (y = z, yi = 0; y != Cube(); y.shift(Cube::AXIS_Y), ++yi)
        for (x = y, xi = 0; x != Cube(); x.shift(Cube::AXIS_X), ++xi)
        {
          const Vector4 translation {xi * cellsize, yi * cellsize,
                                     -zi * cellsize, 1.0f};

          if (compute_rotations(idx, x, piece, translation))
            return;
        }
  }
}

} // namespace Somato
