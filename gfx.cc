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

#include "gfx.h"

#include <glm/gtx/transform.hpp>

// If `n` is a multiple of `nearest`, return `n`. Otherwise return the
// next highest multiple of `nearest`
static inline size_t
roundUp(size_t n, size_t nearest) {
  return ((n + (nearest - 1)) / nearest) * nearest;
}

// Read the entire contents of the file at `path` into a vector and
// return that vector.
//
// Note that this is intended only for reading SPIR-V files, it will
// round up the byte-size of the file to the nearest multiple of 4 and
// pad the end of the vector accordingly.
//
// Prints a message and calls `std::exit(-1)` on failure.
static std::vector<char>
readFile(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "failed to open file '" << path << "'" << std::endl;
    std::exit(-1);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  size_t roundedFileSize = roundUp(fileSize, 4);

  std::vector<char> buffer(roundedFileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

// Read SPIR-V from the file at `spirvPath`, create a VkShaderModule for the
// stage described by `stageBit`, and return a VkPipelinShaderStageCreateInfo
// struct referencing that module.
//
// You must eventually call `vkDestroyShaderModule` on the .module field of the
// returned structure. Currently this happens at the end of `_initPipelines`.
//
// Log and exit on failure.
VkPipelineShaderStageCreateInfo
gfx::Engine::_loadShaderStageInfo(const std::string &spirvPath, VkShaderStageFlagBits stageBit) {
  auto spirv = readFile(spirvPath);

  VkShaderModuleCreateInfo createInfo = {
    .sType  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext  = nullptr,

    .codeSize  = spirv.size(),
    .pCode     = reinterpret_cast<uint32_t const*>(spirv.data()),
  };

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    std::cerr << "failed to create shader module!" << std::endl;
    std::exit(-1);
  }

  VkPipelineShaderStageCreateInfo stageInfo = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext  = nullptr,

    .stage   = stageBit,
    .module  = shaderModule,
    .pName   = "main",
  };

  return stageInfo;
}

// Abstract away the construction of a VkPipelineInputAssemblyStateCreateInfo
//
// Used in _initPipelines
VkPipelineInputAssemblyStateCreateInfo
gfx::Engine::_inputAssemblyInfo(VkPrimitiveTopology topology) {
  VkPipelineInputAssemblyStateCreateInfo info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext  = nullptr,

    .topology                = topology,
    .primitiveRestartEnable  = VK_FALSE,
  };

  return info;
}

// Abstract away the construction of a VkPipelineRasterizationStateCreateInfo
//
// Used in _initPipelines
VkPipelineRasterizationStateCreateInfo
gfx::Engine::_rasterStateInfo(VkPolygonMode polyMode) {
  VkPipelineRasterizationStateCreateInfo info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext  = nullptr,

    .depthClampEnable         = VK_FALSE,
    .rasterizerDiscardEnable  = VK_FALSE,

    .polygonMode  = polyMode,
    .lineWidth    = 1.0f,

    .cullMode   = VK_CULL_MODE_NONE,
    .frontFace  = VK_FRONT_FACE_CLOCKWISE,

    .depthBiasEnable          = VK_FALSE,
    .depthBiasConstantFactor  = 0.0f,
    .depthBiasClamp           = 0.0f,
    .depthBiasSlopeFactor     = 0.0f,
  };

  return info;
}

// Loads a mesh into Vulkan buffers and writes the corresponding handles (etc)
// into the structure pointed to by `mesh`.
bool
gfx::Engine::_loadMesh(std::string const &path, asset::MeshID id, gfx::Mesh *mesh) {
  auto handle    = asset::openMeshFile(path.c_str());
  auto meshData  = asset::getMeshData(handle, id);

  if (!meshData) return false;

  _allocBuffer(meshData->vertexCount*sizeof(asset::StaticVertexData),
	       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	       VMA_MEMORY_USAGE_CPU_TO_GPU,
	       &mesh->vbuffer);

  _allocBuffer(meshData->indexCount*sizeof(uint16_t),
	       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	       VMA_MEMORY_USAGE_CPU_TO_GPU,
	       &mesh->ibuffer);

  mesh->meshData = *meshData;

  asset::StaticVertexData  *verts;
  uint16_t                 *indices;

  vmaMapMemory(_allocator, mesh->vbuffer.alloc, (void **)&verts);
  vmaMapMemory(_allocator, mesh->ibuffer.alloc, (void **)&indices);

  asset::readMesh(handle, id, verts, indices);

  vmaUnmapMemory(_allocator, mesh->vbuffer.alloc);
  vmaUnmapMemory(_allocator, mesh->ibuffer.alloc);

  asset::closeMeshFile(handle);

  return true;
}

// Allocate a Buffer with the given parameters using the VmaAllocator
//
// Log and exit on failure.
void gfx::Engine::_allocBuffer(size_t              size,
			       VkBufferUsageFlags  vkUsage,
			       VmaMemoryUsage      memoryUsage,
			       gfx::Buffer         *buffer)
{
  VkBufferCreateInfo bufferInfo = {
    .sType  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext  = nullptr,

    .size   = size,
    .usage  = vkUsage,
  };

  VmaAllocationCreateInfo vmaAllocInfo = {};

  vmaAllocInfo.usage = memoryUsage;

  if (vmaCreateBuffer(_allocator,
		      &bufferInfo,
		      &vmaAllocInfo,
		      &buffer->buffer,
		      &buffer->alloc,
		      nullptr) != VK_SUCCESS)
  {
    std::cerr << "Failed to allocate a buffer of size " << size << std::endl;
    std::exit(-1);
  }
}


// Free a buffer allocated by _allocBuffer
void gfx::Engine::_freeBuffer(Buffer *buf) {
  vmaDestroyBuffer(_allocator, buf->buffer, buf->alloc);
}

