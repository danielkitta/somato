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

#include <glib.h>
#include <cmath>
#include <algorithm>
#include <memory>

namespace
{

using Math::Matrix4;
using Math::Vector4;

typedef unsigned int EdgeType;
typedef unsigned int EdgeFlags;

class CubePosition
{
private:
  int pos_[3];

public:
  CubePosition()                    { pos_[0] = 0; pos_[1] = 0; pos_[2] = 0; }
  CubePosition(int x, int y, int z) { pos_[0] = x; pos_[1] = y; pos_[2] = z; }

  void assign(int x, int y, int z)  { pos_[0] = x; pos_[1] = y; pos_[2] = z; }

  CubePosition(const CubePosition& other)
    { assign(other.pos_[0], other.pos_[1], other.pos_[2]); }

  CubePosition& operator=(const CubePosition& other)
    { assign(other.pos_[0], other.pos_[1], other.pos_[2]); return *this; }

  int&       operator[](int i)       { return pos_[i]; }
  const int& operator[](int i) const { return pos_[i]; }
};

struct EdgePosition
{
  CubePosition  pos;
  EdgeType      type;

  EdgePosition() : pos (), type (0) {}
  EdgePosition(const CubePosition& pos_, EdgeType type_) : pos (pos_), type (type_) {}

  EdgePosition(const EdgePosition& other)
    : pos (other.pos), type (other.type) {}

  EdgePosition& operator=(const EdgePosition& other)
    { pos = other.pos; type = other.type; return *this; }
};

struct EdgeCorner;

#if SOMATO_USE_UNCHECKEDVECTOR
typedef Util::UncheckedVector<int>          IndexStore;
typedef Util::UncheckedVector<CubePosition> FaceStore;
typedef Util::UncheckedVector<EdgePosition> EdgeStore;
typedef Util::UncheckedVector<EdgeCorner*>  CornerStore;
#else
typedef std::vector<int>                    IndexStore;
typedef std::vector<CubePosition>           FaceStore;
typedef std::vector<EdgePosition>           EdgeStore;
typedef std::vector<EdgeCorner*>            CornerStore;
#endif

struct EdgeCorner
{
  Vector4     vertex;
  EdgeCorner* link[3];
  EdgeFlags   flags;

  static EdgeCorner* insert_unique(CornerStore& store, const Vector4& vertex, EdgeFlags flags);

  void add_link(EdgeCorner* node);
  void remove_link(EdgeCorner* node);

private:
  inline EdgeCorner(const Vector4& vertex_, EdgeFlags flags_);

