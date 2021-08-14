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

#include <fstream>
#include <iostream>

#include <memory>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace asset {
  typedef uint32_t  AssetID;
  typedef AssetID   MeshID;
  typedef AssetID   TextureID;

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
    char      magicNumber[32] = "crpg:asset:static-mesh";
    uint32_t  meshCount;
    uint32_t  vertexCount;
    uint32_t  indexCount;
  };

  struct TextureData {
    TextureID  id;
    uint16_t   width;
    uint16_t   height;
    uint16_t   components;
    uint32_t   offset; // in bytes
  };

  struct TextureFileHeader {
    char      magicNumber[32] = "crpg:asset:texture";
    uint32_t  textureCount;
    uint32_t  sampleCount;
  };

  struct LibraryFileRef {
    AssetID  assetID;
    char     path[124];
  };

  struct LibraryHeader {
    char     magicNumber[32] = "crpg:asset:library";
    uint32_t fileRefCount;
  };

  void writeStaticMeshFile(char const             *path,
			   const StaticMeshData   *meshes,  uint32_t meshCount,
			   const StaticVertexData *verts,   uint32_t vertCount,
			   const uint16_t         *indices, uint32_t indexCount);

  class StaticMeshFileHandleBuffer;

  using StaticMeshFileHandle = std::unique_ptr<StaticMeshFileHandleBuffer>;

  StaticMeshFileHandle openStaticMeshFile(char const *path);

  class StaticMeshFileHandleBuffer {
  public:
    ~StaticMeshFileHandleBuffer();

    friend std::ostream & operator <<(std::ostream &os,
				      const StaticMeshFileHandle &handle);


    friend StaticMeshFileHandle openStaticMeshFile(char const *path);

    StaticMeshData *getMeshData(MeshID id);

    bool readMesh(MeshID id, StaticVertexData *verts, uint16_t *indices);

    void close();

  private:
    size_t _vertexOffsetToBytes(size_t vertOffset) const;
    size_t _indexOffsetToBytes(size_t indexOffset) const;
    
    std::ifstream                _stream;
    StaticMeshFileHeader         _header;
    std::vector<StaticMeshData>  _meshes;

    // This is used to avoid calling close() twice on the ifstream.
    bool _isOpen = false;
  };
};

#endif
