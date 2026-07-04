#pragma once

#include <volk.h>

namespace Raiden::Renderer {

class VulkanRenderPass {
public:
  bool init(VkDevice device, VkFormat imageFormat, VkFormat depthFormat,
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
  void shutdown();

  [[nodiscard]] VkRenderPass renderPass() const { return renderPass_; }
  [[nodiscard]] bool hasDepth() const { return depthFormat_ != VK_FORMAT_UNDEFINED; }
  [[nodiscard]] VkSampleCountFlagBits sampleCount() const { return sampleCount_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
  VkSampleCountFlagBits sampleCount_ = VK_SAMPLE_COUNT_1_BIT;
};

}
