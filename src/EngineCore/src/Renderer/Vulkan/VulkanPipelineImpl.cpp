#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipelineImpl.hpp>

namespace Raiden::Core {

VulkanPipelineImpl::~VulkanPipelineImpl() { shutdown(); }

bool VulkanPipelineImpl::init(VkDevice device, VkRenderPass renderPass,
                              const VulkanShader &vertShader,
                              const VulkanShader &fragShader,
                              const VertexInputDescription &vertexDesc,
                              bool depthTestEnable,
                              VkSampleCountFlagBits sampleCount,
                              VkDescriptorSetLayout *setLayouts,
                              uint32_t setLayoutCount) {
  return pipeline_.initDynamic(device, renderPass, vertShader, fragShader,
                               vertexDesc, depthTestEnable, sampleCount,
                               setLayouts, setLayoutCount);
}

void VulkanPipelineImpl::shutdown() { pipeline_.shutdown(); }

} // namespace Raiden::Core
