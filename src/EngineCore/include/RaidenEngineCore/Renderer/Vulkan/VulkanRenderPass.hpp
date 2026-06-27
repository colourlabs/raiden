#pragma once

#include <volk.h>

namespace Raiden::Core {

class VulkanRenderPass {
public:
  bool init(VkDevice device, VkFormat imageFormat);
  void shutdown();

  VkRenderPass renderPass() const { return renderPass_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
};

}
