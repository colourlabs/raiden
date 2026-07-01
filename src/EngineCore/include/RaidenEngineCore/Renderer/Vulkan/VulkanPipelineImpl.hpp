#pragma once

#include <RaidenEngineCore/Renderer/IPipeline.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipeline.hpp>

namespace Raiden::Core {

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
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
            VkDescriptorSetLayout *setLayouts = nullptr,
            uint32_t setLayoutCount = 0);
  void shutdown();

  VkPipeline handle() const { return pipeline_.pipeline(); }
  VkPipelineLayout layout() const { return pipeline_.layout(); }

private:
  VulkanPipeline pipeline_;
};

} // namespace Raiden::Core