// Free a mesh allocated by _loadMesh
void gfx::Engine::_freeMesh(Mesh *mesh) {
  _freeBuffer(&mesh->vbuffer);
  _freeBuffer(&mesh->ibuffer);
}

// Abstract the creation of VkPipelineMultisampleStateCreateInfo
//
// Used in _initPipelines
VkPipelineMultisampleStateCreateInfo
gfx::Engine::_multisampleInfo() {
  VkPipelineMultisampleStateCreateInfo info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext  = nullptr,

    .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,

    .sampleShadingEnable  = VK_FALSE,
    .minSampleShading     = 1.0f,
    .pSampleMask          = nullptr,

    .alphaToCoverageEnable  = VK_FALSE,
    .alphaToOneEnable       = VK_FALSE,
  };

  return info;
}


// Abstract the creation of VkPipelineColorBlendAttachmentState
//
// Used in _initPipelines
VkPipelineColorBlendAttachmentState
gfx::Engine::_colorBlendAttachState() {
  VkPipelineColorBlendAttachmentState state = {
    .colorWriteMask  = VK_COLOR_COMPONENT_R_BIT
                     | VK_COLOR_COMPONENT_G_BIT
                     | VK_COLOR_COMPONENT_B_BIT
                     | VK_COLOR_COMPONENT_A_BIT,

    .blendEnable  = VK_FALSE,
  };

  return state;
}

// Abstract the creation of VkPipelineDepthStencilStateCreateInfo
//
// Used in _initPipelines
VkPipelineDepthStencilStateCreateInfo
gfx::Engine::_depthStencilState(bool depthTest, bool depthWrite, VkCompareOp cmp) {
  VkPipelineDepthStencilStateCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,

    .depthTestEnable  = (depthTest ? VK_TRUE : VK_FALSE),
    .depthWriteEnable = (depthWrite ? VK_TRUE : VK_FALSE),
    .depthCompareOp   = (depthTest ? cmp : VK_COMPARE_OP_ALWAYS),

    .depthBoundsTestEnable = VK_FALSE,
    .minDepthBounds        = 0.0f,
    .maxDepthBounds        = 1.0f,
    .stencilTestEnable     = VK_FALSE,
  };

  return info;
}

// Abstract the creation of VkPushConstantRange
//
// Used in _initPipelines
VkPushConstantRange
gfx::Engine::_pushConstantRange() {
  VkPushConstantRange range = {
    .offset = 0,
    .size   = sizeof(MeshPushConstants),

    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  return range;
}

// Abstract the creation of VkPipelineLayoutCreateInfo
//
// Used in _initPipelines
VkPipelineLayoutCreateInfo
gfx::Engine::_pipelineLayoutInfo(VkPushConstantRange *range) {
  VkPipelineLayoutCreateInfo info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext  = nullptr,
    .flags  = 0,

    .setLayoutCount  = 0,
    .pSetLayouts     = nullptr,

    .pushConstantRangeCount  = 1,
    .pPushConstantRanges     = range,
  };

  return info;
}

// Here is where we actually initialize our pipelines. Right now there's just
// one pipeline for rendering static meshes.
//
// Log and exit on failure.
void gfx::Engine::_initPipelines() {
  auto pushRange   = _pushConstantRange();
  auto layoutInfo  = _pipelineLayoutInfo(&pushRange);
  if (vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
    std::cerr << "Failed to create pipeline layout." << std::endl;
    std::exit(-1);
  }

  auto assemblyInfo  = _inputAssemblyInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  auto rasterInfo    = _rasterStateInfo(VK_POLYGON_MODE_FILL);
  auto msInfo        = _multisampleInfo();

  auto colorBlendState  = _colorBlendAttachState();

  auto vertStageInfo = _loadShaderStageInfo(".data/static-mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
  auto fragStageInfo = _loadShaderStageInfo(".data/static-mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  std::vector shaderStages {
    vertStageInfo,
    fragStageInfo,
  };

  auto vertInputDesc = asset::StaticVertexData::vertexInputDescription();
  auto vertInputInfo = vertInputDesc.vertexInputInfo();

  auto depthStencil = _depthStencilState(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

  VkViewport viewport = {
    .x  = 0,
    .y  = 0,

    .width   = (float)_swapExtent.width,
    .height  = (float)_swapExtent.height,

    .minDepth  = 0.0f,
    .maxDepth  = 1.0f,
  };

  VkRect2D scissor = {
    .offset  = { 0, 0 },
    .extent  = _swapExtent,
  };

  VkPipelineViewportStateCreateInfo viewportInfo = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext  = nullptr,

    .viewportCount  = 1,
    .pViewports     = (&viewport),

    .scissorCount  = 1,
    .pScissors     = (&scissor),
  };

  VkPipelineColorBlendStateCreateInfo blendInfo = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext  = nullptr,

    .logicOpEnable  = VK_FALSE,
    .logicOp        = VK_LOGIC_OP_COPY,

    .attachmentCount = 1,
    .pAttachments    = (&colorBlendState),
  };

  VkGraphicsPipelineCreateInfo pipelineInfo = {
    .sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext  = nullptr,

    .stageCount  = (uint32_t)shaderStages.size(),
    .pStages     = shaderStages.data(),

    .pVertexInputState   = (&vertInputInfo),
    .pInputAssemblyState = (&assemblyInfo),
    .pViewportState      = (&viewportInfo),
    .pRasterizationState = (&rasterInfo),
    .pMultisampleState   = (&msInfo),
    .pColorBlendState    = (&blendInfo),
    .pDepthStencilState  = (&depthStencil),
    .layout              = _pipelineLayout,
    .renderPass          = _renderPass,
    .subpass             = 0,
    .basePipelineHandle  = VK_NULL_HANDLE,
  };

  if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline)
      != VK_SUCCESS)
    {
      std::cerr << "Failed to construct graphics pipeline!" << std::endl;
      std::exit(-1);
    }

  // IMPORTANT it's safe to delete VkShaderModule's after the pipeline is built,
  // we need to make sure they're all deleted here to avoid a leak.
  for (auto &shinfo : shaderStages) {
    vkDestroyShaderModule(_device, shinfo.module, nullptr);
  }
}

