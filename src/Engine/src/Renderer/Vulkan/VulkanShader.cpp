#include <Raiden/Logger.hpp>
#include <Renderer/Vulkan/VulkanShader.hpp>

#include <fstream>
#include <vector>

namespace Raiden::Renderer {

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanShader");

VulkanShader::~VulkanShader() { shutdown(); }

bool VulkanShader::init(VkDevice device, ShaderStage stage,
                        std::string_view spvPath, std::string_view entryPoint) {
  device_ = device;

  std::ifstream file(spvPath.data(), std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    s_logger.critical("Failed to open shader SPIR-V: {}", spvPath);
    return false;
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(code.data()),
            static_cast<std::streamsize>(fileSize));
  file.close();

  VkShaderModuleCreateInfo moduleInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size() * sizeof(uint32_t),
      .pCode = code.data(),
  };

  if (vkCreateShaderModule(device_, &moduleInfo, nullptr, &module_) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to create shader module from: {}", spvPath);
    return false;
  }

  VkShaderStageFlagBits vkStage{};
  switch (stage) {
  case ShaderStage::Vertex:
    vkStage = VK_SHADER_STAGE_VERTEX_BIT;
    break;
  case ShaderStage::Fragment:
    vkStage = VK_SHADER_STAGE_FRAGMENT_BIT;
    break;
  case ShaderStage::Compute:
    vkStage = VK_SHADER_STAGE_COMPUTE_BIT;
    break;
  }

  stageInfo_.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo_.stage = vkStage;
  stageInfo_.module = module_;
  stageInfo_.pName = entryPoint.data();

  s_logger.info("Shader loaded: {}", spvPath);
  return true;
}

void VulkanShader::shutdown() {
  if (module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device_, module_, nullptr);
    module_ = VK_NULL_HANDLE;
  }
}

} // namespace Raiden::Renderer
