/*-
 * Copyright (c) 2021 Samantha Payson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "asset.h"

#include <cstring>

#include <iostream>
#include <fstream>

void asset::writeStaticMeshFile(char const             *path,
				const StaticMeshData   *meshes,  uint32_t meshCount,
				const StaticVertexData *verts,   uint32_t vertexCount,
				const uint16_t         *indices, uint32_t indexCount)
{
  StaticMeshFileHeader header;

  header.meshCount   = meshCount;
  header.vertexCount = vertexCount;
  header.indexCount  = indexCount;

  std::ofstream out(path, std::ios::binary);

  out.write((char *)&header, sizeof header);
  out.write((char *)meshes, sizeof(StaticMeshData)*meshCount);

  out.write((char *)verts, sizeof(StaticVertexData)*vertexCount);
  out.write((char *)indices, sizeof(uint16_t)*indexCount);
}

asset::StaticMeshFileHandle asset::openStaticMeshFile(std::string const &path) {
  std::ifstream stream(path, std::ios::binary);

  StaticMeshFileHandle handle = std::make_unique<StaticMeshFileHandleBuffer>();

  stream.read((char *)&handle->_header, sizeof(StaticMeshFileHeader));

  handle->_meshes.resize(handle->_header.meshCount);
  stream.read((char *)handle->_meshes.data(),
	      handle->_header.meshCount * sizeof(StaticMeshData));

  handle->_stream = std::move(stream);

  return handle;
}

std::ostream &asset::operator <<(std::ostream &stream, const asset::StaticMeshFileHandle &handle) {
  stream << "StaticMesh {" << std::endl;
  stream << "  meshCount:   " << handle->_header.meshCount << "," << std::endl;
  stream << "  vertexCount: " << handle->_header.vertexCount << "," << std::endl;
  stream << "  indexCount:  " << handle->_header.indexCount << "," << std::endl;
  for (uint32_t i = 0; i < handle->_header.meshCount; i++) {
    stream << "  mesh " << handle->_meshes[i].id << " {" << std::endl;
    stream << "    vertexOffset: " << handle->_meshes[i].vertexOffset << "," << std::endl;
    stream << "    vertexCount:  " << handle->_meshes[i].vertexCount  << "," << std::endl;
    stream << "    indexOffset:  " << handle->_meshes[i].indexOffset  << "," << std::endl;
    stream << "    indexCount:   " << handle->_meshes[i].indexCount   << "," << std::endl;
    stream << "    bounds {" << std::endl;
    stream << "      max: vec3("
	   << handle->_meshes[i].bounds.max.x << ", "
	   << handle->_meshes[i].bounds.max.y << ", "
	   << handle->_meshes[i].bounds.max.z << ")" << std::endl;
    stream << "      min: vec3("
	   << handle->_meshes[i].bounds.min.x << ", "
	   << handle->_meshes[i].bounds.min.y << ", "
	   << handle->_meshes[i].bounds.min.z << ")" << std::endl;
    stream << "    }" << std::endl;
    stream << "  }" << std::endl;
  }
  stream << "}" << std::endl;

  return stream;
}

asset::StaticMeshFileHandleBuffer::~StaticMeshFileHandleBuffer() {
  _stream.close();
}

asset::StaticMeshData *
asset::StaticMeshFileHandleBuffer::getMeshData(asset::MeshID id) {
  for (size_t i = 0; i < _header.meshCount; i++) {
    if (_meshes[i].id == id) {
      return &_meshes[i];
    }
  }

  return nullptr;
}

bool asset::StaticMeshFileHandleBuffer::readMesh(asset::MeshID     id,
						 StaticVertexData  *verts,
						 uint16_t          *indices)
{
  int idx = -1;

  for (int i = 0; static_cast<size_t>(i) < _header.meshCount; i++) {
    if (_meshes[i].id == id) {
      idx = i;
    }
  }

  if (idx == -1) return false;

  auto *mesh = &_meshes[idx];

  size_t vertexByteOffs = _vertexOffsetToBytes(mesh->vertexOffset);
  size_t indexByteOffs  = _indexOffsetToBytes(mesh->indexOffset);

  _stream.seekg(vertexByteOffs, std::ios::beg);
  _stream.read((char *)verts,
		      mesh->vertexCount*sizeof(StaticVertexData));


  _stream.seekg(indexByteOffs, std::ios::beg);
  _stream.read((char *)indices,
		      mesh->indexCount*sizeof(uint16_t));

  return true;
}

asset::VertexInputDescription
asset::StaticVertexData::vertexInputDescription() {
  asset::VertexInputDescription inputDesc;

  VkVertexInputBindingDescription binding = {
    .binding   = 0,
    .stride    = sizeof(StaticVertexData),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  inputDesc.bindings.push_back(binding);

  VkVertexInputAttributeDescription position = {
    .binding   = 0,
    .location  = 0,
    .format    = VK_FORMAT_R32G32B32_SFLOAT,
    .offset    = offsetof(asset::StaticVertexData, position),
  };

  VkVertexInputAttributeDescription normal = {
    .binding   = 0,
    .location  = 1,
    .format    = VK_FORMAT_R32G32B32_SFLOAT,
    .offset    = offsetof(asset::StaticVertexData, normal),
  };

  VkVertexInputAttributeDescription uv = {
    .binding   = 0,
    .location  = 2,
    .format    = VK_FORMAT_R32G32_SFLOAT,
    .offset    = offsetof(asset::StaticVertexData, uv),
  };

  inputDesc.attribs.push_back(position);
  inputDesc.attribs.push_back(normal);
  inputDesc.attribs.push_back(uv);

  inputDesc.flags = 0;

  return inputDesc;
}

size_t asset::StaticMeshFileHandleBuffer::_vertexOffsetToBytes(size_t vertOffset) const {
  return sizeof(StaticMeshFileHeader)
    + _header.meshCount * sizeof(StaticMeshData)
    + vertOffset * sizeof(StaticVertexData)
    ;
}

size_t asset::StaticMeshFileHandleBuffer::_indexOffsetToBytes(size_t indexOffset) const {
  return sizeof(StaticMeshFileHeader)
    + _header.meshCount * sizeof(StaticMeshData)
    + _header.vertexCount * sizeof(StaticVertexData)
    + indexOffset * sizeof(uint16_t)
    ;
}

asset::LibraryFileHandle asset::openLibraryFile(std::string const & path) {
  std::ifstream file(path, std::ios::binary);

  LibraryFileHandle handle = std::make_unique<LibraryFileHandleBuffer>();

  if (!file.is_open()) {
    std::cerr << "Failed to open library file '" << path << "'" << std::endl;
    std::exit(-1);
  }

  LibraryFileHeader header;

  // If file.read fails and we don't write to header, this ensures that the
  // magicNumber won't match.
  header.magicNumber[0] = '\0';

  file.read((char *)&header, sizeof(LibraryFileHeader));

  if (strncmp(header.magicNumber, LibraryFileHeader::MAGIC_NUMBER, 32)) {
    std::cerr << "Bad magic number for library file '" << path << "'" << std::endl;
    std::exit(-1);
  }

  handle->_assetRefs.resize(header.assetRefCount);
  handle->_pathData.resize(header.pathByteCount);

  file.read((char *)handle->_assetRefs.data(), header.assetRefCount*sizeof(LibraryAssetRef));
  file.read((char *)handle->_pathData.data(), header.pathByteCount);

  file.close();

  return handle;
}

std::ostream & asset::operator <<(std::ostream &os,
				  const asset::LibraryFileHandle &handle)
{
  os << "library {" << std::endl;
  for (const auto &ref : handle->_assetRefs) {
    os << "  mesh {" << std::endl;
    os << "    id:   " << ref.assetID << std::endl;
    os << "    path: " << (handle->_pathData.data() + ref.pathOffset) << std::endl;
    os << "  }" << std::endl;
  }
  os << std::endl;

  return os;
}

asset::LibraryFileHandle asset::emptyLibraryFileHandle() {
  return std::make_unique<LibraryFileHandleBuffer>();
}

void asset::LibraryFileHandleBuffer::write(const std::string &path) const {
  LibraryFileHeader header;
  std::ofstream file(path, std::ios::binary);

  header.assetRefCount = (uint32_t)_assetRefs.size();
  header.pathByteCount = (uint32_t)_pathData.size();
  file.write((char const *)&header, sizeof(LibraryFileHeader));

  file.write((char const *)_assetRefs.data(), _assetRefs.size()*sizeof(LibraryAssetRef));
  file.write((char const *)_pathData.data(),  _pathData.size());
}

void asset::LibraryFileHandleBuffer::addMeshRef(MeshID id, std::string const &path) {
  LibraryAssetRef assetRef = {
    .assetID    = id,
    .assetType  = AssetType::StaticMesh,
    .pathOffset = (uint32_t)_pathData.size(),
  };

  _assetRefs.push_back(assetRef);
  _pathData.insert(_pathData.end(), path.begin(), path.end());
  _pathData.push_back('\0');
}

asset::StaticMeshFileHandle &
asset::LibraryFileHandleBuffer::_getStaticMeshFileHandle(std::string const &path)
{
  auto cached = _staticMeshHandleCache.find(path);
  if (cached != _staticMeshHandleCache.end()) {
    return cached->second;
  } else {
    return _staticMeshHandleCache.emplace(path, openStaticMeshFile(path)).first->second;
  }
}

asset::StaticMeshFileHandle &
asset::LibraryFileHandleBuffer::_getStaticMeshFileHandle(MeshID id)
{
  std::string path;
  for (const LibraryAssetRef & ref : _assetRefs) {
    if (ref.assetID == id && ref.assetType == AssetType::StaticMesh) {
      path = std::string(_pathData.data() + ref.pathOffset);
    }
  }

  if (path.empty()) {
    std::cerr << "Can't find mesh with ID " << id << " in asset library." << std::endl;
    std::exit(-1);
  }

  return _getStaticMeshFileHandle(path);
}

asset::StaticMeshData *asset::LibraryFileHandleBuffer::getMeshData(MeshID id) {
  auto &handle = _getStaticMeshFileHandle(id);

  return handle->getMeshData(id);
}

bool asset::LibraryFileHandleBuffer::readMesh(MeshID id, StaticVertexData *verts, uint16_t *indices) {
  auto &handle = _getStaticMeshFileHandle(id);

  return handle->readMesh(id, verts, indices);
}
