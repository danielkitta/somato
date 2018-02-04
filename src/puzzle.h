/*
 * Copyright (c) 2004-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "asynctask.h"
#include "bitcube.h"
#include "puzzlecube.h"
#include "vectormath.h"

#include <array>
#include <vector>

namespace Somato
{

typedef BitCube<3>       SomaBitCube;
typedef PuzzleCube<3, 7> SomaCube;

class PuzzleThread : public Async::Task
{
public:
  PuzzleThread();
  virtual ~PuzzleThread();

  std::vector<SomaCube> acquire_results();

private:
  void execute() override;

  std::vector<SomaCube> solutions_;
};

Math::Matrix4 find_puzzle_piece_orientation(int piece_idx, SomaBitCube piece);

} // namespace Somato

#endif // !SOMATO_PUZZLE_H_INCLUDED
