#pragma once

#include <vector>
#include <volk.h>

namespace Raiden::Renderer {

// i loove boiler plate Yes, fuck  you opengl for being simple
// ref:
// https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html

class VulkanSwapchain {
public:
  bool init(VkPhysicalDevice physicalDevice, VkDevice device,
            VkSurfaceKHR surface, uint32_t graphicsFamily,
            uint32_t presentFamily, uint32_t windowWidth, uint32_t windowHeight,
            bool vsync, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

  bool recreate(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                uint32_t graphicsFamily, uint32_t presentFamily,
                uint32_t windowWidth, uint32_t windowHeight, bool vsync);

  void shutdown();

  [[nodiscard]] VkSwapchainKHR swapchain() const { return swapchain_; };
  [[nodiscard]] VkFormat imageFormat() const { return imageFormat_; };
  [[nodiscard]] VkExtent2D extent() const { return extent_; }
  [[nodiscard]] const std::vector<VkImageView> &imageViews() const {
    return imageViews_;
  }
  [[nodiscard]] const std::vector<VkImage> &images() const { return images_; }

private:
  VkDevice device_ = VK_NULL_HANDLE; // kept for shutdown
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

  std::vector<VkImage> images_;
  std::vector<VkImageView> imageViews_;

  VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};

  void destroyImageViews();

  VkSurfaceFormatKHR static chooseSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &formats);

  static VkPresentModeKHR
  choosePresentMode(const std::vector<VkPresentModeKHR> &modes, bool vsync);

  static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &caps, uint32_t width,
                          uint32_t height);
};

} // namespace Raiden::Renderer