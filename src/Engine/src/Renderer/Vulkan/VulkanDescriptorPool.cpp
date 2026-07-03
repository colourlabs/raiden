#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanDescriptorPool.hpp>

#include <array>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanDescriptorPool");

VulkanDescriptorPool::~VulkanDescriptorPool() { shutdown(); }

bool VulkanDescriptorPool::init(VkDevice device, VkPhysicalDevice physDev,
                                VmaAllocator allocator,
                                VkCommandPool transferPool,
                                VkQueue graphicsQueue) {
  allocator_ = allocator;
  device_ = device;
  physDev_ = physDev;
  transferPool_ = transferPool;
  graphicsQueue_ = graphicsQueue;

  // set 0, frame UBO
  VkDescriptorSetLayoutBinding uboBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  VkDescriptorSetLayoutCreateInfo uboLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &uboBinding,
  };
  if (vkCreateDescriptorSetLayout(device_, &uboLayoutInfo, nullptr,
                                  &uboSetLayout_) != VK_SUCCESS) {
    s_logger.error("Failed to create UBO descriptor set layout");
    return false;
  }

  // set 1, shared sampler (VK_DESCRIPTOR_TYPE_SAMPLER)
  VkDescriptorSetLayoutBinding samplerBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &samplerBinding,
  };
  if (vkCreateDescriptorSetLayout(device_, &samplerLayoutInfo, nullptr,
                                  &samplerSetLayout_) != VK_SUCCESS) {
    s_logger.error("Failed to create sampler descriptor set layout");
    return false;
  }

  // set 2, simple single texture (VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, binding 0)
  VkDescriptorSetLayoutBinding textureBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  VkDescriptorSetLayoutCreateInfo textureLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &textureBinding,
  };
  if (vkCreateDescriptorSetLayout(device_, &textureLayoutInfo, nullptr,
                                  &textureSetLayout_) != VK_SUCCESS) {
    s_logger.error("Failed to create texture descriptor set layout");
    return false;
  }

  // set 2, material textures (5 slots: albedo, normal, metallicRoughness,
  // emissive, occlusion)
  std::array<VkDescriptorSetLayoutBinding, 5> materialBindings{{
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 2,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 3,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 4,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
  }};

  VkDescriptorSetLayoutCreateInfo materialLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(materialBindings.size()),
      .pBindings = materialBindings.data(),
  };

  if (vkCreateDescriptorSetLayout(device_, &materialLayoutInfo, nullptr,
                                  &materialSetLayout_) != VK_SUCCESS) {
    s_logger.error("Failed to create material descriptor set layout");
    return false;
  }

  // set 3, material params UBO
  VkDescriptorSetLayoutBinding materialParamsBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutCreateInfo materialParamsLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &materialParamsBinding,
  };

  if (vkCreateDescriptorSetLayout(device_, &materialParamsLayoutInfo, nullptr,
                                  &materialParamsSetLayout_) != VK_SUCCESS) {
    s_logger.error("Failed to create material params descriptor set layout");
    return false;
  }

  // sampler
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 16.0f,
  };

  if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create sampler");
    return false;
  }

  // clamp-to-edge sampler for cubemaps
  VkSamplerCreateInfo clampSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 16.0f,
  };

  if (vkCreateSampler(device_, &clampSamplerInfo, nullptr, &clampSampler_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create clamp sampler");
    return false;
  }

  // pool sized for: 3 frame UBOs + 1 shared sampler + 1 clamp sampler
  //               + 64 simple textures + 64 material texture sets + 64 material
  //               params UBOs
  std::array<VkDescriptorPoolSize, 3> poolSizes{{
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 + 64},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 2},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64 + 64 * 5},
  }};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 3 + 1 + 64 + 64 + 64,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create descriptor pool");
    return false;
  }

  // pre-allocate and write the shared sampler descriptor set (set 1)
  VkDescriptorSetAllocateInfo sampAlloc{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &samplerSetLayout_,
  };
  if (vkAllocateDescriptorSets(device_, &sampAlloc, &samplerSet_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to allocate sampler descriptor set");
    return false;
  }

  VkDescriptorImageInfo samplerImageInfo{
      .sampler = sampler_,
      .imageView = VK_NULL_HANDLE,
      .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkWriteDescriptorSet samplerWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = samplerSet_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = &samplerImageInfo,
  };
  vkUpdateDescriptorSets(device_, 1, &samplerWrite, 0, nullptr);

  // clamp sampler descriptor set
  VkDescriptorSetAllocateInfo clampAlloc{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &samplerSetLayout_,
  };
  if (vkAllocateDescriptorSets(device_, &clampAlloc, &clampSamplerSet_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to allocate clamp sampler descriptor set");
    return false;
  }

  VkDescriptorImageInfo clampImageInfo{
      .sampler = clampSampler_,
      .imageView = VK_NULL_HANDLE,
      .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkWriteDescriptorSet clampWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = clampSamplerSet_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = &clampImageInfo,
  };
  vkUpdateDescriptorSets(device_, 1, &clampWrite, 0, nullptr);

  if (!createFallbackTexture()) {
    s_logger.error("Failed to create fallback texture");
    return false;
  }

  s_logger.info("Descriptor pool created.");
  return true;
}

VkDescriptorImageInfo VulkanDescriptorPool::fallbackImageInfo() const {
  return {
      .sampler = sampler_,
      .imageView = fallbackImageView_,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
}

void VulkanDescriptorPool::shutdown() {
  // This is not verbose yes.
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
  }
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  if (clampSampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, clampSampler_, nullptr);
    clampSampler_ = VK_NULL_HANDLE;
  }
  if (fallbackImageView_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, fallbackImageView_, nullptr);
    fallbackImageView_ = VK_NULL_HANDLE;
  }
  if (fallbackImage_ != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator_, fallbackImage_, fallbackAllocation_);
    fallbackImage_ = VK_NULL_HANDLE;
    fallbackAllocation_ = VK_NULL_HANDLE;
  }
  if (uboSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, uboSetLayout_, nullptr);
    uboSetLayout_ = VK_NULL_HANDLE;
  }
  if (samplerSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, samplerSetLayout_, nullptr);
    samplerSetLayout_ = VK_NULL_HANDLE;
  }
  if (textureSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, textureSetLayout_, nullptr);
    textureSetLayout_ = VK_NULL_HANDLE;
  }
  if (materialSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, materialSetLayout_, nullptr);
    materialSetLayout_ = VK_NULL_HANDLE;
  }
  if (materialParamsSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, materialParamsSetLayout_, nullptr);
    materialParamsSetLayout_ = VK_NULL_HANDLE;
  }
}

