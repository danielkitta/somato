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

#ifndef SOMATO_MATHUTILS_H_INCLUDED
#define SOMATO_MATHUTILS_H_INCLUDED

#include "vectormath.h"

#include <glib.h>
#include <algorithm>

namespace Math
{

/*
 * Convenience shortcut to clamp a value to a range.
 * Note that the result is undefined if lower > upper.
 */
template <class T>
inline T clamp(T value, T lower, T upper)
{
  return std::min(std::max(value, lower), upper);
}

/*
 * Safely convert a floating point number to an integer.  The return
 * value is clamped to fit into an int, while also avoiding the integer
 * indeterminate value.
 */
inline int clamp_to_int(double value)
{
  if (!G_LIKELY(value < 0. - G_MININT))
    return -1 - G_MININT;
  if (!G_LIKELY(value > 0. + G_MININT))
    return  1 + G_MININT;

  return static_cast<int>(value);
}

/* Round up size to the next multiple of alignment. The alignment
 * must be a positive power of two.
 */
inline unsigned int align(unsigned int size, unsigned int alignment)
{
  return (size + (alignment - 1)) & ~(alignment - 1);
}

/*
 * Simulate a trackball.  Project the points onto the virtual trackball,
 * then figure out the axis of rotation.
 *
 * Note:  This is a deformed trackball -- it is a trackball in the center,
 * but is deformed into a hyperbolic sheet of rotation away from the center.
 * This particular function was chosen after trying out several variations.
 *
 * The arguments should be set up so that the coordinates (-1, -1) and
 * (+1, +1) denote the bottom-left respectively top-right corner of the
 * trackball area.  This area might, for instance, be the whole window
 * or the largest possible square enclosed within the window borders.
 *
 * It is not an error if an argument exceeds the [-1, 1] range since there
 * are situations where this is perfectly valid, for example if the mouse
 * pointer leaves the window while dragging.
 */
Quat trackball_motion(float x1, float y1, float x2, float y2, float trackballsize);

} // namespace Math

#endif // !SOMATO_MATHUTILS_H_INCLUDED
