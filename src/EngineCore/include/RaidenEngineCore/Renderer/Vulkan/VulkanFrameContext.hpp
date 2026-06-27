#pragma once

#include <vector>
#include <volk.h>

namespace Raiden::Core {

class VulkanSwapchain;

class VulkanFrameContext {
public:
  static constexpr uint32_t kMaxFramesInFlight = 2;

  bool init(VkDevice device, uint32_t graphicsFamily);
  void shutdown();

  // returns false if the swapchain is out of date and needs recreation
  bool beginFrame(VulkanSwapchain &swapchain, uint32_t &outImageIndex);

  VkCommandBuffer currentCommandBuffer() const {
    return commandBuffers_[currentFrame_];
  }

  // submits the recorded command buffer and presents. returns false if the
  // swapchain is out of date / suboptimal and needs recreation.
  bool endFrame(VulkanSwapchain &swapchain, VkQueue graphicsQueue,
                VkQueue presentQueue, uint32_t imageIndex);

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;

  std::vector<VkCommandBuffer> commandBuffers_;
  std::vector<VkSemaphore> imageAvailableSemaphores_;
  std::vector<VkSemaphore> renderFinishedSemaphores_;
  std::vector<VkFence> inFlightFences_;

  uint32_t currentFrame_ = 0;
};

} // namespace Raiden::Core