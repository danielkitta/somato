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

#include <config.h>
#include "assetresourceio.h"

#include <glib.h>
#include <giomm/inputstream.h>
#include <giomm/resource.h>

#include <assimp/IOStream.hpp>
#include <string>

namespace
{

class AssetResourceIoStream : public Assimp::IOStream
{
public:
  explicit AssetResourceIoStream(const char* name);
  virtual ~AssetResourceIoStream();

  size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override;
  size_t Write(const void* pvBuffer, size_t pSize, size_t pCount) override;
  aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override;
  size_t Tell() const override;
  size_t FileSize() const override;
  void Flush() override;

private:
  std::string                    name_;
  Glib::RefPtr<Gio::InputStream> stream_;
};

AssetResourceIoStream::AssetResourceIoStream(const char* name)
:
  name_   {name},
  stream_ {Gio::Resource::open_stream_global(name_)}
{}

AssetResourceIoStream::~AssetResourceIoStream()
{}

size_t AssetResourceIoStream::Read(void* pvBuffer, size_t pSize, size_t pCount)
{
  g_return_val_if_fail(stream_, 0);

  if (pSize == 0)
    return 0;

  const gssize n_read = stream_->read(pvBuffer, pSize * pCount);

  return (n_read > 0) ? static_cast<gsize>(n_read) / pSize : 0;
}

size_t AssetResourceIoStream::Write(const void*, size_t, size_t)
{
  g_assert_not_reached();
  return 0;
}

aiReturn AssetResourceIoStream::Seek(size_t, aiOrigin)
{
  g_warn_if_reached();
  return aiReturn_FAILURE;
}

size_t AssetResourceIoStream::Tell() const
{
  g_warn_if_reached();
  return 0;
}

size_t AssetResourceIoStream::FileSize() const
{
  gsize size = 0;
  Gio::ResourceFlags flags = {};

  Gio::Resource::get_info_global(name_, size, flags);
  return size;
}

void AssetResourceIoStream::Flush()
{}

} // anonymous namespace

namespace Util
{

AssetResourceIoSystem::AssetResourceIoSystem()
{}

AssetResourceIoSystem::~AssetResourceIoSystem()
{}

bool AssetResourceIoSystem::Exists(const char* pFile) const
{
  return Gio::Resource::get_file_exists_global_nothrow(pFile);
}

char AssetResourceIoSystem::getOsSeparator() const
{
  return '/';
}

Assimp::IOStream* AssetResourceIoSystem::Open(const char* pFile, const char* pMode)
{
  g_return_val_if_fail(pMode[0] == 'r', nullptr);

  return new AssetResourceIoStream{pFile};
}

void AssetResourceIoSystem::Close(Assimp::IOStream* pFile)
{
  delete pFile;
}

} // namespace Util
