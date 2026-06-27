#pragma once

#include <volk.h>
#include <vector>

namespace Raiden::Core {

class VulkanShader;

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
};

class VulkanPipeline {
public:
  VulkanPipeline() = default;
  ~VulkanPipeline();

  VulkanPipeline(const VulkanPipeline &) = delete;
  VulkanPipeline &operator=(const VulkanPipeline &) = delete;
  VulkanPipeline(VulkanPipeline &&) = delete;
  VulkanPipeline &operator=(VulkanPipeline &&) = delete;

  bool init(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
            const VulkanShader &vertexShader,
            const VulkanShader &fragmentShader,
            const VertexInputDescription &vertexInput);
  void shutdown();

  VkPipeline pipeline() const { return pipeline_; }
  VkPipelineLayout layout() const { return layout_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

}