bool VulkanDescriptorPool::createFallbackTexture() {
  VkImageCreateInfo imgInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {1, 1, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo imgAllocInfo{
      .usage = VMA_MEMORY_USAGE_AUTO,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };

  if (vmaCreateImage(allocator_, &imgInfo, &imgAllocInfo, &fallbackImage_,
                     &fallbackAllocation_, nullptr) != VK_SUCCESS) {
    s_logger.error("Failed to create fallback image");
    return false;
  }

  // write white pixel via staging buffer
  VkBufferCreateInfo bufInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = 4,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  };

  VkBuffer stagingBuf = VK_NULL_HANDLE;
  VmaAllocation stagingAlloc = VK_NULL_HANDLE;
  VmaAllocationInfo stagingAllocInfo{};

  VmaAllocationCreateInfo stagingCreateInfo{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };

  if (vmaCreateBuffer(allocator_, &bufInfo, &stagingCreateInfo, &stagingBuf,
                      &stagingAlloc, &stagingAllocInfo) != VK_SUCCESS) {
    s_logger.error("Failed to create staging buffer");
    return false;
  }

  auto *data = static_cast<uint8_t *>(stagingAllocInfo.pMappedData);
  data[0] = 255;
  data[1] = 255;
  data[2] = 255;
  data[3] = 255;

  // create image view
  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = fallbackImage_,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };

  if (vkCreateImageView(device_, &viewInfo, nullptr, &fallbackImageView_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create fallback image view");
    vmaDestroyBuffer(allocator_, stagingBuf, stagingAlloc);
    return false;
  }

  // one-shot command buffer for layout transition + copy
  VkCommandBufferAllocateInfo allocCmd{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = transferPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(device_, &allocCmd, &cmd);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &beginInfo);

  // UNDEFINED -> TRANSFER_DST
  VkImageMemoryBarrier preBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = fallbackImage_,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &preBarrier);

  VkBufferImageCopy copyRegion{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {1, 1, 1},
  };
  vkCmdCopyBufferToImage(cmd, stagingBuf, fallbackImage_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  // TRANSFER_DST -> SHADER_READ_ONLY
  VkImageMemoryBarrier postBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = fallbackImage_,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &postBarrier);

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };
  vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue_);

  vkFreeCommandBuffers(device_, transferPool_, 1, &cmd);
  vmaDestroyBuffer(allocator_, stagingBuf, stagingAlloc);

  s_logger.info("Fallback 1x1 white texture created.");
  return true;
}

VkDescriptorSet VulkanDescriptorPool::allocSamplerSet() {
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &samplerSetLayout_,
  };

  VkDescriptorSet set = VK_NULL_HANDLE;
  if (vkAllocateDescriptorSets(device_, &allocInfo, &set) != VK_SUCCESS) {
    s_logger.error("Failed to allocate sampler descriptor set");
    return VK_NULL_HANDLE;
  }
  return set;
}

} // namespace Raiden::Renderer
