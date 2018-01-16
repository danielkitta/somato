/*
 * Copyright (c) 2004-2018  Daniel Elstner  <daniel.kitta@gmail.com>
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
#include "vectormath.h"

namespace Math
{

const std::array<Vector4, 4> Vector4::basis =
{{
  {1.f, 0.f, 0.f, 0.f},
  {0.f, 1.f, 0.f, 0.f},
  {0.f, 0.f, 1.f, 0.f},
  {0.f, 0.f, 0.f, 1.f},
}};

Matrix4::Matrix4()
:
  Matrix4{Vector4::basis[0], Vector4::basis[1], Vector4::basis[2], Vector4::basis[3]}
{}

} // namespace Math
