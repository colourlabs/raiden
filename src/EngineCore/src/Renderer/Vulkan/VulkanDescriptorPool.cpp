#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanDescriptorPool");

VulkanDescriptorPool::~VulkanDescriptorPool() { shutdown(); }

bool VulkanDescriptorPool::init(VkDevice device) {
  device_ = device;

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
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 2,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 3,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      {.binding = 4,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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

  // pool sized for: 3 frame UBOs + 1 shared sampler
  //               + 64 simple textures + 64 material texture sets + 64 material params UBOs
  std::array<VkDescriptorPoolSize, 4> poolSizes{{
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 + 64},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 * 5},
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
  if (fallbackImageView_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, fallbackImageView_, nullptr);
    fallbackImageView_ = VK_NULL_HANDLE;
  }
  if (fallbackImage_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_, fallbackImage_, nullptr);
    fallbackImage_ = VK_NULL_HANDLE;
  }
  if (fallbackMemory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, fallbackMemory_, nullptr);
    fallbackMemory_ = VK_NULL_HANDLE;
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

} // namespace Raiden::Core
