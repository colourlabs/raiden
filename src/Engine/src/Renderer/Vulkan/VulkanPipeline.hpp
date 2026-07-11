#pragma once

#include <vector>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanShader;

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
};

struct BlendConfig {
  bool blendEnable = false;
  VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
  VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
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
                   VkPrimitiveTopology topology,
                   bool depthTestEnable, bool depthWriteEnable,
                   VkCompareOp depthCompareOp, VkCullModeFlags cullMode,
                   VkSampleCountFlagBits sampleCount,
                   VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount,
                   const BlendConfig &blendConfig = {});

  void shutdown();

  [[nodiscard]] VkPipeline pipeline() const { return pipeline_; }
  [[nodiscard]] VkPipelineLayout layout() const { return layout_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Renderer