// Abstract the construction of VkImageCreateInfo
//
// Used in various places where images are constructed. Mostly in
// gfx::Engine::init() at the moment.
VkImageCreateInfo
gfx::Engine::_imageInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent) {
  VkImageCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,

    .imageType = VK_IMAGE_TYPE_2D,

    .format = format,
    .extent = extent,

    .mipLevels   = 1,
    .arrayLayers = 1,

    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling  = VK_IMAGE_TILING_OPTIMAL,
    .usage   = usageFlags,
  };

  return info;
}

// Abstract the construction of VkImageCreateInfo
//
// Used in various places where image views are constructed. Mostly in
// gfx::Engine::init() at the moment.
VkImageViewCreateInfo
gfx::Engine::_imageViewInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) {
  VkImageViewCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,

    .image    = image,

    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format   = format,
    .subresourceRange = {
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1,
      .aspectMask     = aspectFlags,
    },
  };

  return info;
}

// Monster function where SDL and Vulkan are initialized, and all
// graphics-related systems with the same lifetime as the engine are
// initialized.
//
// This includes building pipelines and initializing per-frame synchronization
// primitives.
//
// Log and exit on failure.
void gfx::Engine::init() {
  SDL_Init(SDL_INIT_EVERYTHING);

  _window = SDL_CreateWindow(
    "crpg", 0, 0, 1920, 1080, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

  uint32_t extensionCount = 0;

  SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, nullptr);

  std::vector<char const*> extensionNames(extensionCount);

  SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, extensionNames.data());

  for (auto const & ext : extensionNames) {
    std::cout << "Extension '" << ext << "' supported" << std::endl;
  }

  VkApplicationInfo appInfo {
    .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName   = "crpg",
    .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
    .pEngineName        = "crpg",
    .engineVersion      = VK_MAKE_VERSION(0, 0, 1),
    .apiVersion         = VK_API_VERSION_1_2,
  };

  VkInstanceCreateInfo createInfo {
    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo        = &appInfo,
    .enabledExtensionCount   = extensionCount,
    .ppEnabledExtensionNames = extensionNames.data(),
    .enabledLayerCount       = static_cast<uint32_t>(_enabledLayers.size()),
    .ppEnabledLayerNames     = _enabledLayers.data(),
  };

  if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS) {
    std::cerr << "failed to create instance!" << std::endl;
    std::exit(-1);
  }

  uint32_t layerCount;

  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);

  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  std::cout << "Available layers:" << std::endl;
  for (auto & layer : availableLayers) {
    std::cout << "    '" << layer.layerName << "'" << std::endl;
  }
  std::cout << std::endl;

  uint32_t physicalDeviceCount = 0;

  vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);

  if (physicalDeviceCount == 0) {
    std::cerr << "No Devices with Vulkan Support!" << std::endl;
    std::exit(-1);
  }

  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

  vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices.data());

  std::cout << "Available devices: " << std::endl;
  for (const auto & candidate : physicalDevices) {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures   feats;

    vkGetPhysicalDeviceProperties(candidate, &props);
    vkGetPhysicalDeviceFeatures(candidate, &feats);

    std::cout << "device '"<<  props.deviceName << "' {" << std::endl;
    std::cout << "  properties {" << std::endl;
    std::cout << "    // TODO ..." << std::endl;
    std::cout << "  }" << std::endl;
    std::cout << "  features {" << std::endl;
    std::cout << "    alphaToOne:               "
	      << (feats.alphaToOne ? "true" : "false") << std::endl;
    std::cout << "    depthBiasClamp:           "
	      << (feats.depthBiasClamp ? "true" : "false") << std::endl;
    std::cout << "    depthBounds:              "
	      << (feats.depthBounds ? "true" : "false") << std::endl;
    std::cout << "    depthClamp:               "
	      << (feats.depthClamp ? "true" : "false") << std::endl;
    std::cout << "    dualSrcBlend:             "
	      << (feats.dualSrcBlend ? "true" : "false") << std::endl;
    std::cout << "    fillModeNonSolid:         "
	      << (feats.fillModeNonSolid ? "true" : "false") << std::endl;
    std::cout << "    fragmentStoresAndAtomics: "
	      << (feats.fragmentStoresAndAtomics ? "true" : "false") << std::endl;
    std::cout << "    fullDrawIndexUint32:      "
	      << (feats.fullDrawIndexUint32 ? "true" : "false") << std::endl;
    std::cout << "    geometryShader:           "
	      << (feats.geometryShader ? "true" : "false") << std::endl;
    std::cout << "    imageCubeArray:           "
	      << (feats.imageCubeArray ? "true" : "false") << std::endl;
    std::cout << "    independentBlend:         "
	      << (feats.independentBlend ? "true" : "false") << std::endl;
    std::cout << "    inheritedQueries:         "
	      << (feats.inheritedQueries ? "true" : "false") << std::endl;
    std::cout << "    largePoints:              "
	      << (feats.largePoints ? "true" : "false") << std::endl;
    std::cout << "    logicOp:                  "
	      << (feats.logicOp ? "true" : "false") << std::endl;
    std::cout << "    multiDrawIndirect:        "
	      << (feats.multiDrawIndirect ? "true" : "false") << std::endl;
    std::cout << "    multiViewport:            "
	      << (feats.multiViewport ? "true" : "false") << std::endl;
    std::cout << "    occlusionQueryPrecise:    "
	      << (feats.occlusionQueryPrecise ? "true" : "false") << std::endl;
    std::cout << "    pipelineStatisticsQuery:  "
	      << (feats.pipelineStatisticsQuery ? "true" : "false") << std::endl;
    std::cout << "    robustBufferAccess:       "
	      << (feats.robustBufferAccess ? "true" : "false") << std::endl;
    std::cout << "  }" << std::endl;
    std::cout << "}" << std::endl;
  }


  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

  _physicalDevice = physicalDevices[0];

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(
    _physicalDevice,
    &queueFamilyCount,
    queueFamilies.data()
    );

  int i = 0;
  for (auto const & qf : queueFamilies) {
    if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      _graphicsFamily = i;
    }
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, _surface, &presentSupport);
    if (presentSupport) {
      _presentFamily = i;
    }

    i++;
  }

  if (!_graphicsFamily.has_value()) {
    std::cerr << "No Queue Family capable of Graphics!" << std::endl;
    std::exit(-1);
  }

  float queuePriority = 1.0f;

  std::set<uint32_t> uniqueFamilies = {
    _graphicsFamily.value(),
    _presentFamily.value(),
  };

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

  for (uint32_t queueIdx : uniqueFamilies) {
    queueCreateInfos.push_back(VkDeviceQueueCreateInfo {
	.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
	.queueFamilyIndex = queueIdx,
	.queueCount       = 1,
	.pQueuePriorities = &queuePriority,
      });
  }

  VkPhysicalDeviceFeatures  deviceFeatures {};
  std::vector<char const *> deviceExtensions {
    "VK_KHR_swapchain",
  };

  VkDeviceCreateInfo deviceCreateInfo {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pQueueCreateInfos       = queueCreateInfos.data(),
    .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
    .pEnabledFeatures        = &deviceFeatures,
    .enabledLayerCount       = static_cast<uint32_t>(_enabledLayers.size()),
    .ppEnabledLayerNames     = _enabledLayers.data(),
    .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
    .ppEnabledExtensionNames = deviceExtensions.data(),
  };

  if (vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS) {
    std::cerr << "Failed to create logical device" << std::endl;
    std::exit (-1);
  }

  VmaAllocatorCreateInfo allocatorInfo = {};

  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
  allocatorInfo.physicalDevice   = _physicalDevice;
  allocatorInfo.device           = _device;
  allocatorInfo.instance         = _instance;

  vmaCreateAllocator(&allocatorInfo, &_allocator);

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
  };

  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      _physicalDevice,
      _surface,
      &formatCount,
      details.formats.data());
  }


  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      _physicalDevice,
      _surface,
      &presentModeCount,
      details.presentModes.data());
  }

  if (details.presentModes.empty() || details.formats.empty()) {
    std::cerr << "swap  chain is inadequate!" << std::endl;
  }

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

  // for (const auto &pm : details.presentModes) {
  //   if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
  // 	presentMode = pm;
  // 	break;
  //   }
  // }

  VkSurfaceFormatKHR format = details.formats[0];

  for (const auto &fmt : details.formats) {
    if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
	fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      format = fmt;
    }
  }

  VkExtent2D swapExtent;

  if (details.capabilities.currentExtent.width != UINT32_MAX) {
    swapExtent = details.capabilities.currentExtent;
  } else {
    int width, height;

    SDL_Vulkan_GetDrawableSize(_window, &width, &height);

    swapExtent = { (uint32_t)width, (uint32_t)height };

    auto const & caps = details.capabilities;

    swapExtent.width = std::max(
      caps.minImageExtent.width,
      std::min(caps.maxImageExtent.width, swapExtent.width));

    swapExtent.height = std::max(
      caps.minImageExtent.height,
      std::min(caps.maxImageExtent.height, swapExtent.height));
  }

  uint32_t imageCount = std::min(
    details.capabilities.minImageCount + 1,
    details.capabilities.maxImageCount == 0 ? UINT32_MAX
    : details.capabilities.maxImageCount);

  std::set<uint32_t> familySet {
    _graphicsFamily.value(),
    _presentFamily.value(),
  };

  std::vector<uint32_t> familyIndices(familySet.begin(), familySet.end());

  VkSwapchainCreateInfoKHR swapCreateInfo {
    .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface               = _surface,
    .minImageCount         = imageCount,
    .imageFormat           = format.format,
    .imageColorSpace       = format.colorSpace,
    .imageExtent           = swapExtent,
    .imageArrayLayers      = 1,
    .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode      = familyIndices.empty() ? VK_SHARING_MODE_CONCURRENT
    : VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = familyIndices.empty() ? 0
    : (uint32_t)familyIndices.size(),
    .pQueueFamilyIndices   = familyIndices.empty() ? nullptr
    : familyIndices.data(),
    .preTransform          = details.capabilities.currentTransform,
    .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode           = presentMode,
    .clipped               = VK_TRUE,
    .oldSwapchain          = VK_NULL_HANDLE,
  };

  if (vkCreateSwapchainKHR(_device, &swapCreateInfo, nullptr, &_swapChain)
      != VK_SUCCESS)
    {
      std::cerr << "failed to create swapchain!" << std::endl;
      std::exit (-1);
    }

  _swapFormat = format.format;
  _swapExtent = swapExtent;

  std::vector<VkImage> swapImages;

  vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
  _perSwaps.resize(imageCount);
  swapImages.resize(imageCount);
  vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, swapImages.data());

  _depthFormat = VK_FORMAT_D32_SFLOAT;

  for (size_t i = 0; i < _perSwaps.size(); i++) {
    _perSwaps[i].image = swapImages[i];
    VkImageViewCreateInfo swapViewInfo {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image    = _perSwaps[i].image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = _swapFormat,
      .components = {
	.r = VK_COMPONENT_SWIZZLE_IDENTITY,
	.g = VK_COMPONENT_SWIZZLE_IDENTITY,
	.b = VK_COMPONENT_SWIZZLE_IDENTITY,
	.a = VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
	.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
	.baseMipLevel   = 0,
	.levelCount     = 1,
	.baseArrayLayer = 0,
	.layerCount     = 1,
      },
    };

    if (vkCreateImageView(_device, &swapViewInfo, nullptr, &_perSwaps[i].imageView) != VK_SUCCESS) {
      std::cerr << "Failed to create image views" << std::endl;
      std::exit(-1);
    }

    VkExtent3D depthExtent = {
      .width  = _swapExtent.width,
      .height = _swapExtent.height,
      .depth  = 1,
    };

    auto depthInfo = _imageInfo(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthExtent);
    VmaAllocationCreateInfo depthAllocInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateImage(_allocator,
		   &depthInfo,
		   &depthAllocInfo,
		   &_perSwaps[i].depth,
		   &_perSwaps[i].depthAlloc,
		   nullptr);

    auto depthViewInfo = _imageViewInfo(_depthFormat, _perSwaps[i].depth, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (vkCreateImageView(_device, &depthViewInfo, nullptr, &_perSwaps[i].depthView) != VK_SUCCESS) {
      std::cerr << "Failed to create depth image view" << std::endl;
      std::exit(-1);
    }
  }

  vkGetDeviceQueue(_device, _graphicsFamily.value(), 0, &_graphicsQueue);
  vkGetDeviceQueue(_device, _presentFamily.value(), 0, &_presentQueue);

  VkCommandPoolCreateInfo globalPoolInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,

    .queueFamilyIndex = _graphicsFamily.value(),
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };

  if (vkCreateCommandPool(_device, &globalPoolInfo, nullptr, &_globalCommandPool) != VK_SUCCESS) {
    std::cerr << "Failed to create global command pool" << std::endl;
    std::exit(-1);
  }

  VkCommandBufferAllocateInfo globalBufferAllocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,

    .commandPool          = _globalCommandPool,
    .commandBufferCount   = 1,
    .level                = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  if (vkAllocateCommandBuffers(_device, &globalBufferAllocInfo, &_globalCommandBuffer) != VK_SUCCESS) {
    std::cerr << "Failed to allocate global command buffer" << std::endl;
    std::exit(-1);
  }

  VkAttachmentDescription colorAttach = {
    .format   = _swapFormat,
    .samples  = VK_SAMPLE_COUNT_1_BIT,

    .loadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp  = VK_ATTACHMENT_STORE_OP_STORE,

    .stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE,

    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorRef = {
    .attachment = 0,
    .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentDescription depthAttach = {
    .flags  = 0,

    .format   = _depthFormat,
    .samples  = VK_SAMPLE_COUNT_1_BIT,

    .loadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE,

    .stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE,

    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference depthRef = {
    .attachment = 1,
    .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
    .pipelineBindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS,

    .colorAttachmentCount  = 1,
    .pColorAttachments     = (&colorRef),

    .pDepthStencilAttachment  = (&depthRef),
  };

  VkAttachmentDescription attachments[2] = { colorAttach, depthAttach };

  VkRenderPassCreateInfo renderPassInfo = {
    .sType  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,

    .attachmentCount  = 2,
    .pAttachments     = attachments,

    .subpassCount  = 1,
    .pSubpasses    = (&subpass),
  };

  if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    std::cerr << "Failed to create render pass..." << std::endl;
    std::exit(-1);
  }

  VkFramebufferCreateInfo framebufInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .pNext = nullptr,

    .renderPass       = _renderPass,
    .attachmentCount  = 2,

    .width   = _swapExtent.width,
    .height  = _swapExtent.height,
    .layers  = 1,
  };

  for (auto & swap : _perSwaps) {
    VkImageView attachments[2] = { swap.imageView, swap.depthView };
    framebufInfo.pAttachments = attachments;
    if (vkCreateFramebuffer(_device, &framebufInfo, nullptr, &swap.framebuf) != VK_SUCCESS) {
      std::cerr << "Unable to create framebuffer" << std::endl;
      std::exit(1);
    }
  }

  _initPerFrames();

  _initPipelines();

  _initTestData();

  _initialized = true;
}

