#pragma once

#include <Renderer/Vulkan/VulkanAllocator.hpp>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanBuffer {
public:
  VulkanBuffer() = default;
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;
  VulkanBuffer(VulkanBuffer &&) = delete;
  VulkanBuffer &operator=(VulkanBuffer &&) = delete;

  bool init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
            VmaMemoryUsage memoryUsag, VmaAllocationCreateFlags allocFlags = 0);

  void shutdown();

  bool map();
  void unmap();

  void upload(const void *data, VkDeviceSize size);

  [[nodiscard]] VkBuffer buffer() const { return buffer_; }
  [[nodiscard]] void *mapped() const { return mapped_; }

private:
  VmaAllocator allocator_ = nullptr;
  VmaAllocation allocation_ = nullptr;

  VkBuffer buffer_ = VK_NULL_HANDLE;

  void *mapped_ = nullptr;
  VkDeviceSize size_ = 0;
};

} // namespace Raiden::Renderer
