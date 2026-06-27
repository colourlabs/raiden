#pragma once

#include <string_view>
#include <volk.h>

namespace Raiden::Core {

enum class ShaderStage {
  Vertex,
  Fragment,
  Compute
};

class VulkanShader {
public:
  VulkanShader() = default;
  ~VulkanShader();

  VulkanShader(const VulkanShader &) = delete;
  VulkanShader &operator=(const VulkanShader &) = delete;
  VulkanShader(VulkanShader &&) = delete;
  VulkanShader &operator=(VulkanShader &&) = delete;

  bool init(VkDevice device, ShaderStage stage, std::string_view spvPath,
            std::string_view entryPoint = "main");
  void shutdown();

  VkShaderModule module() const { return module_; }
  VkPipelineShaderStageCreateInfo stageCreateInfo() const { return stageInfo_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkShaderModule module_ = VK_NULL_HANDLE;
  VkPipelineShaderStageCreateInfo stageInfo_{};
};

}