// Helper function to allocate per-frame synchronization primitives, called from
// Engine::init().
//
// Log and exit on failure.
void gfx::Engine::_initPerFrames() {
  for (auto &frame : _perFrames) {
    VkFenceCreateInfo fenceInfo = {
      .sType  = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext  = nullptr,
      .flags  = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    if (vkCreateFence(_device, &fenceInfo, nullptr, &frame.renderFinishedFence) != VK_SUCCESS) {
      std::cerr << "Failed to create 'render finished' fence" << std::endl;
      std::exit(-1);
    }

    VkSemaphoreCreateInfo semInfo = {
      .sType  = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext  = nullptr,
      .flags  = 0,
    };

    if (vkCreateSemaphore(_device, &semInfo, nullptr, &frame.imageAcquiredSem) != VK_SUCCESS) {
      std::cerr << "Failed to create 'render finished' fence" << std::endl;
      std::exit(-1);
    }
    if (vkCreateSemaphore(_device, &semInfo, nullptr, &frame.renderFinishedSem) != VK_SUCCESS) {
      std::cerr << "Failed to create 'render finished' fence" << std::endl;
      std::exit(-1);
    }

    VkCommandPoolCreateInfo framePoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,

      .queueFamilyIndex = _graphicsFamily.value(),
      .flags = 0,
    };

    if (vkCreateCommandPool(_device, &framePoolInfo, nullptr, &frame.commandPool) != VK_SUCCESS) {
      std::cerr << "Failed to create frame command pool" << std::endl;
      std::exit(-1);
    }
  }
}

