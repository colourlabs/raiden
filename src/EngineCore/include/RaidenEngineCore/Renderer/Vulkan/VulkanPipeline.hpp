#pragma once

#include <vector>
#include <volk.h>

namespace Raiden::Core {

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
                   const VertexInputDescription &vertexInput,
                   bool depthTestEnable, bool depthWriteEnable = true,
                   VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS,
                   VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                   VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
                   VkDescriptorSetLayout *setLayouts = nullptr,
                   uint32_t setLayoutCount = 0);
  void shutdown();

  VkPipeline pipeline() const { return pipeline_; }
  VkPipelineLayout layout() const { return layout_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Core