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

#ifndef CRPG_ASSET_H
#define CRPG_ASSET_H

#include <cstdint>
#include <cstring>

#include <fstream>
#include <iostream>

#include <memory>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "util.h"

namespace asset {
  typedef uint32_t  AssetID;
  typedef AssetID   MeshID;
  typedef AssetID   TextureID;

  enum class AssetType : uint32_t {
    StaticMesh  = 0,
    Texture     = 1,
  };

  constexpr uint32_t NULL_ASSET_ID = 0;

  struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription>    bindings;
    std::vector<VkVertexInputAttributeDescription>  attribs;
    VkPipelineInputAssemblyStateCreateFlags         flags;

    VkPipelineVertexInputStateCreateInfo
    vertexInputInfo() {
      VkPipelineVertexInputStateCreateInfo info = {
	.sType  = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	.pNext  = nullptr,
	.flags  = flags,

	.vertexBindingDescriptionCount  = (uint32_t)bindings.size(),
	.pVertexBindingDescriptions     = bindings.data(),

	.vertexAttributeDescriptionCount  = (uint32_t)attribs.size(),
	.pVertexAttributeDescriptions     = attribs.data(),
      };

      return info;
    }
  };

  struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
  };

  struct StaticVertexData {
    glm::vec3  position;
    glm::vec2  uv;
    glm::vec3  normal;
    glm::vec3  tangent;

    static VertexInputDescription vertexInputDescription();
  };

  struct StaticMeshData {
    BoundingBox  bounds;
    MeshID       id;
    TextureID    color         { NULL_ASSET_ID };
    TextureID    normal        { NULL_ASSET_ID };
    TextureID    roughness     { NULL_ASSET_ID };
    TextureID    occlusion     { NULL_ASSET_ID };
    TextureID    emission      { NULL_ASSET_ID };
    uint32_t     vertexOffset;
    uint32_t     vertexCount;
    uint32_t     indexOffset;
    uint32_t     indexCount;
  };

  struct StaticMeshFileHeader {
    static constexpr char const *MAGIC_NUMBER = "crpg:asset:static-mesh";
    char      magicNumber[32];
    uint32_t  meshCount;
    uint32_t  vertexCount;
    uint32_t  indexCount;

    StaticMeshFileHeader() {
      strcpy(magicNumber, MAGIC_NUMBER);
    }
  };

  struct TextureData {
    TextureID  id;
    uint16_t   width;
    uint16_t   height;
    uint16_t   components;
    uint32_t   offset; // in bytes
  };

  void writeStaticMeshFile(char const             *path,
			   const StaticMeshData   *meshes,  uint32_t meshCount,
			   const StaticVertexData *verts,   uint32_t vertCount,
			   const uint16_t         *indices, uint32_t indexCount);

  class StaticMeshFileHandleBuffer;

  using StaticMeshFileHandle = std::unique_ptr<StaticMeshFileHandleBuffer>;

  StaticMeshFileHandle openStaticMeshFile(std::string const &path);

  // This is the structure that backs a StaticMeshFileHandle. It holds all the
  // data that's necessary to load meshes from a static mesh file without
  class StaticMeshFileHandleBuffer {
  public:
    ~StaticMeshFileHandleBuffer();

    friend std::ostream & operator <<(std::ostream &os,
				      const StaticMeshFileHandle &handle);


    friend StaticMeshFileHandle openStaticMeshFile(std::string const &path);

    StaticMeshData *getMeshData(MeshID id);

    bool readMesh(MeshID id, StaticVertexData *verts, uint16_t *indices);

  private:
    size_t _vertexOffsetToBytes(size_t vertOffset) const;
    size_t _indexOffsetToBytes(size_t indexOffset) const;
    
    std::ifstream                _stream;
    StaticMeshFileHeader         _header;
    std::vector<StaticMeshData>  _meshes;
  };

  struct TextureFileHeader {
    static constexpr char const *MAGIC_NUMBER = "crpg:asset:texture";
    char      magicNumber[32];
    uint32_t  textureCount;
    uint32_t  sampleCount;

    TextureFileHeader() {
      strcpy(magicNumber, MAGIC_NUMBER);
    }
  };

  struct LibraryAssetRef {
    AssetID    assetID;
    AssetType  assetType;
    uint32_t   pathOffset;
  };

  struct LibraryFileHeader {
    static constexpr char const *MAGIC_NUMBER = "crpg:asset:library";
    char     magicNumber[32];
    uint32_t assetRefCount;
    uint32_t pathByteCount;

    LibraryFileHeader() {
      strcpy(magicNumber, MAGIC_NUMBER);
    }
  };

  class LibraryFileHandleBuffer;

  using LibraryFileHandle = std::unique_ptr<LibraryFileHandleBuffer>;

  LibraryFileHandle openLibraryFile(std::string const &path);

  LibraryFileHandle emptyLibraryFileHandle();

  class LibraryFileHandleBuffer {
  public:
    friend std::ostream & operator <<(std::ostream &os,
				      const LibraryFileHandle &handle);

    void write(std::string const &path) const;


    friend LibraryFileHandle openLibraryFile(std::string const & path);

    void addMeshRef(MeshID id, const std::string &path);

    StaticMeshData *getMeshData(MeshID id);

    bool readMesh(MeshID id, StaticVertexData *verts, uint16_t *indices);

    bool getMultiMeshData(MeshID *ids, StaticMeshData *data, size_t count);

    bool readMultiMesh(MeshID *ids, size_t count, StaticVertexData *verts, uint16_t *indices);

  private:
    StaticMeshFileHandle &_getStaticMeshFileHandle(const std::string &path);
    StaticMeshFileHandle &_getStaticMeshFileHandle(MeshID id);

    std::unordered_map<std::string, StaticMeshFileHandle> _staticMeshHandleCache;

    std::vector<LibraryAssetRef>  _assetRefs;
    std::vector<char>             _pathData;
  };
};

#endif
