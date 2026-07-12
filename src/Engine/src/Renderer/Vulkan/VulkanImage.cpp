#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanImage.hpp>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanImage");

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
      .mipLevels = info.mipLevels,
      .arrayLayers = info.arrayLayers,
      .samples = info.sampleCount,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = info.usage,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo allocInfo{
      .usage = info.memoryUsage,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };

  if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image_,
                     &allocation_, nullptr) != VK_SUCCESS) {
    s_logger.error("Failed to create image ({}x{}, format={})", info.width,
                   info.height, static_cast<int>(info.format));
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
    s_logger.error("Failed to create image view");
    vmaDestroyImage(allocator_, image_, allocation_);
    image_ = VK_NULL_HANDLE;
    return false;
  }

  return true;
}

bool VulkanImage::init(VkDevice device, VmaAllocator allocator, uint32_t width,
                       uint32_t height, VkFormat format,
                       VkImageUsageFlags usage, VmaMemoryUsage memoryUsage,
                       VkImageAspectFlags aspectFlags,
                       VkSampleCountFlagBits sampleCount) {
  device_ = device;
  allocator_ = allocator;

  VkImageCreateInfo imageInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {width, height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = sampleCount,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo allocInfo{
      .usage = memoryUsage,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };

  if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image_,
                     &allocation_, nullptr) != VK_SUCCESS) {
    s_logger.error("Failed to create image ({}x{}, format={})", width, height,
                   static_cast<int>(format));
    return false;
  }

  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image_,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {aspectFlags, 0, 1, 0, 1},
  };

  if (vkCreateImageView(device_, &viewInfo, nullptr, &view_) != VK_SUCCESS) {
    s_logger.error("Failed to create image view");
    vmaDestroyImage(allocator_, image_, allocation_);
    image_ = VK_NULL_HANDLE;
    return false;
  }

  return true;
}

void VulkanImage::shutdown() {
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, view_, nullptr);
    view_ = VK_NULL_HANDLE;
  }
  if (image_ != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator_, image_, allocation_);
    image_ = VK_NULL_HANDLE;
  }
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) {
  std::array<VkFormat, 3> candidates = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
  };

  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }

  return VK_FORMAT_UNDEFINED;
}

} // namespace Raiden::Renderer
