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

#ifndef SOMATO_GLTYPES_H_INCLUDED
#define SOMATO_GLTYPES_H_INCLUDED

#include <glib.h>
#include <cmath>

namespace GL
{

enum Packed2i16 : unsigned int {};
enum Packed4u8  : unsigned int {};
enum Int_2_10_10_10_rev : unsigned int {};

inline Packed2i16 pack_2i16(int x, int y)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  return static_cast<Packed2i16>((x & 0xFFFFu) | ((y & 0xFFFFu) << 16));
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  return static_cast<Packed2i16>(((x & 0xFFFFu) << 16) | (y & 0xFFFFu));
#endif
}

inline Packed2i16 pack_2i16_norm(float x, float y)
{
  const float scale = 32767.f;
  return pack_2i16(std::lrint(x * scale), std::lrint(y * scale));
}

inline Packed4u8 pack_4u8(int r, int g, int b, int a)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  return static_cast<Packed4u8>((r & 0xFFu)        | ((g & 0xFFu) << 8)
                             | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24));
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  return static_cast<Packed4u8>(((r & 0xFFu) << 24) | ((g & 0xFFu) << 16)
                              | ((b & 0xFFu) << 8)  |  (a & 0xFFu));
#endif
}

inline Packed4u8 pack_4u8_norm(float r, float g, float b, float a)
{
  const float scale = 255.f;
  return pack_4u8(std::lrint(r * scale), std::lrint(g * scale),
                  std::lrint(b * scale), std::lrint(a * scale));
}

inline Int_2_10_10_10_rev pack_3i10rev(int x, int y, int z)
{
  return static_cast<Int_2_10_10_10_rev>((x & 0x3FFu)
                                      | ((y & 0x3FFu) << 10)
                                      | ((z & 0x3FFu) << 20));
}

inline Int_2_10_10_10_rev pack_3i10rev_norm(float x, float y, float z)
{
  const float s = 511.f;
  return pack_3i10rev(std::lrint(x * s), std::lrint(y * s), std::lrint(z * s));
}

} // namespace GL

#endif // !SOMATO_GLTYPES_H_INCLUDED
