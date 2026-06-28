#pragma once

#include <volk.h>

// volk handles this for us so
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1004000 // vulkan 1.4

#include <vk_mem_alloc.h>

namespace Raiden::Core {

// abstraction over the VulkanMemoryAllocator for convienence
class VulkanAllocator {
public:
  bool init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
  void shutdown();

  VmaAllocator handle() const noexcept { return allocator_; }

private:
  VmaAllocator allocator_{};
};

} // namespace Raiden::Core