// Here we can initialize any hard-coded test data used by the engine. Called at
// the end of Engine::init().
//
// Log and exit on failure.
void gfx::Engine::_initTestData() {
  auto path = ".data/monkey.mesh";
  auto id   = ID("asset:mesh:monkey");
  if (!_loadMesh(path, id, &_testMesh)) {
    std::cerr << "failed to load test-mesh from file '" << path << "'" << std::endl;
    std::exit(-1);
  }
}

// Cleanup after whatever _initTestData did.
void gfx::Engine::_cleanupTestData() {
  _freeMesh(&_testMesh);
}

// Cleanup all resources used by the graphics engine, shutdown vulkan and SDL.
void gfx::Engine::cleanup() {
  if (_initialized) {
    vkDeviceWaitIdle(_device);

    _cleanupTestData();

    for (auto &frame : _perFrames) {
      vkDestroySemaphore(_device, frame.imageAcquiredSem, nullptr);
      vkDestroySemaphore(_device, frame.renderFinishedSem, nullptr);
      vkDestroyFence(_device, frame.renderFinishedFence, nullptr);
      vkDestroyCommandPool(_device, frame.commandPool, nullptr);
    }

    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

    for (auto &psi : _perSwaps) {
      vkDestroyFramebuffer(_device, psi.framebuf, nullptr);
    }

    vkDestroyRenderPass(_device, _renderPass, nullptr);

    vkDestroyCommandPool(_device, _globalCommandPool, nullptr);

    for (auto &swap : _perSwaps) {
      vmaDestroyImage(_allocator, swap.depth, swap.depthAlloc);
      vkDestroyImageView(_device, swap.depthView, nullptr);
      vkDestroyImageView(_device, swap.imageView, nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vmaDestroyAllocator(_allocator);

    vkDestroyDevice(_device, nullptr);
    vkDestroyInstance(_instance, nullptr);
    SDL_DestroyWindow(_window);
    SDL_Quit();
  }
}

// This is used to synchronize our double-buffered frames. Basically we just
// wait on the vkQueueSubmit fence to make sure that there's only two frames in
// flight at any given time.
//
// Log and exit on failure.
gfx::PerFrame *gfx::Engine::_acquireNextFrame() {
  _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  PerFrame *frame = &_perFrames[_currentFrame];

  if (vkWaitForFences(_device, 1, &frame->renderFinishedFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    std::cerr << "timeout or failure while waiting for frame fence." << std::endl;
    std::exit(-1);
  }

  if (vkResetFences(_device, 1, &frame->renderFinishedFence) != VK_SUCCESS) {
    std::cerr << "failed to reset frame fence." << std::endl;
  }

  return frame;
}

// Allocate a single, non-resetable, primary command buffer from `pool`.
//
// Log and exit on failure.
VkCommandBuffer gfx::Engine::_allocCmdBuffer(VkCommandPool pool) {
  VkCommandBuffer cmdBuf;
  VkCommandBufferAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,

    .commandPool          = pool,
    .commandBufferCount   = 1,
    .level                = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  if (vkAllocateCommandBuffers(_device, &allocInfo, &cmdBuf) != VK_SUCCESS) {
    std::cerr << "Failed to allocate frame command buffer" << std::endl;
    std::exit(-1);
  }

  return cmdBuf;
}

// Call vkBeginCmdBuffer on `cmdBuf` with the given `flags`.
//
// Log and exit on failure.
void gfx::Engine::_beginCmdBuffer(VkCommandBuffer cmdBuf, uint32_t flags) {
  VkCommandBufferBeginInfo beginInfo = {
    .sType  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext  = nullptr,
    .flags  = flags,

    .pInheritanceInfo = nullptr,
  };

  if (vkBeginCommandBuffer(cmdBuf, &beginInfo) != VK_SUCCESS) {
    std::cerr << "Failed to begin command buffer..." << std::endl;
    std::exit(-1);
  }
}

// Abstract over calling vkCmdBeginRenderPass on `cmdBuf`
//
// Log and exit on failure.
void
gfx::Engine::_beginRenderPass(VkCommandBuffer cmdBuf,
			      VkFramebuffer fb,
			      VkClearValue *clearValues,
			      uint32_t clearCount)
{
  VkRenderPassBeginInfo rpassBeginInfo = {
    .sType  = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext  = nullptr,

    .renderPass = _renderPass,
    .renderArea = {
      .offset = { .x = 0, .y = 0 },
      .extent = _swapExtent,
    },
    .framebuffer = fb,

    .clearValueCount = clearCount,
    .pClearValues    = clearValues,
  };

  vkCmdBeginRenderPass(cmdBuf, &rpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

// Draw a frame. Basically, we acquire a frame and swap image, write a bunch of
// commands to a buffer, submit those commands, and then present the frame.
//
// Log and exit on failure.
void gfx::Engine::draw() {
  PerFrame *frame = _acquireNextFrame();

  uint32_t swapIndex;
  if (vkAcquireNextImageKHR(_device,
			    _swapChain,
			    UINT64_MAX,
			    frame->imageAcquiredSem,
			    nullptr,
			    &swapIndex)
      != VK_SUCCESS)
    {
      std::cerr << "Failed to acquire swapchain image." << std::endl;
      std::exit(-1);
    }

  PerSwapImage *swap = &_perSwaps[swapIndex];

  if (vkResetCommandPool(_device, frame->commandPool, 0) != VK_SUCCESS) {
    std::cerr << "Failed to reset frame command pool." << std::endl;
    std::exit(-1);
  }

  VkCommandBuffer cmdBuf = _allocCmdBuffer(frame->commandPool);

  _beginCmdBuffer(cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VkClearValue colorClear =  { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
  VkClearValue depthClear = {
    .depthStencil = {
      .depth = 1.0f,
    },
  };

  VkClearValue clearValues[2] = { colorClear, depthClear };

  _beginRenderPass(cmdBuf, swap->framebuf, clearValues, 2);

  // Draw commands go here :D

  glm::vec3 camPos = { 0.0f, 0.5f, 1.5f };

  glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));

  glm::mat4 project =
    glm::perspective(glm::radians(70.0f),
		     (float) _swapExtent.width / (float) _swapExtent.height,
		     0.1f, 200.0f);

  project[1][1] *= -1;

  glm::mat4 model = glm::rotate(glm::mat4 { 1.0f },
				glm::radians(_framesDrawn * 0.4f),
				glm::vec3(0, 1, 0));

  MeshPushConstants constants = {
    .renderMatrix = project * view * model,
  };

  vkCmdPushConstants(cmdBuf, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
		     sizeof(MeshPushConstants), &constants);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

  VkDeviceSize offset = 0;

  vkCmdBindVertexBuffers(cmdBuf, 0, 1, &_testMesh.vbuffer.buffer, &offset);
  vkCmdBindIndexBuffer(cmdBuf, _testMesh.ibuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdDrawIndexed(cmdBuf, _testMesh.meshData.indexCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(cmdBuf);

  if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
    std::cerr << "failed to end command buffer." << std::endl;
    std::exit(-1);
  }

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo = {
    .sType  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext  = nullptr,

    .pWaitDstStageMask  = (&waitStage),

    .waitSemaphoreCount  = 1,
    .pWaitSemaphores     = (&frame->imageAcquiredSem),

    .signalSemaphoreCount  = 1,
    .pSignalSemaphores     = (&frame->renderFinishedSem),

    .commandBufferCount  = 1,
    .pCommandBuffers     = &cmdBuf,
  };

  if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, frame->renderFinishedFence) != VK_SUCCESS) {
    std::cerr << "Failed to submit command buffer for rendering..." << std::endl;
    std::exit(-1);
  }

  VkPresentInfoKHR presentInfo = {
    .sType  = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext  = nullptr,

    .swapchainCount  = 1,
    .pSwapchains     = (&_swapChain),

    .waitSemaphoreCount  = 1,
    .pWaitSemaphores     = (&frame->renderFinishedSem),

    .pImageIndices  = &swapIndex,
  };

  if (vkQueuePresentKHR(_graphicsQueue, &presentInfo) != VK_SUCCESS) {
    std::cerr << "Failed to present to the swapchain =[" << std::endl;
    std::exit(-1);
  }

  _framesDrawn++;
}

// Initialize this DescriptorAllocator for use with the given `device`
void gfx::DescriptorAllocator::init(VkDevice device) {
  _device = device;
}

// Destroy all VkDescriptorPools in use by the allocator.
void gfx::DescriptorAllocator::cleanup() {
  for (auto p : _freePools) {
    vkDestroyDescriptorPool(_device, p, nullptr);
  }
  for (auto p : _usedPools) {
    vkDestroyDescriptorPool(_device, p, nullptr);
  }

  _freePools.resize(0);
  _usedPools.resize(0);
}

// Helper to create a descriptor pool. For internal use by the allocator.
VkDescriptorPool gfx::DescriptorAllocator::createPool(
  size_t count,
  VkDescriptorPoolCreateFlags flags)
{
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(_descriptorSizes.sizes.size());
  for (auto sz : _descriptorSizes.sizes) {
    sizes.push_back({ sz.first, (uint32_t)(sz.second * (float)count)});
  }

  VkDescriptorPoolCreateInfo poolInfo = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags         = flags,
    .maxSets       = (uint32_t)count,
    .poolSizeCount = (uint32_t)sizes.size(),
    .pPoolSizes    = sizes.data(),
  };

  VkDescriptorPool descriptorPool;

  vkCreateDescriptorPool(_device, &poolInfo, nullptr, &descriptorPool);

  return descriptorPool;
}

// Get an unused pool, either from the free-list or by creating a new one.
VkDescriptorPool gfx::DescriptorAllocator::grabPool() {
  if (_freePools.size() > 0) {
    VkDescriptorPool pool = _freePools.back();
    _freePools.pop_back();
    return pool;
  } else {
    return createPool(1000, 0);
  }
}

// Allocate a descriptor set with the given layout.
bool gfx::DescriptorAllocator::allocate(
  VkDescriptorSet *set,
  VkDescriptorSetLayout layout)
{
  if (_currentPool == VK_NULL_HANDLE) {
    _currentPool = grabPool();
    _usedPools.push_back(_currentPool);
  }

  VkDescriptorSetAllocateInfo allocInfo = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext              = nullptr,
    .pSetLayouts        = (&layout),
    .descriptorPool     = _currentPool,
    .descriptorSetCount = 1,
  };

  VkResult allocResult = vkAllocateDescriptorSets(_device, &allocInfo, set);

  bool needNewPool = false;

  switch (allocResult) {
  case VK_SUCCESS:
    return true;

  case VK_ERROR_FRAGMENTED_POOL:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    needNewPool = true;
    break;

  default:
    return false;
  };

  if (needNewPool) {
    _currentPool = grabPool();
    _usedPools.push_back(_currentPool);

    allocResult = vkAllocateDescriptorSets(_device, &allocInfo, set);

    if (allocResult == VK_SUCCESS) {
      return true;
    }
  }

  // We failed to allocate twice, even with a brand new pool --
  // something's wrong.
  return false;
}

// Reset all pools allocated by this allocator. This effective frees every
// DescriptorSet it has been used to allocate.
void gfx::DescriptorAllocator::resetPools() {
  for (auto p : _usedPools) {
    vkResetDescriptorPool(_device, p, 0);
    _freePools.push_back(p);
  }

  _usedPools.clear();
  _currentPool = VK_NULL_HANDLE;
}

// Initialize a DescriptorLayoutCache for the given `device`
void gfx::DescriptorLayoutCache::init(VkDevice device) {
  _device = device;
}

// Destroy all layouts created by the cache.
void gfx::DescriptorLayoutCache::cleanup() {
  for (auto pair : _layoutCache) {
    vkDestroyDescriptorSetLayout(_device, pair.second, nullptr);
  }
}

// Create a new VkDescriptorSetLayout, or if a layout that matches `info`
// already exists, return it from the cache.
VkDescriptorSetLayout
gfx::DescriptorLayoutCache::createDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info) {
  DescriptorLayoutInfo layoutInfo;
  layoutInfo.bindings.reserve(info->bindingCount);

  bool isSorted = true;
  int lastBinding = -1;

  for (int i = 0; i < info->bindingCount; i++) {
    layoutInfo.bindings.push_back(info->pBindings[i]);
    if (info->pBindings[i].binding > lastBinding) {
      lastBinding = info->pBindings[i].binding;
    } else {
      isSorted = false;
    }
  }

  if (!isSorted) {
    std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(),
	      [](VkDescriptorSetLayoutBinding & a, VkDescriptorSetLayoutBinding & b) {
		return a.binding > b.binding;
	      });

  }

  auto it = _layoutCache.find(layoutInfo);
  if (it != _layoutCache.end()) {
    return (*it).second;
  } else {
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(_device, info, nullptr, &layout);

    _layoutCache[layoutInfo] = layout;
    return layout;
  }
}

