#pragma once

#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Renderer/Vulkan/VulkanDescriptorPool.hpp>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanCommandBuffer final : public ICommandBuffer {
public:
  VulkanCommandBuffer(VkCommandBuffer cmd, VulkanDescriptorPool &pool,
                      VkDescriptorSet uboSet = VK_NULL_HANDLE)
      : cmd_(cmd), pool_(&pool), uboSet_(uboSet) {}

  [[nodiscard]] VkCommandBuffer handle() const { return cmd_; }
  [[nodiscard]] uint32_t drawCalls() const { return drawCalls_; }
  [[nodiscard]] uint32_t triangles() const { return triangles_; }

  void bindPipeline(const IPipeline &pipeline) override;
  void bindVertexBuffer(const IBuffer &buffer) override;
  void bindIndexBuffer(const IBuffer &buffer) override;
  void bindTexture(uint32_t slot, const ITexture &texture) override;
  void drawIndexed(uint32_t indexCount) override;
  void setViewport(int x, int y, int w, int h) override;
  void setScissor(int x, int y, int w, int h) override;
  void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
            uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
  void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount = 1,
                            uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                            uint32_t firstInstance = 0) override;
  void pushConstants(uint32_t offset, uint32_t size,
                     const void *data) override;

  void setPipelineLayout(VkPipelineLayout layout) { currentLayout_ = layout; }

private:
  VkCommandBuffer cmd_ = VK_NULL_HANDLE;
  VulkanDescriptorPool *pool_ = nullptr;
  VkPipelineLayout currentLayout_ = VK_NULL_HANDLE;
  VkDescriptorSet uboSet_ = VK_NULL_HANDLE;
  uint32_t drawCalls_ = 0;
  uint32_t triangles_ = 0;
};

} // namespace Raiden::Renderer