  // noncopyable
  EdgeCorner(const EdgeCorner&);
  EdgeCorner& operator=(const EdgeCorner&);
};

template <class T>
class ScopeClearDynamic
{
private:
  T& cont_;

public:
  explicit ScopeClearDynamic(T& cont) : cont_ (cont) {}
  inline  ~ScopeClearDynamic();
};

enum
{
  EDGE_SLICES = 4,
  EDGE_SCALE  = 16  // denominator of the edge radius
};

static const float edgeradius = 1.0 / EDGE_SCALE;

static const EdgeType  ET_BOTTOM   = 0;
static const EdgeType  ET_RIGHT    = 1;
static const EdgeType  ET_TOP      = 2;
static const EdgeType  ET_LEFT     = 3;
static const EdgeType  ET_TURNCCW  = 1;
static const EdgeType  ET_TURNCW   = 3;      // same as -1 but without the risk of overflows
static const EdgeType  ET_WRAP     = 4;
static const EdgeType  ET_MASK     = 0x3;
static const EdgeType  ET_MIRROR   = 0x2;    // used with xor to select the opposite edge
static const EdgeType  ET_STAIR    = 1 << 2;
static const EdgeType  ET_INNER    = 1 << 3;
static const EdgeType  ET_CONTINUE = 1 << 4;

static const EdgeFlags EF_JOINY    = 1 << 0;
static const EdgeFlags EF_JOINZ    = 1 << 1;
static const EdgeFlags EF_JOINYZ   = EF_JOINY | EF_JOINZ;
static const EdgeFlags EF_BRIDGE   = 1 << 2;
static const EdgeFlags EF_EDGEEND  = 1 << 3;
static const EdgeFlags EF_CONTINUE = 1 << 4;

/*
 * I very much dislike using the preprocessor to define these constants.
 * Unfortunately GCC generates a (huge) runtime initializer for the static
 * arrays below if the numbers are properly defined as variables of type
 * const float or const double.  Interestingly enough, even so the compiler
 * never generates actual memory loads for the constants, but puts them as
 * immediate operands directly into the code.  Presumably the corresponding
 * optimization takes place only at the code generation stage later on, and
 * therefore does not take care of static array initializers.
 */
#define SINPI8 0.38268343236508977172845998 /* sin(PI / 8) */
#define SINPI4 0.70710678118654752440084436 /* sin(PI / 4) */
#define COSPI8 0.92387953251128675612818319 /* cos(PI / 8) */

/*
 * The subdivision is optimized so that the area of the center triangle
 * is equal to the area of each neighboring triangle.  Mathematically,
 * the problem boils down to solving the equation:
 *
 * 0 = 9*z*z*z - 2*sqrt(2)*z*z - 7*z + 2*sqrt(2)
 *
 * The term below is a mathematically exact solution of this polynom.
 *
 *     2                      1            4*401*sqrt(2)
 * z = -- * (sqrt(197) * cos[ - * arccos(- -------------) ] + sqrt(2))
 *     27                     3            197*sqrt(197)
 */
#define SUBDV1 0.42859318955462237500354246 /* sqrt((1 - z*z) / 2) */
#define SUBDV2 0.79537145770689500384661081 /* z */

/*
 * Precomputed magic numbers used to define arrays
 * of normals for the bridge joints between edges.
 */
#define NORMC1 0.0055069676901018
#define NORMC2 0.176659168404023
#define NORMC3 0.195523867907935
#define NORMC4 0.284590680520277
#define NORMC5 0.307795063149275
#define NORMC6 0.679113294636966
#define NORMC7 0.712458189135422
#define NORMC8 0.938498034968282
#define NORMC9 0.951436741148771

static
const Vector4::array_type edgedata[EDGE_SLICES + 1] =
{
  {    0.0,    0.0,    1.0,    0.0 },
  {    0.0, SINPI8, COSPI8,    0.0 },
  {    0.0, SINPI4, SINPI4,    0.0 },
  {    0.0, COSPI8, SINPI8,    0.0 },
  {    0.0,    1.0,    0.0,    0.0 }
};

static
const Vector4::array_type cornerdata[EDGE_SLICES][3 * EDGE_SLICES] =
{ 
  { // z-turn, slice 0
    { SINPI8,    0.0, COSPI8,    0.0 },
    {    0.0,    0.0,    1.0,    0.0 },
    {    0.0, SINPI8, COSPI8,    0.0 },
    // y-turn, slice 0
    {    1.0,    0.0,    0.0,    0.0 },
    { COSPI8, SINPI8,    0.0,    0.0 },
    { COSPI8,    0.0, SINPI8,    0.0 },
    { SUBDV2, SUBDV1, SUBDV1,    0.0 },
    { SINPI4,    0.0, SINPI4,    0.0 },
    { SUBDV1, SUBDV1, SUBDV2,    0.0 },
    { SINPI8,    0.0, COSPI8,    0.0 },
    {    0.0, SINPI8, COSPI8,    0.0 },
    {    0.0,    0.0,    1.0,    0.0 }
  },
  { // z-turn, slice 1
    { SINPI4,    0.0, SINPI4,    0.0 },
    { SINPI8,    0.0, COSPI8,    0.0 },
    { SUBDV1, SUBDV1, SUBDV2,    0.0 },
    {    0.0, SINPI8, COSPI8,    0.0 },
    {    0.0, SINPI4, SINPI4,    0.0 },
    // y-turn, slice 1
    { COSPI8, SINPI8,    0.0,    0.0 },
    { SINPI4, SINPI4,    0.0,    0.0 },
    { SUBDV2, SUBDV1, SUBDV1,    0.0 },
    { SUBDV1, SUBDV2, SUBDV1,    0.0 },
    { SUBDV1, SUBDV1, SUBDV2,    0.0 },
    {    0.0, SINPI4, SINPI4,    0.0 },
    {    0.0, SINPI8, COSPI8,    0.0 }
  },
  { // z-turn, slice 2
    { COSPI8,    0.0, SINPI8,    0.0 },
    { SINPI4,    0.0, SINPI4,    0.0 },
    { SUBDV2, SUBDV1, SUBDV1,    0.0 },
    { SUBDV1, SUBDV1, SUBDV2,    0.0 },
    { SUBDV1, SUBDV2, SUBDV1,    0.0 },
    {    0.0, SINPI4, SINPI4,    0.0 },
    {    0.0, COSPI8, SINPI8,    0.0 },
    // y-turn, slice 2
    { SINPI4, SINPI4,    0.0,    0.0 },
    { SINPI8, COSPI8,    0.0,    0.0 },
    { SUBDV1, SUBDV2, SUBDV1,    0.0 },
    {    0.0, COSPI8, SINPI8,    0.0 },
    {    0.0, SINPI4, SINPI4,    0.0 }
  },
  { // z-turn, slice 3
    {    1.0,    0.0,    0.0,    0.0 },
    { COSPI8,    0.0, SINPI8,    0.0 },
    { COSPI8, SINPI8,    0.0,    0.0 },
    { SUBDV2, SUBDV1, SUBDV1,    0.0 },
    { SINPI4, SINPI4,    0.0,    0.0 },
    { SUBDV1, SUBDV2, SUBDV1,    0.0 },
    { SINPI8, COSPI8,    0.0,    0.0 },
    {    0.0, COSPI8, SINPI8,    0.0 },
    {    0.0,    1.0,    0.0,    0.0 },
    // y-turn, slice 3
    { SINPI8, COSPI8,    0.0,    0.0 },
    {    0.0,    1.0,    0.0,    0.0 },
    {    0.0, COSPI8, SINPI8,    0.0 }
  }
};

static
const Vector4::array_type normaldata_bridge_y[2 * EDGE_SLICES + 2] =
{
  {    -1.0,     0.0,     0.0,     0.0 },
  {    -1.0,     0.0,     0.0,     0.0 },
  { -NORMC8,  NORMC4,  NORMC3,     0.0 },
  { -NORMC9,  NORMC5,  NORMC1,     0.0 },
  { -NORMC7,  NORMC6,  NORMC2,     0.0 },
  { -NORMC6,  NORMC7, -NORMC2,     0.0 },
  { -NORMC5,  NORMC9, -NORMC1,     0.0 },
  { -NORMC4,  NORMC8, -NORMC3,     0.0 },
  {     0.0,     1.0,     0.0,     0.0 },
  {     0.0,     1.0,     0.0,     0.0 }
};

static
const Vector4::array_type normaldata_bridge_z[2 * EDGE_SLICES + 2] =
{
  {     0.0,     0.0,     1.0,     0.0 },
  {     0.0,     0.0,     1.0,     0.0 },
  { -NORMC4,  NORMC3,  NORMC8,     0.0 },
  { -NORMC5,  NORMC1,  NORMC9,     0.0 },
  { -NORMC6,  NORMC2,  NORMC7,     0.0 },
  { -NORMC7, -NORMC2,  NORMC6,     0.0 },
  { -NORMC9, -NORMC1,  NORMC5,     0.0 },
  { -NORMC8, -NORMC3,  NORMC4,     0.0 },
  {    -1.0,     0.0,     0.0,     0.0 },
  {    -1.0,     0.0,     0.0,     0.0 }
};

/*
 * The offset from the back-bottom-left corner of the cube towards the origin.
 * Used for translation of vertices to texture coordinates, and also to align
 * cell edges at integer coordinates before rounding.
 */
static
const Vector4::array_type centeroffset =
{
  Somato::Cube::N / 2.0, Somato::Cube::N / 2.0, Somato::Cube::N / 2.0, 0.0
};

/*
 * We do not really want any texturing for the rounded edges and corners,
 * thus set the texture coordinate to the same value for all vertices.  At
 * least on my hardware, this is faster than repeatedly turning on and off
 * texturing, and adjusting the material parameters to match the average
 * texture luminance.
 */
static
const Vector4::array_type edgetexcoord = { 0.5 / EDGE_SCALE, 0.5 / EDGE_SCALE, 0.0, 0.0 };

static
const signed char clockwise_offset[4][2] = { {+1, +1}, {-1, +1}, {-1, -1}, {+1, -1} };
static
const signed char neighbor_offset [4][2] = { { 0, -1}, {+1,  0}, { 0, +1}, {-1,  0} };

inline
EdgeCorner::EdgeCorner(const Vector4& vertex_, EdgeFlags flags_)
:
  vertex  (vertex_),
  flags   (flags_)
{
  link[0] = 0;
  link[1] = 0;
  link[2] = 0;
}

// static
EdgeCorner* EdgeCorner::insert_unique(CornerStore& store, const Vector4& vertex, EdgeFlags flags)
{
  for (CornerStore::const_iterator p = store.begin(); p != store.end(); ++p)
  {
    // XXX
    if (((*p)->flags == 0 || (*p)->flags == EF_CONTINUE) && !(*p)->link[2] && (*p)->vertex == vertex)
    {
      if (flags == EF_CONTINUE) // XXX
        (*p)->flags |= EF_CONTINUE;
      else
        g_return_val_if_fail(flags == 0, 0);
      return *p;
    }
  }

  std::auto_ptr<EdgeCorner> node (new EdgeCorner(vertex, flags));

  store.push_back(node.get());

  return node.release();
}

void EdgeCorner::add_link(EdgeCorner* node)
{
  int index = 0;

  if (link[0])
    index = 1;
  if (link[1])
    index = 2;

  g_return_if_fail(link[2] == 0);

  link[index] = node;
}

void EdgeCorner::remove_link(EdgeCorner* node)
{
  if (link[2] != node)
  {
    if (link[1] != node)
    {
      g_return_if_fail(link[0] == node);

      link[0] = link[1];
    }
    link[1] = link[2];
  }
  link[2] = 0;
}

template <class T> inline
ScopeClearDynamic<T>::~ScopeClearDynamic()
{
  std::for_each(cont_.begin(), cont_.end(), Util::Delete<typename T::value_type>());
  cont_.clear();
}

static inline
bool is_surface_cell(const Somato::Cube& cube, int x, int y, int z)
{
  return (cube.getsafe(x, y, z) && !cube.getsafe(x, y, z - 1));
}

static
Vector4 adjust_joint(int i, EdgeFlags flags)
{
  Vector4 v (edgedata[i]);

  if ((flags & EF_JOINYZ) != 0 && (flags & EF_BRIDGE) == 0)
  {
    if ((flags & EF_EDGEEND) == 0)
    {
      if ((flags & EF_JOINY) != 0 && ((flags & EF_JOINZ) == 0 || v.y() > v.z()))
        v[0] = -v.y();
      else
        v[0] = -v.z();
    }
    else
    {
      if ((flags & EF_JOINZ) != 0 && ((flags & EF_JOINY) == 0 || v.y() > v.z()))
        v[0] = v.y();
      else
        v[0] = v.z();
    }
  }

  return v;
}

static
Matrix4 compute_edge_rotation(const Vector4& a, const Vector4& b)
{
  // First, determine the edge's horizontal direction.
  const Vector4 dx = Vector4::sign(a - b);

  // Figure out the spatial orientation by extracting
  // the sign of the edge's radius offset.
  const Vector4 t = a + centeroffset;
  const Vector4 r = Vector4::mask_ifnonzero(Vector4::sign(Vector4::rint(t) - t), dx);

  const Vector4 cross = r % dx;

  const Vector4 dy = Vector4::sign(r + cross);
  const Vector4 dz = Vector4::sign(r - cross);

  return Matrix4(dx, dy, dz, Vector4(Matrix4::identity[3]));
}

} // anonymous namespace

