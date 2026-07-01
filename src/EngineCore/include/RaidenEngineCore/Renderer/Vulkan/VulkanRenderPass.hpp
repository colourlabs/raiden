#pragma once

#include <volk.h>

namespace Raiden::Core {

class VulkanRenderPass {
public:
  bool init(VkDevice device, VkFormat imageFormat, VkFormat depthFormat,
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
  void shutdown();

  VkRenderPass renderPass() const { return renderPass_; }
  bool hasDepth() const { return depthFormat_ != VK_FORMAT_UNDEFINED; }
  VkSampleCountFlagBits sampleCount() const { return sampleCount_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
  VkSampleCountFlagBits sampleCount_ = VK_SAMPLE_COUNT_1_BIT;
};

}
