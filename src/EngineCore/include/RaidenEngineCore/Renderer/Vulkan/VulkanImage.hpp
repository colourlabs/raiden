#pragma once

#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>
#include <volk.h>

namespace Raiden::Core {

class VulkanImage {
public:
  VulkanImage() = default;
  ~VulkanImage();

  VulkanImage(const VulkanImage &) = delete;
  VulkanImage &operator=(const VulkanImage &) = delete;
  VulkanImage(VulkanImage &&) = delete;
  VulkanImage &operator=(VulkanImage &&) = delete;

  bool init(VkDevice device, VmaAllocator allocator, uint32_t width,
            uint32_t height, VkFormat format, VkImageUsageFlags usage,
            VmaMemoryUsage memoryUsage, VkImageAspectFlags aspectFlags);

  void shutdown();

  VkImage image() const { return image_; }
  VkImageView view() const { return view_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = nullptr;
  VmaAllocation allocation_ = nullptr;

  VkImage image_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
};

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

} // namespace Raiden::Core