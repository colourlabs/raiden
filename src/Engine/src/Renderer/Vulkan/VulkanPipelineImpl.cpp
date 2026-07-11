#include <Renderer/Vulkan/VulkanPipelineImpl.hpp>

namespace Raiden::Renderer {

VulkanPipelineImpl::~VulkanPipelineImpl() { shutdown(); }

bool VulkanPipelineImpl::init(VkDevice device, VkRenderPass renderPass,
                              const VulkanShader &vertShader,
                              const VulkanShader &fragShader,
                              const VertexInputDescription &vertexDesc,
                              VkPrimitiveTopology topology,
                              bool depthTestEnable, bool depthWriteEnable,
                              VkCompareOp depthCompareOp,
                              VkCullModeFlags cullMode,
                              VkSampleCountFlagBits sampleCount,
                              VkDescriptorSetLayout *setLayouts,
                              uint32_t setLayoutCount,
                              const BlendConfig &blendConfig) {
  return pipeline_.initDynamic(device, renderPass, vertShader, fragShader,
                               vertexDesc, topology, depthTestEnable,
                               depthWriteEnable, depthCompareOp, cullMode,
                               sampleCount, setLayouts, setLayoutCount,
                               blendConfig);
}

void VulkanPipelineImpl::shutdown() { pipeline_.shutdown(); }

} // namespace Raiden::Renderer
