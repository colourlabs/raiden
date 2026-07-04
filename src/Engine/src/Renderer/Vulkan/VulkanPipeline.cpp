#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanPipeline.hpp>
#include <Renderer/Vulkan/VulkanShader.hpp>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanPipeline");

VulkanPipeline::~VulkanPipeline() { shutdown(); }

bool VulkanPipeline::initDynamic(VkDevice device, VkRenderPass renderPass,
                                 const VulkanShader &vertexShader,
                                 const VulkanShader &fragmentShader,
                                 const VertexInputDescription &vertexDesc,
                                 bool depthTestEnable, bool depthWriteEnable,
                                 VkCompareOp depthCompareOp,
                                 VkCullModeFlags cullMode,
                                 VkSampleCountFlagBits sampleCount,
                                 VkDescriptorSetLayout *setLayouts,
                                 uint32_t setLayoutCount,
                                 bool blendEnable) {
  device_ = device;

  std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertexShader.stageCreateInfo(),
                                                           fragmentShader.stageCreateInfo()};

  VkPipelineVertexInputStateCreateInfo vertexInput{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount =
          static_cast<uint32_t>(vertexDesc.bindings.size()),
      .pVertexBindingDescriptions = vertexDesc.bindings.data(),
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(vertexDesc.attributes.size()),
      .pVertexAttributeDescriptions = vertexDesc.attributes.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };

  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = cullMode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0F,
  };

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = sampleCount,
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = blendEnable ? VK_TRUE : VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE,
      .depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE,
      .depthCompareOp = depthCompareOp,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamicStates.data(),
  };

  VkPushConstantRange pushRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = 64, // size of glm:mat4
  };

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = setLayoutCount,
      .pSetLayouts = setLayouts,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushRange,
  };

  if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to create pipeline layout");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = stages.data(),
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = layout_,
      .renderPass = renderPass,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline_) != VK_SUCCESS) {
    s_logger.critical("Failed to create graphics pipeline");
    return false;
  }

  s_logger.info("Pipeline created (dynamic viewport)");
  return true;
}

void VulkanPipeline::shutdown() {
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }

  if (layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, layout_, nullptr);
    layout_ = VK_NULL_HANDLE;
  }
}

} // namespace Raiden::Renderer
