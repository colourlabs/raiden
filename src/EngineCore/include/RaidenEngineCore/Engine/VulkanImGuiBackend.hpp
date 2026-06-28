#pragma once

#include <RaidenEngineCore/Engine/IImGuiBackend.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/IVulkanRenderDevice.hpp>
#include <volk.h>

namespace Raiden::Core {

class VulkanImGuiBackend final : public IImGuiBackend {
public:
  VulkanImGuiBackend(IVulkanRenderDevice &device, VkRenderPass renderPass,
                     uint32_t imageCount);
  ~VulkanImGuiBackend() override;

  bool init() override;
  void newFrame() override;
  void renderDrawData(ICommandBuffer &cmd) override;
  void shutdown() override;
  void waitIdle() override;

private:
  IVulkanRenderDevice &device_;
  VkRenderPass renderPass_;
  uint32_t imageCount_;
  VkDevice vkDevice_ = VK_NULL_HANDLE;
  bool shutdown_ = true;
};

} // namespace Raiden::Core
