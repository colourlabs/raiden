#pragma once

#include <volk.h>

namespace Raiden::Core {

class VulkanDescriptorPool {
public:
  VulkanDescriptorPool() = default;
  ~VulkanDescriptorPool();

  VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
  VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;
  VulkanDescriptorPool(VulkanDescriptorPool &&) = delete;
  VulkanDescriptorPool &operator=(VulkanDescriptorPool &&) = delete;

  bool init(VkDevice device);
  void shutdown();

  VkDescriptorPool handle() const { return pool_; }

  // set 0, frame UBO
  VkDescriptorSetLayout uboSetLayout() const { return uboSetLayout_; }
  const VkDescriptorSetLayout *uboSetLayoutAddr() const {
    return &uboSetLayout_;
  }

  // set 1, sampler (shared, used by both simple and material paths)
  VkDescriptorSetLayout samplerSetLayout() const { return samplerSetLayout_; }
  VkDescriptorSet samplerSet() const { return samplerSet_; }

  // set 2, simple single texture (VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, binding 0)
  VkDescriptorSetLayout textureSetLayout() const { return textureSetLayout_; }

  // set 2, material textures (albedo, normal, metallicRoughness, emissive, occlusion)
  VkDescriptorSetLayout materialSetLayout() const { return materialSetLayout_; }
  const VkDescriptorSetLayout *materialSetLayoutAddr() const {
    return &materialSetLayout_;
  }

  // set 3, material params UBO
  VkDescriptorSetLayout materialParamsSetLayout() const {
    return materialParamsSetLayout_;
  }
  const VkDescriptorSetLayout *materialParamsSetLayoutAddr() const {
    return &materialParamsSetLayout_;
  }

  VkSampler sampler() const { return sampler_; }
  VkDevice device() const { return device_; }

  VkDescriptorSet allocSamplerSet();
  VkDescriptorImageInfo fallbackImageInfo() const;

private:
  bool createFallbackTexture();

  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout uboSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout samplerSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout materialParamsSetLayout_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSet samplerSet_ = VK_NULL_HANDLE;

  // 1x1 white fallback for unbound material texture slots
  VkImage fallbackImage_ = VK_NULL_HANDLE;
  VkImageView fallbackImageView_ = VK_NULL_HANDLE;
  VkDeviceMemory fallbackMemory_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Core