// Equality operator on DescriptorLayoutInfo, for use with the hash map.
bool gfx::DescriptorLayoutCache::DescriptorLayoutInfo::operator == (const DescriptorLayoutInfo & other) const
{
  if (other.bindings.size() != bindings.size()) {
    return false;
  }

  for (int i = 0; i < bindings.size(); i++) {
    if (other.bindings[i].binding != bindings[i].binding) {
      return false;
    }
    if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
      return false;
    }
    if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
      return false;
    }
    if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
      return false;
    }
  }

  return true;
}

// Hash implementation for DescriptorLayoutInfo, for use with the hash map.
size_t gfx::DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
  size_t result = bindings.size();

  for (const VkDescriptorSetLayoutBinding& b : bindings)
  {
    size_t binding_hash = b.binding
                        ^ b.descriptorType << 8
                        ^ b.descriptorCount << 16
                        ^ b.stageFlags << 24;

    result = (result << 5)
           ^ binding_hash
           ^ (result >> 45);
  }

  return result;
}

// Create a new, blank DescriptorBuilder
gfx::DescriptorBuilder gfx::DescriptorBuilder::begin(
  DescriptorLayoutCache *layoutCache,
  DescriptorAllocator *allocator)
{
  DescriptorBuilder builder;

  builder._cache     = layoutCache;
  builder._allocator = allocator;

  return builder;
}

