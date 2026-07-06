#pragma once

#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1004000

#include <vk_mem_alloc.h>

namespace Raiden::Renderer {

class VulkanAllocator {
public:
  bool init(VkInstance instance, VkPhysicalDevice physicalDevice,
            VkDevice device);
  void shutdown();

  [[nodiscard]] VmaAllocator handle() const noexcept { return allocator_; }

private:
  VmaAllocator allocator_{};
};

} // namespace Raiden::Renderer