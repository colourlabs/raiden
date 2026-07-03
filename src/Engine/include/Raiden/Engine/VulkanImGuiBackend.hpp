#pragma once

#include <Raiden/Engine/IImGuiBackend.hpp>
#include <Raiden/Renderer/Vulkan/IVulkanRenderDevice.hpp>
#include <volk.h>

namespace Raiden::Engine {

class VulkanImGuiBackend final : public IImGuiBackend {
public:
  VulkanImGuiBackend(::Raiden::Renderer::IVulkanRenderDevice &device, VkRenderPass renderPass,
                     uint32_t imageCount);
  ~VulkanImGuiBackend() override;

  bool init() override;
  void newFrame() override;
  void renderDrawData(::Raiden::Renderer::ICommandBuffer &cmd) override;
  void shutdown() override;
  void waitIdle() override;

private:
  ::Raiden::Renderer::IVulkanRenderDevice &device_;
  VkRenderPass renderPass_;
  uint32_t imageCount_;
  VkDevice vkDevice_ = VK_NULL_HANDLE;
  bool shutdown_ = true;
};

} // namespace Raiden::Engine
