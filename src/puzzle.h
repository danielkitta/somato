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
 * along with Somato.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SOMATO_PUZZLE_H_INCLUDED
#define SOMATO_PUZZLE_H_INCLUDED

#include "cube.h"
#include "vectormath.h"

#include <glibmm/dispatcher.h>
#include <array>
#include <functional>
#include <thread>
#include <vector>

namespace Somato
{

enum { CUBE_PIECE_COUNT = 7 };

typedef std::array<Cube, CUBE_PIECE_COUNT> Solution;

class PuzzleThread
{
public:
  PuzzleThread();
  virtual ~PuzzleThread();

  void set_on_done(std::function<void ()> func);
  void run();
  std::vector<Solution> acquire_results();

private:
  std::vector<Solution>  solutions_;
  std::function<void ()> done_func_;
  Glib::Dispatcher       signal_exit_;
  sigc::connection       thread_exit_;
  std::thread            thread_;

  // noncopyable
  PuzzleThread(const PuzzleThread&) = delete;
  PuzzleThread& operator=(const PuzzleThread&) = delete;

  void execute();
  void on_thread_exit();
};

Math::Matrix4 find_puzzle_piece_orientation(int piece_idx, Cube piece);

} // namespace Somato

#endif /* SOMATO_PUZZLE_H_INCLUDED */
