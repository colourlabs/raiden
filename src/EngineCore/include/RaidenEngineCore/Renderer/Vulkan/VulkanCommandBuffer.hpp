#pragma once

#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>
#include <volk.h>

namespace Raiden::Core {

class VulkanCommandBuffer final : public ICommandBuffer {
public:
  VulkanCommandBuffer(VkCommandBuffer cmd, VulkanDescriptorPool &pool,
                      VkDescriptorSet uboSet = VK_NULL_HANDLE)
      : cmd_(cmd), pool_(&pool), uboSet_(uboSet) {}

  void bindPipeline(const IPipeline &pipeline) override;
  void bindVertexBuffer(const IBuffer &buffer) override;
  void bindIndexBuffer(const IBuffer &buffer) override;
  void bindTexture(uint32_t slot, const ITexture &texture) override;
  void drawIndexed(uint32_t indexCount) override;

private:
  VkCommandBuffer cmd_ = VK_NULL_HANDLE;
  VulkanDescriptorPool *pool_ = nullptr;
  VkPipelineLayout currentLayout_ = VK_NULL_HANDLE;
  VkDescriptorSet uboSet_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Core
