#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanSwapchain.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanSwapchain");

bool VulkanSwapchain::init(VkPhysicalDevice physicalDevice, VkDevice device,
                           VkSurfaceKHR surface, uint32_t graphicsFamily,
                           uint32_t presentFamily, uint32_t windowWidth,
                           uint32_t windowHeight, bool vsync,
                           VkSwapchainKHR oldSwapchain) {
  device_ = device;

  VkSurfaceCapabilitiesKHR caps;
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                &caps) != VK_SUCCESS) {
    s_logger.error("Failed to query surface capabilities.");
    return false;
  }

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);
  if (formatCount == 0) {
    s_logger.error("No surface formats available.");
    return false;
  }
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats.data());

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);
  if (presentModeCount == 0) {
    s_logger.error("No surface present modes available.");
    return false;
  }
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physicalDevice, surface, &presentModeCount, presentModes.data());

  // picks format, present mode and extent
  VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
  VkPresentModeKHR presentMode = choosePresentMode(presentModes, vsync);
  extent_ = chooseExtent(caps, windowWidth, windowHeight);
  imageFormat_ = surfaceFormat.format;

  // image count: min+1, clamped to max (0 = no max)
  uint32_t imageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
    imageCount = caps.maxImageCount;
  }

  // swapchain creation
  std::array<uint32_t, 2> queueFamilyIndices = {graphicsFamily, presentFamily};

  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent_,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = oldSwapchain,
  };

  if (graphicsFamily != presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create swapchain.");
    return false;
  }

  // fetch swapchain images
  uint32_t actualImageCount = 0;
  vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, nullptr);
  images_.resize(actualImageCount);
  vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount,
                          images_.data());

  // create an image view per image
  imageViews_.resize(images_.size());
  for (size_t i = 0; i < images_.size(); ++i) {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = images_[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageFormat_,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageViews_[i]) !=
        VK_SUCCESS) {
      s_logger.error("Failed to create image view.");
      return false;
    }
  }

  s_logger.info("Swapchain created with " + std::to_string(images_.size()) +
                " images.");
  return true;
}

void VulkanSwapchain::destroyImageViews() {
  for (auto *view : imageViews_) {
    vkDestroyImageView(device_, view, nullptr);
  }
  imageViews_.clear();
}

void VulkanSwapchain::shutdown() {
  destroyImageViews();

  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &formats) {
  for (const auto &f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }

  return formats[0]; // do fuck all
}

VkPresentModeKHR
VulkanSwapchain::choosePresentMode(const std::vector<VkPresentModeKHR> &modes,
                                    bool vsync) {
  if (!vsync) {
    for (const auto &m : modes) {
      if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        return m;
      }
    }
  } else {
    for (const auto &m : modes) {
      if (m == VK_PRESENT_MODE_FIFO_KHR) {
        return m;
      }
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseExtent(const VkSurfaceCapabilitiesKHR &caps,
                                         uint32_t width, uint32_t height) {
  if (caps.currentExtent.width != UINT32_MAX) {
    return caps.currentExtent; // window manager dictates exact size
  }

  VkExtent2D extent{width, height};
  extent.width = std::clamp(extent.width, caps.minImageExtent.width,
                            caps.maxImageExtent.width);
  extent.height = std::clamp(extent.height, caps.minImageExtent.height,
                             caps.maxImageExtent.height);

  return extent;
}

bool VulkanSwapchain::recreate(VkPhysicalDevice physicalDevice,
                               VkSurfaceKHR surface, uint32_t graphicsFamily,
                               uint32_t presentFamily, uint32_t windowWidth,
                               uint32_t windowHeight, bool vsync) {
  VkSwapchainKHR oldSwapchain = swapchain_;
  destroyImageViews();

  bool result =
      init(physicalDevice, device_, surface, graphicsFamily, presentFamily,
           windowWidth, windowHeight, vsync, oldSwapchain);

  if (oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
  }

  return result;
}

} // namespace Raiden::Renderer