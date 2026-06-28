#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDescriptorPool.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanDescriptorPool");

VulkanDescriptorPool::~VulkanDescriptorPool() { shutdown(); }

bool VulkanDescriptorPool::init(VkDevice device) {
  device_ = device;

  // set 0: uniform buffer (binding 0)
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

  // set 1: combined image sampler (binding 0)
  VkDescriptorSetLayoutBinding samplerBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
    vkDestroyDescriptorSetLayout(device_, uboSetLayout_, nullptr);
    uboSetLayout_ = VK_NULL_HANDLE;
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
    vkDestroyDescriptorSetLayout(device_, uboSetLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, samplerSetLayout_, nullptr);
    uboSetLayout_ = VK_NULL_HANDLE;
    samplerSetLayout_ = VK_NULL_HANDLE;
    return false;
  }

  // pool: sized for 3 UBO sets + 64 texture sets
  std::array<VkDescriptorPoolSize, 2> poolSizes{{
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
  }};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 3 + 64,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_) !=
      VK_SUCCESS) {
    s_logger.error("Failed to create descriptor pool");
    vkDestroySampler(device_, sampler_, nullptr);
    vkDestroyDescriptorSetLayout(device_, uboSetLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, samplerSetLayout_, nullptr);
    sampler_ = VK_NULL_HANDLE;
    uboSetLayout_ = VK_NULL_HANDLE;
    samplerSetLayout_ = VK_NULL_HANDLE;
    return false;
  }

  s_logger.info("Descriptor pool created (3 UBO + 64 sampler sets).");
  return true;
}

void VulkanDescriptorPool::shutdown() {
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
  }
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  if (uboSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, uboSetLayout_, nullptr);
    uboSetLayout_ = VK_NULL_HANDLE;
  }
  if (samplerSetLayout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, samplerSetLayout_, nullptr);
    samplerSetLayout_ = VK_NULL_HANDLE;
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
