#pragma once

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

  bool init(VkPhysicalDevice physicalDevice, VkDevice device,
            uint32_t width, uint32_t height, VkFormat format,
            VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
            VkImageAspectFlags aspectFlags);
  void shutdown();

  VkImage image() const { return image_; }
  VkImageView view() const { return view_; }

private:
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
};

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

}
