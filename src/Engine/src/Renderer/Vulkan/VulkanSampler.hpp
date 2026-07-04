#pragma once

#include <Raiden/Renderer/ISampler.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>
#include <volk.h>

namespace Raiden::Renderer {

class VulkanSampler final : public ISampler {
public:
  VulkanSampler() = default;
  ~VulkanSampler() override;

  VulkanSampler(const VulkanSampler &) = delete;
  VulkanSampler &operator=(const VulkanSampler &) = delete;
  VulkanSampler(VulkanSampler &&) = delete;
  VulkanSampler &operator=(VulkanSampler &&) = delete;

  bool init(VkDevice device, const SamplerDesc &desc);
  void shutdown();

  [[nodiscard]] VkSampler handle() const { return sampler_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
};

} // namespace Raiden::Renderer
