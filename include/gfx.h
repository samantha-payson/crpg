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

#ifndef CRPG_GFX_H
#define CRPG_GFX_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <array>
#include <set>
#include <optional>
#include <algorithm>
#include <initializer_list>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "vk_mem_alloc.h"

#include "asset.h"

namespace gfx {

  struct Buffer {
    VkBuffer       buffer;
    VmaAllocation  alloc;
  };

  struct Mesh {
    asset::StaticMeshData  meshData;

    Buffer  vbuffer;
    Buffer  ibuffer;
  };

  struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
  };

  struct PerFrame {
    VkSemaphore      imageAcquiredSem    { VK_NULL_HANDLE };
    VkSemaphore      renderFinishedSem   { VK_NULL_HANDLE };
    VkFence          renderFinishedFence { VK_NULL_HANDLE };
    VkCommandPool    commandPool         { VK_NULL_HANDLE };
  };

  struct PerSwapImage {
    VkImage        image;
    VkImageView    imageView;
    VkImage        depth;
    VkImageView    depthView;
    VmaAllocation  depthAlloc;
    VkFramebuffer  framebuf;
  };

  struct MeshPass {
    enum {
      Transparency,
      DirectionalShadow,
      Forward,
      MAX,
    };
  };

  template <typename T>
  using PerPassData = std::array<T, MeshPass::MAX>;

  struct ShaderStage {
    VkShaderModule         module;
    VkShaderStageFlagBits  stage;
  };

  struct ShaderEffect {
    VkDescriptorSetLayout     descLayout;
    VkPipelineLayout          layout;
    std::vector<ShaderStage>  stages;
  };

  struct ShaderPass {
    ShaderEffect      *effect   =  nullptr;
    VkPipeline        pipeline  =  VK_NULL_HANDLE;
    VkPipelineLayout  layout    =  VK_NULL_HANDLE;
  };

  struct Material {
    PerPassData<ShaderPass*>      shaders;
    PerPassData<VkDescriptorSet>  descriptorSets;
  };

  class Engine {
  public:
    Engine(std::initializer_list<char const *> enabledLayers)
      : _enabledLayers(enabledLayers) {}

    void init();
    void draw();
    void cleanup();

    size_t framesDrawn() { return _framesDrawn; }

  private:
    void _allocBuffer(size_t              size,
		      VkBufferUsageFlags  vkUsage,
		      VmaMemoryUsage      memoryUsage,
		      Buffer              *buffer);

    void _freeBuffer(Buffer *buffer);

    bool _loadMesh(std::string const &path, asset::MeshID id, Mesh *mesh);

    void _freeMesh(Mesh *mesh);

    void _initPerFrames();

    void _initPipelines();

    // Advance _currentFrame and do the vkWaitForFences-dance to acquire the
    // next frame, then return a pointer to its corresponding PerFrame
    // structure.
    PerFrame *_acquireNextFrame();

    VkCommandBuffer _allocCmdBuffer(VkCommandPool pool);

    // Shorthands for recording commands.
    void _beginCmdBuffer(VkCommandBuffer  cmdBuf,
			 uint32_t         flags = 0);

    void _beginRenderPass(VkCommandBuffer  cmdBuf,
			  VkFramebuffer    framebuf,
			  VkClearValue     *clearValues,
			  uint32_t         clearCount);


    // Helper methods for building pipelines
    VkPipelineShaderStageCreateInfo
    _loadShaderStageInfo(const std::string &spirvPath, VkShaderStageFlagBits stageFlags);

    VkPipelineVertexInputStateCreateInfo
    _vertexInputInfo();

    VkPipelineInputAssemblyStateCreateInfo
    _inputAssemblyInfo(VkPrimitiveTopology topology);

    VkPipelineRasterizationStateCreateInfo
    _rasterStateInfo(VkPolygonMode polyMode);

    VkPipelineMultisampleStateCreateInfo
    _multisampleInfo();

    VkPipelineColorBlendAttachmentState
    _colorBlendAttachState();

    VkPipelineDepthStencilStateCreateInfo
    _depthStencilState(bool depthTest, bool depthWrite, VkCompareOp cmp);

    VkPushConstantRange
    _pushConstantRange();

    VkPipelineLayoutCreateInfo
    _pipelineLayoutInfo(VkPushConstantRange *pushRange);

    VkImageCreateInfo
    _imageInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

    VkImageViewCreateInfo
    _imageViewInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

    ////////////////////////////////////
    // For one-off testing, not part of the final form of this class..
    //
    void _initTestData();
    void _cleanupTestData();
    //
    Mesh _testMesh;
    //
    // End test decls
    ////////////////////////////////////
    
    bool _initialized = false;

    static constexpr size_t  MAX_FRAMES_IN_FLIGHT { 2 };

    std::vector<char const *>  _enabledLayers;

    // This is an index into _perFrames, it should always be
    //
    //         0 <= _currentFrame < MAX_FRAMES_IN_FLIGHT
    //
    size_t  _currentFrame { 0 };

    // This is just the number of times that .draw() has successfully been
    // called on this Engine.
    //
    // On a 32-bit system w/ a 60Hz refresh rate, this will take more than 2
    // years to overflow. On a 64-bit system it should be more like 9 billion
    // years.
    //
    // Both are acceptable =]
    size_t _framesDrawn { 0 };

    SDL_Window        *_window;
    VkInstance        _instance;
    VkPhysicalDevice  _physicalDevice;
    VkDevice          _device;
    VkSurfaceKHR      _surface;

    VkExtent2D     _swapExtent;
    VkFormat       _swapFormat;
    VkSwapchainKHR _swapChain;

    VkFormat _depthFormat;

    std::vector<PerSwapImage>  _perSwaps;

    std::array<PerFrame, MAX_FRAMES_IN_FLIGHT>  _perFrames;

    std::optional<uint32_t>  _graphicsFamily;
    std::optional<uint32_t>  _presentFamily;
    VkQueue                  _graphicsQueue;
    VkQueue                  _presentQueue;

    // These are used for issuing commands that are independent of any frame.
    //
    // This pool has VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT set,
    // whereas the frame pools don't.
    VkCommandPool    _globalCommandPool;
    VkCommandBuffer  _globalCommandBuffer;

    VkRenderPass      _renderPass;
    VkPipelineLayout  _pipelineLayout;
    VkPipeline        _graphicsPipeline;

    VmaAllocator  _allocator;
  };

  /// The following three classes are taken more-or-less directly from vkguide.dev
  ///
  ///     DescriptorAllocator   - Simplifies VkDescriptorSet allocation by abstracting
  ///                             over descriptor pools.
  ///
  ///     DescriptorLayoutCache - Caches VkDescriptorSetLayouts, like it says on the
  ///                             tin. Prevents us from creating duplicate layouts.
  ///
  ///     DescriptorBuilder     - Simplifies building VkDescriptorSetLayouts with a
  ///                             much more to-the-point interface.
  ///
  /// Like everything described in this header, implementations are found in gfx.cc

  class DescriptorAllocator {
  public:
    struct PoolSizes {
      std::vector<std::pair<VkDescriptorType, float>> sizes {
	{ VK_DESCRIPTOR_TYPE_SAMPLER,                 0.5f },
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  4.f  },
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           4.f  },
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           1.f  },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,    1.f  },
	{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,    1.f  },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          2.f  },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          2.f  },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,  1.f  },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,  1.f  },
	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,        0.5f },
      };
    };

    void resetPools();
    bool allocate(VkDescriptorSet *set, VkDescriptorSetLayout layout);
    void init(VkDevice device);
    void cleanup();

    VkDevice _device;

  private:
    VkDescriptorPool createPool(size_t count, VkDescriptorPoolCreateFlags flags);
    VkDescriptorPool grabPool();

    VkDescriptorPool _currentPool = VK_NULL_HANDLE;
    PoolSizes        _descriptorSizes;

    std::vector<VkDescriptorPool> _usedPools;
    std::vector<VkDescriptorPool> _freePools;
  };

  class DescriptorLayoutCache {
  public:
    void init(VkDevice device);
    void cleanup();

    VkDescriptorSetLayout createDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info);

    struct DescriptorLayoutInfo {
      std::vector<VkDescriptorSetLayoutBinding> bindings;
      bool operator == (const DescriptorLayoutInfo & other) const;

      size_t hash() const;
    };

  private:
    struct DescriptorLayoutHash {
      size_t operator()(const DescriptorLayoutInfo &k) const {
	return k.hash();
      };
    };

    std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash>
        _layoutCache;

    VkDevice  _device;
  };

  class DescriptorBuilder {
  public:
    static DescriptorBuilder begin(
      DescriptorLayoutCache  *layoutCache,
      DescriptorAllocator    *allocator);

    DescriptorBuilder &bind_buffer(
      uint32_t                binding,
      VkDescriptorBufferInfo  *bufferInfo,
      VkDescriptorType        type,
      VkShaderStageFlags      stageFlags);

    DescriptorBuilder &bind_image(
      uint32_t               binding,
      VkDescriptorImageInfo  *bufferInfo,
      VkDescriptorType       type,
      VkShaderStageFlags     stageFlags);

    bool build(VkDescriptorSet &set, VkDescriptorSetLayout &layout);
    bool build(VkDescriptorSet &set);

  private:
    std::vector<VkWriteDescriptorSet>          _writes;
    std::vector<VkDescriptorSetLayoutBinding>  _bindings;

    DescriptorLayoutCache  *_cache;
    DescriptorAllocator    *_allocator;
  };
}

#endif // gfx.h
