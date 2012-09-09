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

#include "puzzle.h"

#include <glib.h>
#include <sigc++/sigc++.h>
#include <glibmm/thread.h>

#include <algorithm>
#include <numeric>

#include <config.h>

namespace
{

using Somato::Cube;

typedef std::vector<Cube>       PieceStore;
typedef std::vector<PieceStore> ColumnStore;

class PuzzleSolver
{
private:
  ColumnStore                   columns_;
  std::vector<Somato::Solution> solutions_;
  Somato::Solution              state_;

  // noncopyable
  PuzzleSolver(const PuzzleSolver&) = delete;
  PuzzleSolver& operator=(const PuzzleSolver&) = delete;

  void recurse(int col, Cube cube);
  void add_solution();

public:
  PuzzleSolver();
  ~PuzzleSolver();

  void execute();
  std::vector<Somato::Solution>& result() { return solutions_; }
};

/*
 * Cube pieces rearranged for maximum efficiency.  It is about 15 times
 * faster than with the original order from the project description.
 * The cube piece at index 0 should be suitable for use as the anchor.
 */
static
const bool cube_piece_data[Somato::CUBE_PIECE_COUNT][3][3][3] =
{
  { // Piece #6
    { {1,1,0}, {0,0,0}, {0,0,0} },
    { {0,1,0}, {0,1,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #7
    { {1,1,0}, {0,1,0}, {0,0,0} },
    { {0,1,0}, {0,0,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #5
    { {1,1,0}, {1,0,0}, {0,0,0} },
    { {0,1,0}, {0,0,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #4
    { {1,0,0}, {1,0,0}, {0,0,0} },
    { {0,0,0}, {1,0,0}, {1,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #3
    { {1,0,0}, {1,0,0}, {1,0,0} },
    { {0,0,0}, {1,0,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #2
    { {1,0,0}, {1,0,0}, {1,0,0} },
    { {1,0,0}, {0,0,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  },
  { // Piece #1
    { {1,0,0}, {1,0,0}, {0,0,0} },
    { {1,0,0}, {0,0,0}, {0,0,0} },
    { {0,0,0}, {0,0,0}, {0,0,0} }
  }
};

/*
 * Rotate the cube.  This takes care of all orientations possible.
 */
static
void compute_rotations(Cube cube, PieceStore& store)
{
  for (unsigned int i = 0;; ++i)
  {
    Cube temp = cube;

    // Add the 4 possible orientations of each cube side.
    store.push_back(temp);
    store.push_back(temp.rotate(Cube::AXIS_Z));
    store.push_back(temp.rotate(Cube::AXIS_Z));
    store.push_back(temp.rotate(Cube::AXIS_Z));

    if (i == 5)
      break;

    // Due to the zigzagging performed here, only 5 rotations are
    // necessary to move each of the 6 cube sides in turn to the front.
    cube.rotate(Cube::AXIS_X + i % 2);
  }
}

/*
 * Push the Soma block around; into every position respectively rotation
 * imaginable.  Note that the block is assumed to be positioned initially
 * in the (0, 0, 0) corner of the cube.
 */
static
void shuffle_cube_piece(Cube cube, PieceStore& store)
{
  // Make sure the piece is positioned where we expect it to be.
  g_return_if_fail(cube.get(0, 0, 0));

  for (Cube z = cube; z != Cube(); z.shift(Cube::AXIS_Z))
    for (Cube y = z; y != Cube(); y.shift(Cube::AXIS_Y))
      for (Cube x = y; x != Cube(); x.shift(Cube::AXIS_X))
      {
        compute_rotations(x, store);
      }
}

/*
 * Replace store by a new set of piece placements that contains only those
 * items from the source which cannot be reproduced by rotating any other
 * item.  This is not a universally applicable utility function; the input
 * is assumed to have come straight out of shuffle_cube_piece().
 */
static
void filter_rotations(PieceStore& store)
{
  g_return_if_fail(store.size() % 24 == 0);

  PieceStore::iterator pdest = store.begin();

  for (PieceStore::const_iterator p = store.begin(); p != store.end(); p += 24)
  {
    *pdest++ = *std::min_element(p, p + 24, Cube::SortPredicate());
  }

  store.erase(pdest, store.end());
}

static
bool find_piece_translation(Cube original, Cube piece, Math::Matrix4& transform)
{
  using Math::Matrix4;
  using Math::Vector4;

  int z = 0;

  for (Cube piece_z = piece; piece_z != Cube(); piece_z.shift_rev(Cube::AXIS_Z))
  {
    int y = 0;

    for (Cube piece_y = piece_z; piece_y != Cube(); piece_y.shift_rev(Cube::AXIS_Y))
    {
      int x = 0;

      for (Cube piece_x = piece_y; piece_x != Cube(); piece_x.shift_rev(Cube::AXIS_X))
      {
        if (piece_x == original)
        {
          transform *= Matrix4{Matrix4::identity[0],
                               Matrix4::identity[1],
                               Matrix4::identity[2],
                               Vector4(x, y, z, 1.0f)};
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

PuzzleSolver::PuzzleSolver()
:
  columns_ (Somato::CUBE_PIECE_COUNT)
{}

PuzzleSolver::~PuzzleSolver()
{}

void PuzzleSolver::execute()
{
  solutions_.reserve(512);

  for (int i = 0; i < Somato::CUBE_PIECE_COUNT; ++i)
  {
    PieceStore& store = columns_[i];

    store.reserve(256);
    shuffle_cube_piece(Cube(cube_piece_data[i]), store);

    if (i == 0)
      filter_rotations(store);

    std::sort(store.begin(), store.end(), Cube::SortPredicate());
    store.erase(std::unique(store.begin(), store.end()), store.end());
  }

  const Cube common = std::accumulate(columns_[0].begin(), columns_[0].end(),
                                      ~Cube(), Util::Intersect<Cube>());

  if (common != Cube())
    for (int i = 1; i < Somato::CUBE_PIECE_COUNT; ++i)
    {
      columns_[i].erase(std::remove_if(columns_[i].begin(), columns_[i].end(),
                                       Util::DoesIntersect<Cube>(common)),
                        columns_[i].end());
    }

  // Add zero-termination.
  for (int i = 0; i < Somato::CUBE_PIECE_COUNT; ++i)
    columns_[i].push_back(Cube());

  recurse(0, Cube());
}

void PuzzleSolver::recurse(int col, Cube cube)
{
  PieceStore::const_iterator row = columns_[col].begin();

  for (;;)
  {
    const Cube cell = *row;

    ++row;

    if ((cell & cube) == Cube())
    {
      if (cell == Cube())
        break;

      state_[col] = cell;

      if (col < Somato::CUBE_PIECE_COUNT - 1)
        recurse(col + 1, cube | cell);
      else
        add_solution();
    }
  }
}

void PuzzleSolver::add_solution()
{
  // This innocent line translates to quite a bit of code.  Moving this
  // out of recurse() helps the compiler generate optimal code where it
  // is actually needed.
  solutions_.push_back(state_);
}

} // anonymous namespace

namespace Somato
{

// MS Visual C++ complains about the use of 'this' in an initializer list.
// However, it harmless in this case as only a base object will be accessed.
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4355)
#endif
PuzzleThread::PuzzleThread()
:
  thread_exit_ {signal_exit_.connect(sigc::mem_fun(*this, &PuzzleThread::on_thread_exit))},
  thread_      {nullptr}
{}
#ifdef _MSC_VER
# pragma warning(pop)
#endif

PuzzleThread::~PuzzleThread()
{
  thread_exit_.disconnect();

  // Normally, the thread should not be running anymore at this point,
  // but in case it is we have to wait in order to ensure proper cleanup.
  if (thread_)
    thread_->join();
}

void PuzzleThread::set_on_done(std::function<void ()> func)
{
  done_func_ = std::move(func);
}

void PuzzleThread::run()
{
  g_return_if_fail(thread_ == 0);

  thread_ = Glib::Thread::create(sigc::mem_fun(*this, &PuzzleThread::execute), true);
}

void PuzzleThread::swap_result(std::vector<Solution>& result)
{
  g_return_if_fail(thread_ == 0);

  solutions_.swap(result);
}

/*
 * We can get away without any explicit synchronization, as long as the
 * thread is always properly joined in response to its exit notification.
 */
void PuzzleThread::execute()
{
  try
  {
    PuzzleSolver solver;

    solver.execute();
    solver.result().swap(solutions_);
  }
  catch (...)
  {
    signal_exit_(); // emit
    throw;
  }

  signal_exit_(); // emit
}

void PuzzleThread::on_thread_exit()
{
  thread_->join();
  thread_ = 0;

  if (done_func_)
    done_func_();
}

Math::Matrix4 find_puzzle_piece_orientation(int piece_idx, Cube piece)
{
  using Math::Matrix4;

  static const Matrix4::array_type rotate90[3] =
  {
    { {1, 0,  0, 0}, { 0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1} }, // 90 deg around x
    { {0, 0, -1, 0}, { 0, 1,  0, 0}, {1, 0, 0, 0}, {0, 0, 0, 1} }, // 90 deg around y
    { {0, 1,  0, 0}, {-1, 0,  0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} }  // 90 deg around z
  };

  Matrix4 transform {Matrix4::identity};

  g_return_val_if_fail(piece_idx >= 0 && piece_idx < CUBE_PIECE_COUNT, transform);

  const Cube original {cube_piece_data[piece_idx]};

  for (unsigned int i = 0; i < 6; ++i)
  {
    // Add the 4 possible orientations of each cube side.
    for (int k = 0; k < 4; ++k)
    {
      if (find_piece_translation(original, piece, transform))
        return transform;

      piece.rotate(Cube::AXIS_Z);
      transform *= rotate90[Cube::AXIS_Z];
    }

    // Due to the zigzagging performed here, only 5 rotations are
    // necessary to move each of the 6 cube sides in turn to the front.
    piece.rotate(i % 2);
    transform *= rotate90[i % 2];
  }

  g_warn_if_reached();
  return transform;
}

} // namespace Somato
