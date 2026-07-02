#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanAllocator.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanImage.hpp>

#include <vector>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanImage");

VulkanImage::~VulkanImage() { shutdown(); }

VulkanImage::VulkanImage(VulkanImage &&other) noexcept
    : device_(other.device_), allocator_(other.allocator_),
      allocation_(other.allocation_), image_(other.image_),
      view_(other.view_), arrayLayers_(other.arrayLayers_) {
  other.device_ = VK_NULL_HANDLE;
  other.allocator_ = nullptr;
  other.allocation_ = nullptr;
  other.image_ = VK_NULL_HANDLE;
  other.view_ = VK_NULL_HANDLE;
  other.arrayLayers_ = 1;
}

VulkanImage &VulkanImage::operator=(VulkanImage &&other) noexcept {
  if (this != &other) {
    shutdown();
    device_ = other.device_;
    allocator_ = other.allocator_;
    allocation_ = other.allocation_;
    image_ = other.image_;
    view_ = other.view_;
    arrayLayers_ = other.arrayLayers_;
    other.device_ = VK_NULL_HANDLE;
    other.allocator_ = nullptr;
    other.allocation_ = nullptr;
    other.image_ = VK_NULL_HANDLE;
    other.view_ = VK_NULL_HANDLE;
    other.arrayLayers_ = 1;
  }
  return *this;
}

bool VulkanImage::init(VkDevice device, VmaAllocator allocator,
                       const VulkanImageInfo &info) {
  device_ = device;
  allocator_ = allocator;
  arrayLayers_ = info.arrayLayers;

  VkImageCreateInfo imageInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .flags = info.createFlags,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = info.format,
      .extent = {info.width, info.height, 1},
      .mipLevels = 1,
      .arrayLayers = info.arrayLayers,
      .samples = info.sampleCount,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = info.usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo allocInfo{
      .usage = info.memoryUsage,
  };

  if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image_,
                     &allocation_, nullptr) != VK_SUCCESS) {
    s_logger.critical("Failed to create image");
    return false;
  }

  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image_,
      .viewType = info.viewType,
      .format = info.format,
      .subresourceRange = {info.aspectFlags, 0, 1, 0, info.arrayLayers},
  };

  if (vkCreateImageView(device_, &viewInfo, nullptr, &view_) != VK_SUCCESS) {
    s_logger.critical("Failed to create image view");
    return false;
  }

  return true;
}

bool VulkanImage::init(VkDevice device, VmaAllocator allocator,
                       uint32_t width, uint32_t height, VkFormat format,
                       VkImageUsageFlags usage, VmaMemoryUsage memoryUsage,
                       VkImageAspectFlags aspectFlags,
                       VkSampleCountFlagBits sampleCount) {
  VulkanImageInfo info{
      .width = width,
      .height = height,
      .format = format,
      .usage = usage,
      .memoryUsage = memoryUsage,
      .aspectFlags = aspectFlags,
      .sampleCount = sampleCount,
  };
  return init(device, allocator, info);
}

void VulkanImage::shutdown() {
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, view_, nullptr);
    view_ = VK_NULL_HANDLE;
  }

  if (image_ != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator_, image_, allocation_);
    image_ = VK_NULL_HANDLE;
    allocation_ = nullptr;
  }

  device_ = VK_NULL_HANDLE;
  allocator_ = nullptr;
  arrayLayers_ = 1;
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
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

} // namespace Raiden::Core
