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

#include <config.h>

#include "mathutils.h"

#include <limits>
#include <cmath>
#include <cstdlib>

namespace
{

/*
 * Project an (x, y) pair onto a sphere of radius r,
 * or a hyperbolic sheet if we are away from the center of the sphere.
 */
Math::Vector4 project_to_sphere(float x, float y, float r)
{
  const float t = r * r * 0.5f;  // square of r * sin(PI/4)
  const float d = x * x + y * y;

  // Inside sphere if d < t, otherwise on hyperbola.
  const float z = (d < t) ? std::sqrt(r * r - d) : (t / std::sqrt(d));

  return {x, y, z};
}

} // anonymous namespace

Math::Quat Math::trackball_motion(float x1, float y1, float x2, float y2, float trackballsize)
{
  const float epsilon = std::numeric_limits<float>::epsilon();

  if (std::abs(x2 - x1) < epsilon && std::abs(y2 - y1) < epsilon)
  {
    // Zero rotation
    return {};
  }
  else
  {
    // First, figure out z-coordinates for projection of P1 and P2 to
    // deformed sphere.
    const Math::Vector4 a = project_to_sphere(x1, y1, trackballsize);
    const Math::Vector4 b = project_to_sphere(x2, y2, trackballsize);

    // Determine axis of rotation and cosine of angle.
    return Math::Quat::from_vectors(a, b);
  }
}
