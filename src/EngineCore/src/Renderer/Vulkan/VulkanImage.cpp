#include <RaidenEngineCore/Renderer/Vulkan/VulkanImage.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <vector>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanImage");

VulkanImage::~VulkanImage() { shutdown(); }

bool VulkanImage::init(VkPhysicalDevice physicalDevice, VkDevice device,
                        uint32_t width, uint32_t height, VkFormat format,
                        VkImageUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkImageAspectFlags aspectFlags) {
  physicalDevice_ = physicalDevice;
  device_ = device;

  VkImageCreateInfo imageInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {width, height, 1},
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
    s_logger.critical("Failed to create image");
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device_, image_, &memRequirements);

  VkMemoryAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = memRequirements.size,
    .memoryTypeIndex =
        findMemoryType(memRequirements.memoryTypeBits, properties),
  };

  if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
    s_logger.critical("Failed to allocate image memory");
    return false;
  }

  vkBindImageMemory(device_, image_, memory_, 0);

  VkImageViewCreateInfo viewInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image_,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .subresourceRange = {aspectFlags, 0, 1, 0, 1},
  };

  if (vkCreateImageView(device_, &viewInfo, nullptr, &view_) != VK_SUCCESS) {
    s_logger.critical("Failed to create image view");
    return false;
  }

  return true;
}

void VulkanImage::shutdown() {
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, view_, nullptr);
    view_ = VK_NULL_HANDLE;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_, image_, nullptr);
    image_ = VK_NULL_HANDLE;
  }
}

uint32_t VulkanImage::findMemoryType(uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  s_logger.critical("Failed to find suitable memory type");
  return 0;
}

static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                                     const std::vector<VkFormat> &candidates,
                                     VkImageTiling tiling,
                                     VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  s_logger.critical("Failed to find supported format");
  return VK_FORMAT_UNDEFINED;
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) {
  return findSupportedFormat(
      physicalDevice,
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT,
       VK_FORMAT_D16_UNORM},
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

}
