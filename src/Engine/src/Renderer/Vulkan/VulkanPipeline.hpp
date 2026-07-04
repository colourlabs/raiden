#pragma once

#include <vector>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanShader;

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
};

class VulkanPipeline {
public:
  VulkanPipeline() = default;
  ~VulkanPipeline();

  VulkanPipeline(const VulkanPipeline &) = delete;
  VulkanPipeline &operator=(const VulkanPipeline &) = delete;
  VulkanPipeline(VulkanPipeline &&) = delete;
  VulkanPipeline &operator=(VulkanPipeline &&) = delete;

  bool initDynamic(VkDevice device, VkRenderPass renderPass,
                   const VulkanShader &vertexShader,
                   const VulkanShader &fragmentShader,
                   const VertexInputDescription &vertexDesc,
                   bool depthTestEnable, bool depthWriteEnable,
                   VkCompareOp depthCompareOp, VkCullModeFlags cullMode,
                   VkSampleCountFlagBits sampleCount,
                   VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount);

  void shutdown();

  [[nodiscard]] VkPipeline pipeline() const { return pipeline_; }
  [[nodiscard]] VkPipelineLayout layout() const { return layout_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Renderer