/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"

// Vulkan forward declarations
#if defined (IDRA_VULKAN)
#define IDRA_VK_DEFINE_HANDLE(object) typedef struct object##_T* object;

IDRA_VK_DEFINE_HANDLE( VkAccelerationStructureKHR )
IDRA_VK_DEFINE_HANDLE( VkBuffer )
IDRA_VK_DEFINE_HANDLE( VkDescriptorSet )
IDRA_VK_DEFINE_HANDLE( VkDescriptorSetLayout )
IDRA_VK_DEFINE_HANDLE( VkDeviceMemory )
IDRA_VK_DEFINE_HANDLE( VkImage )
IDRA_VK_DEFINE_HANDLE( VkImageView )
IDRA_VK_DEFINE_HANDLE( VkPipeline )
IDRA_VK_DEFINE_HANDLE( VkPipelineLayout )
IDRA_VK_DEFINE_HANDLE( VkSampler )
IDRA_VK_DEFINE_HANDLE( VkShaderModule )
IDRA_VK_DEFINE_HANDLE( VmaAllocation )

struct VkDescriptorSetLayoutBinding;
struct VkPipelineShaderStageCreateInfo;
struct VkComputePipelineCreateInfo;
struct VkRayTracingShaderGroupCreateInfoKHR;

typedef uint32_t VkBool32;
typedef uint64_t VkDeviceAddress;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkFlags;
typedef uint32_t VkSampleMask;

typedef VkFlags VkAccessFlags;
typedef VkFlags VkBufferUsageFlags;
typedef VkFlags VkCullModeFlags;
typedef VkFlags VkImageUsageFlags;
typedef VkFlags VkPipelineStageFlags;

typedef uint32_t VkOpaqueEnum;

#endif // IDRA_VULKAN