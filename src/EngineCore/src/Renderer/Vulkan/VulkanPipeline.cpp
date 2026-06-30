#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanPipeline.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanShader.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanPipeline");

VulkanPipeline::~VulkanPipeline() { shutdown(); }

bool VulkanPipeline::initDynamic(VkDevice device, VkRenderPass renderPass,
                                 const VulkanShader &vertexShader,
                                 const VulkanShader &fragmentShader,
                                 const VertexInputDescription &vertexDesc,
                                 bool depthTestEnable,
                                 VkDescriptorSetLayout *setLayouts,
                                 uint32_t setLayoutCount) {
  device_ = device;

  VkPipelineShaderStageCreateInfo stages[] = {vertexShader.stageCreateInfo(),
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
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
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
      .depthWriteEnable = depthTestEnable ? VK_TRUE : VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamicStates,
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
      .pStages = stages,
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

} // namespace Raiden::Core
