#include <RaidenEngineCore/Renderer/Vulkan/VulkanBufferImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanCommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipelineImpl.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanTextureImpl.hpp>

namespace Raiden::Core {

void VulkanCommandBuffer::bindPipeline(const IPipeline &pipeline) {
  auto &vkPipeline = static_cast<const VulkanPipelineImpl &>(pipeline);
  vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline.handle());
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

void VulkanCommandBuffer::bindTexture(uint32_t slot, const ITexture &texture) {
  if (currentLayout_ == VK_NULL_HANDLE)
    return;

  auto &vkTex = static_cast<const VulkanTextureImpl &>(texture);

  // bind shared sampler at set 1
  VkDescriptorSet samplerSet = pool_->samplerSet();
  vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, currentLayout_,
                          1, 1, &samplerSet, 0, nullptr);

  // bind texture (sampled image) at set 2, binding 0
  VkDescriptorSet texSet = vkTex.getOrCreateDescriptorSet(
      pool_->device(), pool_->handle(), pool_->textureSetLayout(),
      pool_->sampler());

  if (texSet == VK_NULL_HANDLE)
    return;

  vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, currentLayout_,
                          2, 1, &texSet, 0, nullptr);
}

void VulkanCommandBuffer::drawIndexed(uint32_t indexCount) {
  vkCmdDrawIndexed(cmd_, indexCount, 1, 0, 0, 0);
  drawCalls_++;
  triangles_ += indexCount / 3;
}

void VulkanCommandBuffer::setViewport(int x, int y, int w, int h) {
  VkViewport vp{};
  vp.x = static_cast<float>(x);
  vp.y = static_cast<float>(y);
  vp.width = static_cast<float>(w);
  vp.height = static_cast<float>(h);
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmd_, 0, 1, &vp);
}

void VulkanCommandBuffer::setScissor(int x, int y, int w, int h) {
  VkRect2D rect{};
  rect.offset.x = x;
  rect.offset.y = y;
  rect.extent.width = static_cast<uint32_t>(w);
  rect.extent.height = static_cast<uint32_t>(h);
  vkCmdSetScissor(cmd_, 0, 1, &rect);
}

void VulkanCommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount,
                               uint32_t firstVertex, uint32_t firstInstance) {
  vkCmdDraw(cmd_, vertexCount, instanceCount, firstVertex, firstInstance);
  drawCalls_++;
}

void VulkanCommandBuffer::drawIndexedInstanced(uint32_t indexCount,
                                               uint32_t instanceCount,
                                               uint32_t firstIndex,
                                               int32_t vertexOffset,
                                               uint32_t firstInstance) {
  vkCmdDrawIndexed(cmd_, indexCount, instanceCount, firstIndex, vertexOffset,
                   firstInstance);
  drawCalls_++;
  triangles_ += indexCount / 3 * instanceCount;
}

void VulkanCommandBuffer::pushConstants(uint32_t offset, uint32_t size,
                                        const void *data) {
  if (currentLayout_ == VK_NULL_HANDLE)
    return;
  vkCmdPushConstants(cmd_, currentLayout_, VK_SHADER_STAGE_VERTEX_BIT, offset,
                     size, data);
}

} // namespace Raiden::Core
