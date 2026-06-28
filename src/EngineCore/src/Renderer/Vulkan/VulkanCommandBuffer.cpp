#include <RaidenEngineCore/Renderer/Vulkan/VulkanBufferImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanCommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipelineImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanTextureImpl.hpp>

namespace Raiden::Core {

void VulkanCommandBuffer::bindPipeline(const IPipeline &pipeline) {
  auto &vkPipeline = static_cast<const VulkanPipelineImpl &>(pipeline);
  vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    vkPipeline.handle());
  currentLayout_ = vkPipeline.layout();

  if (uboSet_ != VK_NULL_HANDLE) {
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            currentLayout_, 0, 1, &uboSet_, 0, nullptr);
  }
}

void VulkanCommandBuffer::bindVertexBuffer(const IBuffer &buffer) {
  auto &vkBuffer = static_cast<const VulkanBufferImpl &>(buffer);
  VkBuffer vkbuf = vkBuffer.handle();
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd_, 0, 1, &vkbuf, &offset);
}

void VulkanCommandBuffer::bindIndexBuffer(const IBuffer &buffer) {
  auto &vkBuffer = static_cast<const VulkanBufferImpl &>(buffer);
  vkCmdBindIndexBuffer(cmd_, vkBuffer.handle(), 0, VK_INDEX_TYPE_UINT16);
}

void VulkanCommandBuffer::bindTexture(uint32_t slot,
                                       const ITexture &texture) {
  if (currentLayout_ == VK_NULL_HANDLE)
    return;

  auto &vkTex = static_cast<const VulkanTextureImpl &>(texture);
  VkDescriptorSet set = vkTex.getOrCreateDescriptorSet(
      pool_->device(), pool_->handle(), pool_->samplerSetLayout(),
      pool_->sampler());

  if (set == VK_NULL_HANDLE)
    return;

  vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          currentLayout_, slot + 1, 1, &set, 0, nullptr);
}

void VulkanCommandBuffer::drawIndexed(uint32_t indexCount) {
  vkCmdDrawIndexed(cmd_, indexCount, 1, 0, 0, 0);
}

} // namespace Raiden::Core
