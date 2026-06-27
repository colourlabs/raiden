#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanFrameContext.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanSwapchain.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanFrameContext");

bool VulkanFrameContext::init(VkDevice device, uint32_t graphicsFamily) {
  device_ = device;

  // command pool
  VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsFamily,
  };

  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to create command pool");
    return false;
  }

  // command buffers, one per frame in flight
  commandBuffers_.resize(kMaxFramesInFlight);

  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = kMaxFramesInFlight,
  };

  if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to allocate command buffers");
    return false;
  }

  // sync objects, one set per frame in flight
  imageAvailableSemaphores_.resize(kMaxFramesInFlight);
  renderFinishedSemaphores_.resize(kMaxFramesInFlight);
  inFlightFences_.resize(kMaxFramesInFlight);

  VkSemaphoreCreateInfo semInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceInfo{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, // start signaled so first
                                             // beginFrame doesn't block forever
  };

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (vkCreateSemaphore(device_, &semInfo, nullptr,
                          &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semInfo, nullptr,
                          &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) !=
            VK_SUCCESS) {
      s_logger.critical("Failed to create sync objects");
      return false;
    }
  }

  s_logger.info("Frame context created.");
  return true;
}

void VulkanFrameContext::shutdown() {
  for (uint32_t i = 0; i < inFlightFences_.size(); ++i) {
    vkDestroyFence(device_, inFlightFences_[i], nullptr);
    vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
    vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
  }
  inFlightFences_.clear();
  renderFinishedSemaphores_.clear();
  imageAvailableSemaphores_.clear();

  if (commandPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, commandPool_,
                         nullptr); // also frees its command buffers
    commandPool_ = VK_NULL_HANDLE;
  }
  commandBuffers_.clear();
}

bool VulkanFrameContext::beginFrame(VulkanSwapchain &swapchain,
                                    uint32_t &outImageIndex) {
  vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE,
                  UINT64_MAX);

  VkResult result = vkAcquireNextImageKHR(
      device_, swapchain.swapchain(), UINT64_MAX,
      imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &outImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return false; // caller should recreate swapchain
  }
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    s_logger.error("Failed to acquire swapchain image");
    return false;
  }

  vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
  vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

  return true;
}

bool VulkanFrameContext::endFrame(VulkanSwapchain &swapchain,
                                  VkQueue graphicsQueue, VkQueue presentQueue,
                                  uint32_t imageIndex) {
  VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffers_[currentFrame_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signalSemaphores,
  };

  if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                    inFlightFences_[currentFrame_]) != VK_SUCCESS) {
    s_logger.error("Failed to submit draw command buffer");
    return false;
  }

  VkSwapchainKHR swapchains[] = {swapchain.swapchain()};

  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signalSemaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &imageIndex,
  };

  VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

  currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return false; // caller should recreate swapchain
  }
  if (result != VK_SUCCESS) {
    s_logger.error("Failed to present swapchain image");
    return false;
  }

  return true;
}

}; // namespace Raiden::Core