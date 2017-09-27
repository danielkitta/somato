/*
 * Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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

#ifndef SOMATO_ASSETRESOURCEIO_H_INCLUDED
#define SOMATO_ASSETRESOURCEIO_H_INCLUDED

#include <assimp/IOSystem.hpp>

namespace Util
{

class AssetResourceIoSystem : public Assimp::IOSystem
{
public:
  AssetResourceIoSystem();
  virtual ~AssetResourceIoSystem();

  bool Exists(const char* pFile) const override;
  char getOsSeparator() const override;
  Assimp::IOStream* Open(const char* pFile, const char* pMode) override;
  void Close(Assimp::IOStream* pFile) override;
};

} // namespace Util

#endif // !SOMATO_ASSETRESOURCEIO_H_INCLUDED
