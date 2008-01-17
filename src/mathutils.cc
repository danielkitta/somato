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

#include "mathutils.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>

namespace
{

/*
 * Project an (x, y) pair onto a sphere of radius r,
 * or a hyperbolic sheet if we are away from the center of the sphere.
 */
static
Math::Vector4 project_to_sphere(float x, float y, float r)
{
  const float t = r * r * 0.5f;  // square of r * sin(PI/4)
  const float d = x * x + y * y;

  // Inside sphere if d < t, otherwise on hyperbola.
  const float z = (d < t) ? std::sqrt(r * r - d) : (t / std::sqrt(d));

  return Math::Vector4(x, y, z);
}

} // anonymous namespace

Math::Quat Math::trackball_motion(float x1, float y1, float x2, float y2, float trackballsize)
{
  const float epsilon = std::numeric_limits<float>::epsilon();

  if (std::abs(x2 - x1) < epsilon && std::abs(y2 - y1) < epsilon)
  {
    // Zero rotation
    return Math::Quat();
  }
  else
  {
    // First, figure out z-coordinates for projection of P1 and P2 to
    // deformed sphere.
    const Math::Vector4 p1 = project_to_sphere(x1, y1, trackballsize);
    const Math::Vector4 p2 = project_to_sphere(x2, y2, trackballsize);

    // Compute the the cross product of P1 and P2.
    const Math::Vector4 axis = p1 % p2;

    // Figure out how much to rotate around that axis.
    const float t   = Math::Vector4::mag(p2 - p1) / (2.0f * trackballsize);
    const float phi = 2.0f * std::asin(Math::clamp(t, -1.0f, 1.0f));

    return Math::Quat::from_axis(axis, phi);
  }
}
