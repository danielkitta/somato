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

#ifndef SOMATO_PUZZLE_H_INCLUDED
#define SOMATO_PUZZLE_H_INCLUDED

#include "array.h"
#include "cube.h"

#include <glibmm/dispatcher.h>
#include <vector>

#ifndef SOMATO_HIDE_FROM_INTELLISENSE
namespace Glib { class Thread; }
#endif

namespace Somato
{

enum { CUBE_PIECE_COUNT = 7 };

typedef Util::Array<Cube, CUBE_PIECE_COUNT> Solution;

class PuzzleThread
{
public:
  PuzzleThread();
  virtual ~PuzzleThread();

  sigc::signal<void>& signal_done() { return signal_done_; }

  void run();
  void swap_result(std::vector<Solution>& result);

private:
  std::vector<Solution> solutions_;
  sigc::signal<void>    signal_done_;
  Glib::Dispatcher      signal_exit_;
  sigc::connection      thread_exit_;
  Glib::Thread*         thread_;

  // noncopyable
  PuzzleThread(const PuzzleThread&);
  PuzzleThread& operator=(const PuzzleThread&);

  void execute();
  void on_thread_exit();
};

Cube puzzle_piece_at_origin(int index);

} // namespace Somato

#endif /* SOMATO_PUZZLE_H_INCLUDED */
