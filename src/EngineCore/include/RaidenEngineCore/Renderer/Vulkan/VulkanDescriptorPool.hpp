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
  VkDescriptorSetLayout uboSetLayout() const { return uboSetLayout_; }
  const VkDescriptorSetLayout *uboSetLayoutAddr() const { return &uboSetLayout_; }
  VkDescriptorSetLayout samplerSetLayout() const { return samplerSetLayout_; }
  VkSampler sampler() const { return sampler_; }
  VkDevice device() const { return device_; }

  VkDescriptorSet allocSamplerSet();

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout uboSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout samplerSetLayout_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Core
