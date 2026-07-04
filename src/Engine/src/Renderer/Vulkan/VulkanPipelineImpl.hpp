#pragma once

#include <Raiden/Renderer/IPipeline.hpp>
#include <Renderer/Vulkan/VulkanPipeline.hpp>

namespace Raiden::Renderer {

class VulkanPipelineImpl final : public IPipeline {
public:
  VulkanPipelineImpl() = default;
  ~VulkanPipelineImpl() override;

  VulkanPipelineImpl(const VulkanPipelineImpl &) = delete;
  VulkanPipelineImpl &operator=(const VulkanPipelineImpl &) = delete;
  VulkanPipelineImpl(VulkanPipelineImpl &&) = delete;
  VulkanPipelineImpl &operator=(VulkanPipelineImpl &&) = delete;

  bool init(VkDevice device, VkRenderPass renderPass,
            const VulkanShader &vertShader, const VulkanShader &fragShader,
            const VertexInputDescription &vertexDesc, bool depthTestEnable,
            bool depthWriteEnable = true,
            VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS,
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
            VkDescriptorSetLayout *setLayouts = nullptr,
            uint32_t setLayoutCount = 0,
            bool blendEnable = false);
  void shutdown();

  [[nodiscard]] VkPipeline handle() const { return pipeline_.pipeline(); }
  [[nodiscard]] VkPipelineLayout layout() const { return pipeline_.layout(); }

private:
  VulkanPipeline pipeline_;
};

} // namespace Raiden::Renderer