// Bind a buffer to the resulting VkDescriptorSet
gfx::DescriptorBuilder &gfx::DescriptorBuilder::bind_buffer(
  uint32_t binding,
  VkDescriptorBufferInfo *bufferInfo,
  VkDescriptorType type,
  VkShaderStageFlags stageFlags)
{
  VkDescriptorSetLayoutBinding dslb {
    .descriptorCount    = 1,
    .descriptorType     = type,
    .pImmutableSamplers = nullptr,
    .stageFlags         = stageFlags,
    .binding            = binding,
  };

  _bindings.push_back(dslb);

  VkWriteDescriptorSet wds {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,

    .descriptorCount = 1,
    .descriptorType  = type,
    .pBufferInfo     = bufferInfo,
    .dstBinding      = binding,
  };

  _writes.push_back(wds);

  return *this;
}

// Bind an image to the resulting VkDescriptorSet
gfx::DescriptorBuilder &gfx::DescriptorBuilder::bind_image(
  uint32_t binding,
  VkDescriptorImageInfo *imageInfo,
  VkDescriptorType type,
  VkShaderStageFlags stageFlags)
{
  VkDescriptorSetLayoutBinding dslb = {
    .descriptorCount    = 1,
    .descriptorType     = type,
    .pImmutableSamplers = nullptr,
    .stageFlags         = stageFlags,
    .binding            = binding,
  };

  _bindings.push_back(dslb);

  VkWriteDescriptorSet wds = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,

    .descriptorCount = 1,
    .descriptorType  = type,
    .dstBinding      = binding,
    .pImageInfo      = imageInfo,
  };

  _writes.push_back(wds);

  return *this;
}

// Build a VkDescriptorSet from this builder.
bool gfx::DescriptorBuilder::build(VkDescriptorSet &set, VkDescriptorSetLayout &layout) {
  VkDescriptorSetLayoutCreateInfo layoutInfo = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext        = nullptr,
    .pBindings    = _bindings.data(),
    .bindingCount = (uint32_t)_bindings.size(),
  };

  layout = _cache->createDescriptorLayout(&layoutInfo);

  bool success = _allocator->allocate(&set, layout);

  if (!success) { return false; };

  for (auto &w : _writes) {
    w.dstSet = set;
  }

  vkUpdateDescriptorSets(_allocator->_device, _writes.size(), _writes.data(), 0, nullptr);

  return true;
}
