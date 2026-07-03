#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanRenderPass.hpp>

#include <array>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanRenderPass");

bool VulkanRenderPass::init(VkDevice device, VkFormat imageFormat,
                            VkFormat depthFormat,
                            VkSampleCountFlagBits sampleCount) {
  device_ = device;
  depthFormat_ = depthFormat;
  sampleCount_ = sampleCount;

  bool msaa = sampleCount != VK_SAMPLE_COUNT_1_BIT;

  // attachments
  // with MSAA:  [0] MSAA color, [1] MSAA depth, [2] resolve color
  // without:    [0] color, [1] depth
  std::array<VkAttachmentDescription, 3> attachments{};
  uint32_t attachmentCount = 1;

  // color attachment
  attachments[0] = {
      .format = imageFormat,
      .samples = sampleCount,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                      : VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                          : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorRef{
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  // resolve attachment (only used with MSAA)
  VkAttachmentReference resolveRef{};
  if (msaa) {
    attachments[2] = {
        .format = imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    resolveRef.attachment = 2;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentCount = 3;
  }

  VkAttachmentDescription depthAttachment{};
  VkAttachmentReference depthRef{};
  if (depthFormat != VK_FORMAT_UNDEFINED) {
    attachments[msaa ? 1u : 1u] = {
        .format = depthFormat,
        .samples = sampleCount,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    depthRef.attachment = msaa ? 1u : 1u;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentCount = msaa ? 3u : 2u;
  }

  // subpass
  VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorRef,
  };
  if (msaa) {
    subpass.pResolveAttachments = &resolveRef;
  }
  if (depthFormat != VK_FORMAT_UNDEFINED) {
    subpass.pDepthStencilAttachment = &depthRef;
  }

  // dependencies
  VkSubpassDependency dependencies[2]{};
  uint32_t dependencyCount = 1;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = 0;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  if (depthFormat != VK_FORMAT_UNDEFINED) {
    dependencies[0].srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask |=
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencyCount = 2;
  }

  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = attachmentCount,
      .pAttachments = attachments.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = dependencyCount,
      .pDependencies = dependencies,
  };

  if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to create render pass");
    return false;
  }

  s_logger.info("Render pass created (samples={}).",
                static_cast<int>(sampleCount));
  return true;
}

void VulkanRenderPass::shutdown() {
  if (renderPass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
  }
}

} // namespace Raiden::Renderer