namespace Somato
{

class CubeTesselator::Impl
{
private:
  Matrix4     matrix_;
  EdgeStore   edgestore_;
  CornerStore cornerstore_;
  EdgeCorner* lastcorner_;
  Cube        piece_;
  Cube        edgesdone_;
  int         first_element_;
  int         strip_index_;

  void begin_strip();
  void end_strip();
  void strip_element(const CubeElement& element);

  void trace_contour(const CubePosition& start);
  bool is_edge(const CubePosition& pos, EdgeType type) const;
  EdgeType get_inner_flag(const CubePosition& pos, EdgeType type) const;
  EdgeType find_edge_break(CubePosition& pos, EdgeType type) const;
  EdgeType find_adjacent_edge(CubePosition& pos, EdgeType type) const;

  void build_plane();
  void build_polygon(const CubePosition& start);

  bool find_top_vertex_pair(const CubePosition& pos, IndexStore& indices) const;
  bool find_left_vertex_pair(const CubePosition& pos, IndexStore& indices) const;
#if 0
  void build_quad_contour();
  void build_hexagon_contour();
  void build_octagon_contour();
  void build_contour_strip(const FaceStore& vertices,
                           const unsigned char* order, int count, int offset);
  void build_edge_stripe2(const Matrix4& rotation, const EdgeCorner& ca, const EdgeCorner& cb);
#endif
  void build_contour_strip2(const FaceStore& vertices, const IndexStore& indices);
  void compute_contour_vertices(FaceStore& vertices, IndexStore& inward);
  void check_new_edge(const CubePosition& pos, const CubePosition& ivertex,
                      EdgeType type, EdgeType prevtype);
  Vector4 vertex_at_corner(const CubePosition& vpos) const;
  EdgeFlags get_join_type_start(const CubePosition& pos, EdgeType type) const;
  EdgeFlags get_join_type_end(const CubePosition& pos, EdgeType type) const;

