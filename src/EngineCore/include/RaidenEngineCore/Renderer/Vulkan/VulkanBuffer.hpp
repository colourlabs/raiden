#pragma once

#include <volk.h>

namespace Raiden::Core {

class VulkanBuffer {
public:
  VulkanBuffer() = default;
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;
  VulkanBuffer(VulkanBuffer &&) = delete;
  VulkanBuffer &operator=(VulkanBuffer &&) = delete;

  bool init(VkPhysicalDevice physicalDevice, VkDevice device,
            VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties);
  void shutdown();

  void upload(const void *data, VkDeviceSize size);

  VkBuffer buffer() const { return buffer_; }

private:
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  void *mapped_ = nullptr;
  VkDeviceSize size_ = 0;
};

}