  void trace_connected_edges();
  void build_single_edge(const Matrix4& rotation, const EdgeCorner& ca, const EdgeCorner& cb);
  void build_edge_stripe(const Matrix4& rotation, const CornerStore& stripe);
  void build_corner_slice(const Vector4& origin,
                          const Vector4::array_type* slice, int begin, int end);
  void terminate_edge_slice(const Vector4& origin, EdgeFlags flags, int a, int b);
  void build_bridge(const Matrix4& rotation,
                    const Vector4& origin, EdgeFlags flags);
  void build_bridge_strip(const Matrix4& matrixa, const Matrix4& matrixb,
                          const Vector4& a, const Vector4& b,
                          const Vector4::array_type* normals);
  // noncopyable
  Impl(const CubeTesselator::Impl&);
  CubeTesselator::Impl& operator=(const CubeTesselator::Impl&);

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
  matrix_           (),
  edgestore_        (),
  cornerstore_      (),
  lastcorner_       (0),
  piece_            (),
  edgesdone_        (),
  first_element_    (0),
  strip_index_      (0),
  element_array     (0),
  range_start_array (0),
  range_count_array (0),
  index_array       (0),
  cellsize          (1.0),
  trianglecount     (0)
{}

CubeTesselator::Impl::~Impl()
{}

void CubeTesselator::Impl::run(Cube piece)
{
  static const Matrix4::array_type rotate90[2] =
  {
    { {1, 0,  0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1} },  // 90 deg around x
    { {0, 0, -1, 0}, {0, 1,  0, 0}, {1, 0, 0, 0}, {0, 0, 0, 1} }   // 90 deg around y
  };

  g_return_if_fail(element_array != 0);
  g_return_if_fail((range_start_array != 0) == (range_count_array != 0));
  g_return_if_fail((index_array != 0) != (range_start_array != 0));
  g_return_if_fail(range_start_array == 0 || range_start_array->size() == range_count_array->size());

  piece_         = piece;
  first_element_ = element_array->size();
  matrix_        = Matrix4::identity;
  lastcorner_    = 0;

  ScopeClearDynamic<CornerStore> cornerscope (cornerstore_);

  for (unsigned int i = 0;; ++i)
  {
    build_plane();

    if (i == 5)
      break;

    piece_.rotate(i % 2);
    matrix_ *= rotate90[i % 2];
  }

  trace_connected_edges();
}

void CubeTesselator::Impl::begin_strip()
{
  strip_index_ = 0;
}

void CubeTesselator::Impl::end_strip()
{
  if (strip_index_ > 0)
  {
    if (!index_array)
    {
      const int strip_end = element_array->size();

      g_return_if_fail(strip_end >= strip_index_);

      range_start_array->push_back(strip_end - strip_index_);
      range_count_array->push_back(strip_index_);
    }

    if (strip_index_ > 2)
      trianglecount += strip_index_ - 2;
  }
}

void CubeTesselator::Impl::strip_element(const CubeElement& element)
{
  if (index_array)
  {
#if 0
    // It gets a bit tricky with reverse iterators, but after all the vertex
    // we look for is most likely to be found among the more recent additions.

    const CubeElementArray::reverse_iterator p =
        std::find(element_array->rbegin(), element_array->rend() - first_element_, element);

    int element_index = (element_array->rend() - p) - 1;

    if (element_index < first_element_)
    {
      element_index = element_array->size();
      element_array->push_back(element);
    }
#else
    // Not using reverse iterators because it generated inefficient code.

    const CubeElementArray::const_iterator pbegin = element_array->begin();
    const CubeElementArray::const_iterator pend   = element_array->end();

    const CubeElementArray::const_iterator p = std::find(pbegin + first_element_, pend, element);

    const int element_index = p - pbegin;

    if (p == pend)
      element_array->push_back(element);
#endif

    if (strip_index_ > 2)
    {
      const CubeIndexArray::const_iterator pos = index_array->end();

      const int i0 = (strip_index_ % 2 != 0) ? pos[-1] : pos[-3];
      const int i1 = (strip_index_ % 2 != 0) ? pos[-2] : pos[-1];

      index_array->push_back(i0);
      index_array->push_back(i1);
    }

    index_array->push_back(element_index);
  }
  else
  {
    element_array->push_back(element);
  }

  ++strip_index_;
}

/*
 * Move along the boundaries of the contour in counterclockwise direction,
 * and define a vertex at the start coordinates of each new edge.  The cube
 * surface tile at start should be positioned at the bottom-left corner of
 * the contour.
 */
void CubeTesselator::Impl::trace_contour(const CubePosition& start)
{
  EdgePosition edge (start, get_inner_flag(start, ET_BOTTOM));

  do
  {
    edgesdone_.put(edge.pos[0], edge.pos[1], edge.pos[2], true);
    edgestore_.push_back(edge);

    edge.type = find_edge_break(edge.pos, edge.type);
  }
  while ((edge.type & ET_MASK) != ET_BOTTOM || edge.pos[0] != start[0] || edge.pos[1] != start[1]);
}

bool CubeTesselator::Impl::is_edge(const CubePosition& pos, EdgeType type) const
{
  return (is_surface_cell(piece_, pos[0], pos[1], pos[2])
          && !is_surface_cell(piece_, pos[0] + neighbor_offset[type & ET_MASK][0],
                                      pos[1] + neighbor_offset[type & ET_MASK][1],
                                      pos[2]));
}

EdgeType CubeTesselator::Impl::get_inner_flag(const CubePosition& pos, EdgeType type) const
{
  if (piece_.getsafe(pos[0] + neighbor_offset[type & ET_MASK][0],
                     pos[1] + neighbor_offset[type & ET_MASK][1],
                     pos[2] - 1))
    return type |  ET_INNER;
  else
    return type & ~ET_INNER;
}

EdgeType CubeTesselator::Impl::find_edge_break(CubePosition& pos, EdgeType type) const
{
  static const signed char nextoffset[4][2] = { {+1, 0}, {0, +1}, {-1, 0}, {0, -1} };

  if ((type & ET_STAIR) != 0)
  {
    EdgeType result = (type + ET_TURNCW) % ET_WRAP;

    if (!is_edge(pos, result))
    {
      result ^= ET_MIRROR;
      pos[0] -= clockwise_offset[type & ET_MASK][0];
      pos[1] -= clockwise_offset[type & ET_MASK][1];
    }

    pos[0] += nextoffset[result][0];
    pos[1] += nextoffset[result][1];

    return get_inner_flag(pos, result);
  }
  else
  {
    const CubePosition next (pos[0] + nextoffset[type & ET_MASK][0],
                             pos[1] + nextoffset[type & ET_MASK][1],
                             pos[2]);

    if (!is_edge(next, type))
    {
      return find_adjacent_edge(pos, type);
    }
    else if (type == get_inner_flag(next, type))
    {
      pos[0] = next[0];
      pos[1] = next[1];

      return type | ET_CONTINUE;
    }
    else // inner/outer flip
    {
      // The 'stair step' within inner and outer subdivisions of what was
      // originally a single polygon edge is represented as a special edge
      // of length 0.  Edges that have the etStair flag are always treated
      // as inner edges, thus there's no need to (re)set etInner.
      if ((type & ET_INNER) != 0)
      {
        return ((type + ET_TURNCCW) % ET_WRAP) | ET_STAIR;
      }
      else
      {
        const EdgeType result = (type + ET_TURNCW) % ET_WRAP;

        pos[0] += clockwise_offset[result][0];
        pos[1] += clockwise_offset[result][1];

        return result | ET_STAIR;
      }
    }
  }
}

EdgeType CubeTesselator::Impl::find_adjacent_edge(CubePosition& pos, EdgeType type) const
{
  EdgeType result = (type + ET_TURNCCW) % ET_WRAP;

  if (!is_edge(pos, result))
  {
    result ^= ET_MIRROR;
    pos[0] += clockwise_offset[result][0];
    pos[1] += clockwise_offset[result][1];

    g_return_val_if_fail(is_edge(pos, result), result);
  }

  return get_inner_flag(pos, result);
}

void CubeTesselator::Impl::build_plane()
{
  edgesdone_ = Cube();

  for (int z = 0; z < Cube::N; ++z)
    for (int y = 0; y < Cube::N; ++y)
      for (int x = 0; x < Cube::N; ++x)
      {
        if (!edgesdone_.get(x, y, z))
        {
          const CubePosition pos (x, y, z);

          if (is_edge(pos, ET_LEFT) && is_edge(pos, ET_BOTTOM))
            build_polygon(pos);
        }
      }
}

/*
 * Build a new polygon defined by a single contour.
 */
void CubeTesselator::Impl::build_polygon(const CubePosition& start)
{
  edgestore_.clear();

  trace_contour(start);

#if 0
  FaceStore   vertices;
  IndexStore  inward;

  compute_contour_vertices(vertices, inward);

  switch (count)
  {
    case 4: build_quad_contour();    break;
    case 6: build_hexagon_contour(); break;
    case 8: build_octagon_contour(); break;
    default:
      g_return_if_reached();
  }
#endif

  int stair_count = 0;

  for (EdgeStore::const_iterator p = edgestore_.begin(); p != edgestore_.end(); ++p)
  {
    stair_count += ((p->type & ET_STAIR) != 0);
  }

  g_return_if_fail(stair_count <= 1);

  FaceStore   vertices;
  IndexStore  inward;

  compute_contour_vertices(vertices, inward);

  int min_x = G_MAXINT;
  int max_x = G_MININT;
  int min_y = G_MAXINT;
  int max_y = G_MININT;

  for (unsigned int i = 0; i < vertices.size(); ++i)
  {
    const int x = vertices[i][0];
    const int y = vertices[i][1];

    if (x < min_x)
      min_x = x;
    if (x > max_x)
      max_x = x;
    if (y < min_y)
      min_y = y;
    if (y > max_y)
      max_y = y;
  }

  if ((max_x - min_x > max_y - min_y) != (stair_count != 0))
  {
    for (unsigned int i = 0; i + 1 < edgestore_.size(); ++i)
    {
      if ((edgestore_[i].type & (ET_MASK | ET_STAIR)) == ET_RIGHT)
      {
        CubePosition pos = edgestore_[i].pos;

        IndexStore indices;

        bool foo = false;
        if (i + 1 < edgestore_.size()
            && (edgestore_[i + 1].type & (ET_MASK | ET_STAIR)) == (ET_TOP | ET_STAIR))
        {
          indices.push_back(i + 1);
          indices.push_back((i + 2 < edgestore_.size()) ? i + 2 : 0);
          indices.push_back(i);
          foo = true;
        }
        else
        {
          indices.push_back(i);
          indices.push_back(i + 1);
        }

        if (i > 0 && (edgestore_[i - 1].type & (ET_MASK | ET_STAIR)) == (ET_BOTTOM | ET_STAIR))
        {
          indices.push_back(i - 1);
          foo = true;
        }

        while (find_left_vertex_pair(pos, indices))
          --pos[0];

        if (foo)
        {
          g_return_if_fail(indices.size() == 5);

          std::swap(indices[3], indices[4]);
        }
#if 1
        if (!foo && indices.back() + 1 < int(edgestore_.size())
            && ((edgestore_[indices.back()].type & (ET_MASK | ET_STAIR)) == (ET_TOP | ET_STAIR)
                || (edgestore_[indices.back() + 1].type & (ET_MASK | ET_STAIR)) == (ET_BOTTOM | ET_STAIR)))
        {
          g_return_if_fail(indices.size() == 4);

          indices.push_back(indices.back() + 1);
        }
#endif
        build_contour_strip2(vertices, indices);
      }
    }
  }
  else // vertical action
  {
    for (unsigned int i = 0; i + 1 < edgestore_.size(); ++i)
    {
      if ((edgestore_[i].type & (ET_MASK | ET_STAIR)) == ET_BOTTOM)
      {
        CubePosition pos = edgestore_[i].pos;

        IndexStore indices;

        bool foo = false;
        if (i + 1 < edgestore_.size()
            && (edgestore_[i + 1].type & (ET_MASK | ET_STAIR)) == (ET_RIGHT | ET_STAIR))
        {
          indices.push_back(i + 1);
          indices.push_back((i + 2 < edgestore_.size()) ? i + 2 : 0);
          indices.push_back(i);
          foo = true;
        }
        else
        {
          indices.push_back(i);
          indices.push_back(i + 1);
        }

        if ((((i > 0) ? edgestore_[i - 1].type : edgestore_.back().type)
             & (ET_MASK | ET_STAIR)) == (ET_LEFT | ET_STAIR))
        {
          indices.push_back(i - 1);
          foo = true;
        }

        while (find_top_vertex_pair(pos, indices))
          ++pos[1];

        if (foo)
        {
          g_return_if_fail(indices.size() == 5);

          std::swap(indices[3], indices[4]);
        }
#if 1
        if (!foo && indices.back() + 1 < int(edgestore_.size())
            && ((edgestore_[indices.back()].type & (ET_MASK | ET_STAIR)) == (ET_RIGHT | ET_STAIR)
                || (edgestore_[indices.back() + 1].type & (ET_MASK | ET_STAIR)) == (ET_LEFT | ET_STAIR)))
        {
          g_return_if_fail(indices.size() == 4);

          indices.push_back(indices.back() + 1);
        }
#endif
        build_contour_strip2(vertices, indices);
      }
    }
  }
}

bool CubeTesselator::Impl::find_top_vertex_pair(const CubePosition& pos, IndexStore& indices) const
{
  static const signed char d_left [4][2] = { {0, 1}, {-1, 1}, {-1, 0}, {0, 0} };
  static const signed char d_right[4][2] = { {1, 1}, { 0, 1}, { 0, 0}, {1, 0} };

  int left  = -1;
  int right = -1;

  for (EdgeStore::const_iterator p = edgestore_.begin(); p != edgestore_.end(); ++p)
  {
    const EdgeType type = p->type & ET_MASK;

    const int rx = pos[0] + d_right[type][0];
    const int ry = pos[1] + d_right[type][1];

    if ((type == ET_RIGHT || (p->type & ET_STAIR) == 0) && p->pos[0] == rx && p->pos[1] == ry)
    {
//      g_return_val_if_fail(right < 0, false);
      if (right < 0)
        right = p - edgestore_.begin();
    }

    const int lx = pos[0] + d_left[type][0];
    const int ly = pos[1] + d_left[type][1];

    if ((type == ET_RIGHT || (p->type & ET_STAIR) == 0) && p->pos[0] == lx && p->pos[1] == ly)
    {
//      g_return_val_if_fail(left < 0, false);
      if (left < 0)
        left = p - edgestore_.begin();
    }
  }

  if (left >= 0 && right >= 0)
  {
    indices.push_back(left);
    indices.push_back(right);

    return true;
  }

//  g_return_val_if_fail(left < 0 && right < 0, false);

  return false;
}

bool CubeTesselator::Impl::find_left_vertex_pair(const CubePosition& pos, IndexStore& indices) const
{
  static const signed char d_bottom[4][2] = { {0, 0}, {-1, 0}, {-1, -1}, {0, -1} };
  static const signed char d_top   [4][2] = { {0, 1}, {-1, 1}, {-1,  0}, {0,  0} };

  int bottom = -1;
  int top    = -1;

  for (EdgeStore::const_iterator p = edgestore_.begin(); p != edgestore_.end(); ++p)
  {
    const EdgeType type = p->type & ET_MASK;
#ifdef _MSC_VER
    __assume(type <= ET_MASK); // code analysis needs help
#endif
    const int tx = pos[0] + d_top[type][0];
    const int ty = pos[1] + d_top[type][1];

    if ((type == ET_TOP || (p->type & ET_STAIR) == 0) && p->pos[0] == tx && p->pos[1] == ty)
    {
//      g_return_val_if_fail(top < 0, false);
      if (top < 0)
        top = p - edgestore_.begin();
    }

    const int bx = pos[0] + d_bottom[type][0];
    const int by = pos[1] + d_bottom[type][1];

    if ((type == ET_TOP || (p->type & ET_STAIR) == 0) && p->pos[0] == bx && p->pos[1] == by)
    {
//      g_return_val_if_fail(bottom < 0, false);
      if (bottom < 0)
        bottom = p - edgestore_.begin();
    }
  }

  if (bottom >= 0 && top >= 0)
  {
    indices.push_back(bottom);
    indices.push_back(top);

    return true;
  }

//  g_return_val_if_fail(left < 0 && right < 0, false);

  return false;
}

void CubeTesselator::Impl::build_contour_strip2(const FaceStore& vertices,
                                                const IndexStore& indices)
{
  const int count = indices.size();

//  g_return_if_fail(count >= 4 && count % 2 == 0);
  g_return_if_fail(count >= 4);

  const Vector4 normal = matrix_ * edgedata[0];

  {
    begin_strip();

    for (int i = 0; i < count; ++i)
    {
      const int k = indices[i];

      g_return_if_fail(k >= 0 && unsigned(k) < vertices.size());

      Vector4 vector (vertices[k][0], vertices[k][1], vertices[k][2]);

      vector *= edgeradius;

      const Vector4 texcoord = vector + centeroffset;
      const Vector4 vertex   = (matrix_ * vector) * cellsize;

      strip_element(CubeElement(texcoord, normal, vertex));
    }

    end_strip();
  }
}

#if 0
void CubeTesselator::Impl::build_quad_contour()
{
  static const unsigned char indices[4] = { 1, 2, 0, 3 };

  FaceStore   vertices;
  IndexStore  inward;

  compute_contour_vertices(vertices, inward);

  g_return_if_fail(inward.empty());

  build_contour_strip(vertices, indices, G_N_ELEMENTS(indices), 0);
}

void CubeTesselator::Impl::build_hexagon_contour()
{
  static const unsigned char indices[6] = { 1, 2, 0, 3, 5, 4 };

  FaceStore   vertices;
  IndexStore  inward;

  compute_contour_vertices(vertices, inward);

  g_return_if_fail(inward.size() == 1);

  build_contour_strip(vertices, indices, G_N_ELEMENTS(indices), inward[0]);
}

void CubeTesselator::Impl::build_octagon_contour()
{
  static const unsigned char indicesS1[8] = { 2, 3, 1, 4, 0, 5, 7, 6 };
  static const unsigned char indicesS2[8] = { 1, 2, 0, 3, 7, 4, 6, 5 };
  static const unsigned char indicesT1[6] = { 1, 2, 0, 3, 5, 4 };
  static const unsigned char indicesT2[4] = { 5, 6, 0, 7 };

  FaceStore   vertices;
  IndexStore  inward;

  compute_contour_vertices(vertices, inward);

  g_return_if_fail(inward.size() == 2);

  switch (inward[1] - inward[0])
  {
    case 4:
    {
      // Good: Not the darn T shaped one.  But we have to figure out
      // which one of the two possible mirrored S shapes we got here.
      const int i = inward[0];
      const bool d1x = vertices[i][0] < vertices[i + 2][0];
      const bool d1y = vertices[i][1] < vertices[i + 2][1];
      const bool d2x = vertices[i][0] < vertices[i + 4][0];
      const bool d2y = vertices[i][1] < vertices[i + 4][1];

      if (d1x == d2x && d1y == d2y)
        build_contour_strip(vertices, indicesS1, G_N_ELEMENTS(indicesS1), i);
      else
        build_contour_strip(vertices, indicesS2, G_N_ELEMENTS(indicesS2), i);
      break;
    }
    case 3:
      build_contour_strip(vertices, indicesT1, G_N_ELEMENTS(indicesT1), inward[1]);
      build_contour_strip(vertices, indicesT2, G_N_ELEMENTS(indicesT2), inward[1]);
      break;

    case 5:
      build_contour_strip(vertices, indicesT1, G_N_ELEMENTS(indicesT1), inward[0]);
      build_contour_strip(vertices, indicesT2, G_N_ELEMENTS(indicesT2), inward[0]);
      break;

    default:
      g_return_if_reached();
  }
}
#endif

void CubeTesselator::Impl::compute_contour_vertices(FaceStore& vertices, IndexStore& inward)
{
  enum { ORIGIN = Cube::N * EDGE_SCALE / 2 };
  static const signed char corneroffset[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

  vertices.reserve(edgestore_.size());

  EdgeType prevtype = edgestore_.back().type;

  for (EdgeStore::iterator p = edgestore_.begin(); p != edgestore_.end(); ++p)
  {
    int x = EDGE_SCALE * (p->pos[0] + corneroffset[p->type & ET_MASK][0]) - ORIGIN;
    int y = EDGE_SCALE * (p->pos[1] + corneroffset[p->type & ET_MASK][1]) - ORIGIN;

    if ((p->type & ET_CONTINUE) == 0)
    {
      if ((p->type & (ET_INNER | ET_STAIR)) == 0
          || ((p->type & ET_STAIR) != 0 && (prevtype & ET_INNER) != 0))
      {
        x -= neighbor_offset[p->type & ET_MASK][0];
        y -= neighbor_offset[p->type & ET_MASK][1];
      }
    }

    if ((prevtype & (ET_INNER | ET_STAIR)) == 0
        || ((prevtype & ET_STAIR) != 0 && (p->type & ET_INNER) != 0))
    {
      x -= neighbor_offset[prevtype & ET_MASK][0];
      y -= neighbor_offset[prevtype & ET_MASK][1];
    }

    vertices.push_back(CubePosition(x, y, ORIGIN - EDGE_SCALE * p->pos[2]));

    check_new_edge(p->pos, vertices.back(), p->type, prevtype);

    if ((p->type & ET_MASK) == ((prevtype + ET_TURNCW) & ET_MASK))
      inward.push_back(p - edgestore_.begin());

    prevtype = p->type;
  }
}

#if 0
void CubeTesselator::Impl::build_contour_strip(const FaceStore& vertices,
                                               const unsigned char* order, int count, int offset)
{
  const int n_vertices = vertices.size();

  g_return_if_fail(offset >= 0 && offset < n_vertices);

  const Vector4 normal = matrix_ * edgedata[0];

  {
    begin_strip();

    for (int i = 0; i < count; ++i)
    {
      int k = order[i] + offset;

      if (k >= n_vertices)
        k -= n_vertices;

      Vector4 vector (vertices[k][0], vertices[k][1], vertices[k][2]);

      vector *= edgeradius;

      const Vector4 texcoord = vector + centeroffset;
      const Vector4 vertex   = (matrix_ * vector) * cellsize;

      strip_element(CubeElement(texcoord, normal, vertex));
    }

    end_strip();
  }
}
#endif

void CubeTesselator::Impl::check_new_edge(const CubePosition& pos, const CubePosition& ivertex,
                                          EdgeType type, EdgeType prevtype)
{
  if ((prevtype & ~ET_CONTINUE) == ET_TOP || (prevtype & ~ET_CONTINUE) == ET_RIGHT)
  {
    const EdgeFlags flags = get_join_type_end(pos, type) | ((type & ET_CONTINUE) ? EF_CONTINUE : 0);

    CubePosition vpos (ivertex[0], ivertex[1], ivertex[2] - 1);

    if ((flags & EF_BRIDGE) == 0)
      switch (type & ~ET_CONTINUE)
      {
        case ET_LEFT | ET_INNER:  --vpos[0]; break;
        case ET_TOP  | ET_INNER:  ++vpos[1]; break;
      }

    g_return_if_fail(lastcorner_ != 0);
    EdgeCorner *const cb = EdgeCorner::insert_unique(cornerstore_, vertex_at_corner(vpos), flags);
    EdgeCorner *const ca = lastcorner_;
    lastcorner_ = 0;

    ca->add_link(cb);
    cb->add_link(ca);
  }

  if ((type & ~ET_CONTINUE) == ET_TOP || (type & ~ET_CONTINUE) == ET_RIGHT)
  {
    const EdgeFlags flags = get_join_type_start(pos, type) | ((type & ET_CONTINUE) ? EF_CONTINUE : 0);

    CubePosition vpos (ivertex[0], ivertex[1], ivertex[2] - 1);

    if ((flags & EF_BRIDGE) == 0)
      switch (prevtype & ~ET_CONTINUE)
      {
        case ET_RIGHT  | ET_INNER:  ++vpos[0]; break;
        case ET_BOTTOM | ET_INNER:  --vpos[1]; break;
      }

    g_return_if_fail(lastcorner_ == 0);
    lastcorner_ = EdgeCorner::insert_unique(cornerstore_, vertex_at_corner(vpos), flags);
  }
}

Vector4 CubeTesselator::Impl::vertex_at_corner(const CubePosition& vpos) const
{
  const Vector4 vector (vpos[0], vpos[1], vpos[2]);

  return matrix_ * (vector * edgeradius);
}

EdgeFlags CubeTesselator::Impl::get_join_type_start(const CubePosition& pos, EdgeType type) const
{
  EdgeFlags result = 0;

  switch (type)
  {
    case ET_RIGHT:
      if (piece_.getsafe(pos[0] + 1, pos[1] - 1, pos[2]))
        result |= EF_JOINY;
      if (piece_.getsafe(pos[0], pos[1] - 1, pos[2] - 1))
        result |= EF_JOINZ;
      if (piece_.getsafe(pos[0] + 1, pos[1] - 1, pos[2] - 1))
        result ^= EF_JOINYZ | EF_BRIDGE;
      break;

    case ET_TOP:
      if (piece_.getsafe(pos[0] + 1, pos[1] + 1, pos[2]))
        result |= EF_JOINY;
      if (piece_.getsafe(pos[0] + 1, pos[1], pos[2] - 1))
        result |= EF_JOINZ;
      if (piece_.getsafe(pos[0] + 1, pos[1] + 1, pos[2] - 1))
        result ^= EF_JOINYZ | EF_BRIDGE;
      break;

    default:
      break;
  }

  return result;
}

EdgeFlags CubeTesselator::Impl::get_join_type_end(const CubePosition& pos, EdgeType type) const
{
  EdgeFlags result = 0;

  switch (type)
  {
    case ET_RIGHT:  // previous was ET_TOP
    case ET_BOTTOM: // previous was ET_RIGHT
      result |= EF_JOINZ;
      break;

    case ET_RIGHT  | ET_STAIR: // previous was ET_TOP
    case ET_BOTTOM | ET_STAIR: // previous was ET_RIGHT
      result |= EF_JOINY | EF_BRIDGE;
      break;

    case ET_LEFT | ET_INNER: // previous was etTop
      result |= EF_JOINY;
      if (piece_.getsafe(pos[0] - 1, pos[1] + 1, pos[2]))
        result |= EF_JOINZ;
      if (piece_.getsafe(pos[0] - 1, pos[1] + 1, pos[2] - 1))
        result ^= EF_JOINYZ | EF_BRIDGE;
      break;

    case ET_TOP | ET_INNER: // previous was etRight
      result |= EF_JOINY;
      if (piece_.getsafe(pos[0] + 1, pos[1] + 1, pos[2]))
        result |= EF_JOINZ;
      if (piece_.getsafe(pos[0] + 1, pos[1] + 1, pos[2] - 1))
        result ^= EF_JOINYZ | EF_BRIDGE;
      break;

    default:
      break;
  }

  return result;
}

#if 1
void CubeTesselator::Impl::trace_connected_edges()
{
#if 1
  for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
  {
    const EdgeCorner& c = **p;

    // Make sure that each corner is either a real one that links to all three
    // neighbors, or a sharp end of an edge with exactly one neighbor link.

    g_return_if_fail(c.link[0] != 0
                     && (((c.flags & EF_CONTINUE) == 0 && (c.link[2] != 0 || c.link[1] == 0))
                         || ((c.flags & EF_CONTINUE) != 0 && c.link[2] == 0 && c.link[1] != 0)));
  }
#endif

  CornerStore stripe;
  stripe.reserve(cornerstore_.size());

  for (;;)
  {
    EdgeCorner* cc = 0;
    EdgeCorner* ca = 0;

    for (CornerStore::reverse_iterator p = cornerstore_.rbegin(); p != cornerstore_.rend(); ++p)
      if ((*p)->link[0] && !(*p)->link[1])
      {
        cc = *p;

        if ((cc->flags & EF_CONTINUE) == 0) // XXX
        {
          ca = cc;
          break;
        }
      }

    if (!ca)
      ca = cc;
    if (!ca)
      break;

    EdgeCorner* cb = ca->link[0];

    const Matrix4 rotation = compute_edge_rotation(ca->vertex, cb->vertex);

    EdgeFlags flags = ca->flags;

    if ((flags & EF_BRIDGE) != 0)
    {
      ca->flags = 0;
      build_bridge(rotation, ca->vertex, flags);
    }

    stripe.clear();

    stripe.push_back(ca);
    stripe.push_back(cb);

    unsigned int n = 0;

    ca->remove_link(cb);
    cb->remove_link(ca);

//    while (cb->link[0] && (cb->link[1] || (cb->flags & EF_CONTINUE) != 0))
    while (cb->link[0] && (cb->link[1])) // XXX
    {
      if (cb->link[1])
      {
        g_return_if_fail((cb->flags & EF_CONTINUE) == 0);

        const Vector4 d0 = Vector4::sign(ca->vertex - cb->vertex);
        const Vector4 d1 = Vector4::sign(cb->vertex - cb->link[0]->vertex);
        const Vector4 d2 = Vector4::sign(cb->vertex - cb->link[1]->vertex);

        ca = cb;
        cb = ca->link[(n % 2) ^ unsigned(d1 % d2 == d0)];
        ++n;
      }
      else
      {
        g_return_if_fail((cb->flags & EF_CONTINUE) != 0);

        ca = cb;
        cb = ca->link[0];
      }

      stripe.push_back(cb);

      ca->remove_link(cb);
      cb->remove_link(ca);
    }

#if 0
    if (stripe.size() > 2)
      build_edge_stripe(rotation, stripe);
    else
      build_single_edge(rotation, *ca, *cb);
#else
    if (n > 0)
      build_edge_stripe(rotation, stripe);
    else
    {
      for (CornerStore::iterator p = stripe.begin(); p != stripe.end() - 1; ++p)
        build_single_edge(rotation, **p, **(p + 1));
    }
#endif

    flags = cb->flags;

    if ((flags & EF_BRIDGE) != 0)
    {
      cb->flags = 0;
      build_bridge(compute_edge_rotation(cb->vertex, ca->vertex), cb->vertex, flags);
    }
  }

  // Final sanity check
  for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
  {
    g_return_if_fail((*p)->link[0] == 0);
  }
}
#else
void CubeTesselator::Impl::trace_connected_edges()
{
#if 1
  for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
  {
    const EdgeCorner& c = **p;

    // Make sure that each corner is either a real one that links to all three
    // neighbors, or a sharp end of an edge with exactly one neighbor link.

    g_return_if_fail(c.link[0] != 0
                     && (((c.flags & EF_CONTINUE) == 0 && (c.link[2] != 0 || c.link[1] == 0))
                         || ((c.flags & EF_CONTINUE) != 0 && c.link[2] == 0 && c.link[1] != 0)));
  }
#endif

#if 0
  CornerStore stripe;
  stripe.reserve(cornerstore_.size());
#endif

  for (;;)
  {
    EdgeCorner* ca = 0;
    EdgeCorner* cb = 0;

    for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
    {
      EdgeCorner* c = *p;

      if (c->flags == 0 && c->link[2])
      {
        for (int i = 0; i < 3; ++i)
          if (c->link[i] && c->link[i]->flags == 0 && c->link[i]->link[2])
          {
            ca = c;
            cb = c->link[i];
            break;
          }

        if (cb)
          break;
      }
      else if (!c->link[1] && c->link[0] && !c->link[0]->link[1])
      {
        ca = c;
        cb = c->link[0];
      }
    }

    if (!cb)
      break;

    const Matrix4 rotation = compute_edge_rotation(ca->vertex, cb->vertex);

    ca->remove_link(cb);
    cb->remove_link(ca);

    if (ca->link[1] && cb->link[1])
      build_edge_stripe2(rotation, *ca, *cb);
    else
      build_single_edge(rotation, *ca, *cb);
  }

  for (;;)
  {
    EdgeCorner* ca = 0;
    EdgeCorner* cb = 0;

    for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
    {
      EdgeCorner* c = *p;

      if (c->link[0])
      {
        ca = c;
        cb = c->link[0];
        break;
      }
    }

    if (!cb)
      break;

    const Matrix4 rotation = compute_edge_rotation(ca->vertex, cb->vertex);

    ca->remove_link(cb);
    cb->remove_link(ca);

    build_single_edge(rotation, *ca, *cb);
  }

  // Final sanity check
  for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
  {
    g_return_if_fail((*p)->link[0] == 0);
  }
}
#endif

void CubeTesselator::Impl::build_single_edge(const Matrix4& rotation,
                                             const EdgeCorner& ca, const EdgeCorner& cb)
{
  {
    begin_strip();

    for (int i = 0; i <= EDGE_SLICES; ++i)
    {
      const Vector4 normal = rotation * edgedata[i];
      {
        const Vector4 vector = rotation * adjust_joint(i, cb.flags | EF_EDGEEND);
        const Vector4 vertex = (cb.vertex + vector * edgeradius) * cellsize;

        strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
      }
      {
        const Vector4 vector = rotation * adjust_joint(i, ca.flags);
        const Vector4 vertex = (ca.vertex + vector * edgeradius) * cellsize;

        strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
      }
    }

    end_strip();
  }
}

#if 0
void CubeTesselator::Impl::build_edge_stripe2(const Matrix4& rotation,
                                              const EdgeCorner& ca, const EdgeCorner& cb)
{
  static const Matrix4::array_type turnmatrices[2] =
  {
    { {0, 1, 0, 0}, {-1, 0, 0, 0}, { 0, 0, 1, 0}, {0, 0, 0, 1} },  //  90 deg around z
    { {0, 0, 1, 0}, { 0, 1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1} }   // 270 deg around y
  };

  for (int i = 0; i < EDGE_SLICES; ++i)
  {
    matrix_ = rotation;

    {
      begin_strip();

      build_corner_slice(ca.vertex, cornerdata[i], 2 * i + 3, G_N_ELEMENTS(cornerdata[i]));

      matrix_ *= turnmatrices[0];

      build_corner_slice(cb.vertex, cornerdata[i], 0, 2 * i + 3);

      end_strip();
    }
  }
}
#endif

void CubeTesselator::Impl::build_edge_stripe(const Matrix4& rotation, const CornerStore& stripe)
{
  static const Matrix4::array_type turnmatrices[2] =
  {
    { {0, 1, 0, 0}, {-1, 0, 0, 0}, { 0, 0, 1, 0}, {0, 0, 0, 1} },  //  90 deg around z
    { {0, 0, 1, 0}, { 0, 1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1} }   // 270 deg around y
  };

  for (int i = 0; i < EDGE_SLICES; ++i)
  {
    matrix_ = rotation;

    {
      begin_strip();

      terminate_edge_slice(stripe.front()->vertex, stripe.front()->flags, i, i + 1);

      unsigned int odd = 0;

      for (unsigned int k = 1; k + 1 < stripe.size(); ++k)
      {
        if ((stripe[k]->flags & EF_CONTINUE) == 0)
        {
          odd ^= 1;
          matrix_ *= turnmatrices[odd];

          if (odd)
            build_corner_slice(stripe[k]->vertex, cornerdata[i], 2 * i + 3,
                               G_N_ELEMENTS(cornerdata[i]));
          else
            build_corner_slice(stripe[k]->vertex, cornerdata[i], 0, 2 * i + 3);
        }
        else
        {
          terminate_edge_slice(stripe[k]->vertex, 0, i + odd, i + 1 - odd);
        }
      }

      terminate_edge_slice(stripe.back()->vertex, stripe.back()->flags | EF_EDGEEND,
                           i + odd, i + 1 - odd);
      end_strip();
    }
  }
}

void CubeTesselator::Impl::build_corner_slice(const Vector4& origin,
                                              const Vector4::array_type* slice,
                                              int begin, int end)
{
  for (int i = begin; i < end; ++i)
  {
    const Vector4 normal = matrix_ * slice[i];
    const Vector4 vertex = (origin + normal * edgeradius) * cellsize;

    strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
  }
}

void CubeTesselator::Impl::terminate_edge_slice(const Vector4& origin, EdgeFlags flags,
                                                int a, int b)
{
  {
    const Vector4 normal = matrix_ * edgedata[a];
    const Vector4 vector = matrix_ * adjust_joint(a, flags);
    const Vector4 vertex = (origin + vector * edgeradius) * cellsize;

    strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
  }
  {
    const Vector4 normal = matrix_ * edgedata[b];
    const Vector4 vector = matrix_ * adjust_joint(b, flags);
    const Vector4 vertex = (origin + vector * edgeradius) * cellsize;

    strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
  }
}

void CubeTesselator::Impl::build_bridge(const Matrix4& rotation,
                                        const Vector4& origin, EdgeFlags flags)
{
  static const Matrix4::array_type matrix_y =
  {
    {0, -1, 0, 0}, {0, 0, -1, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1}
  };
  static const Matrix4::array_type matrix_z =
  {
    {0, 0, -1, 0}, {-1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 0, 1}
  };

  const Vector4 r = Vector4::rint(origin + centeroffset);
  const EdgeCorner* cb = 0;

  for (CornerStore::iterator p = cornerstore_.begin(); p != cornerstore_.end(); ++p)
  {
    if (((*p)->flags & EF_BRIDGE) != 0 && Vector4::rint((*p)->vertex + centeroffset) == r)
    {
      (*p)->flags = 0;
      cb = *p;
      break;
    }
  }

  g_return_if_fail(cb != 0);

  switch (flags & EF_JOINYZ)
  {
    case EF_JOINY:
      build_bridge_strip(rotation, rotation * matrix_y, origin, cb->vertex, normaldata_bridge_y);
      break;

    case EF_JOINZ:
      build_bridge_strip(rotation, rotation * matrix_z, origin, cb->vertex, normaldata_bridge_z);
      break;

    default:
      g_return_if_reached();
  }
}

void CubeTesselator::Impl::build_bridge_strip(const Matrix4& matrixa, const Matrix4& matrixb,
                                              const Vector4& a, const Vector4& b,
                                              const Vector4::array_type* normals)
{
  {
    begin_strip();

    for (int i = 0; i <= EDGE_SLICES; ++i)
    {
      {
        const Vector4 normal = matrixa * normals[2 * i];
        const Vector4 vector = matrixa * edgedata[i];
        const Vector4 vertex = (a + vector * edgeradius) * cellsize;

        strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
      }
      {
        const Vector4 normal = matrixa * normals[2 * i + 1];
        const Vector4 vector = matrixb * edgedata[i];
        const Vector4 vertex = (b + vector * edgeradius) * cellsize;

        strip_element(CubeElement(Vector4(edgetexcoord), normal, vertex));
      }
    }

    end_strip();
  }
}

} // namespace Somato
