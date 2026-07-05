#include <Raiden/ECS/Camera.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Renderer/Vulkan/VulkanBufferImpl.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>
#include <Renderer/Vulkan/VulkanSampler.hpp>
#include <Renderer/Vulkan/VulkanMaterial.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

namespace Raiden::Renderer {

using namespace ::Raiden::Core;
using namespace ::Raiden::ECS;
using namespace ::Raiden::Platform;
using namespace ::Raiden::Jobs;

static const ::Raiden::Core::Logger s_logger("Raiden::Renderer::VulkanDevice");

static VkSampleCountFlagBits toVkSampleCount(Antialiasing aa) {
  switch (aa) {
  case Antialiasing::MSAAx2:
    return VK_SAMPLE_COUNT_2_BIT;
  case Antialiasing::MSAAx4:
    return VK_SAMPLE_COUNT_4_BIT;
  case Antialiasing::MSAAx8:
    return VK_SAMPLE_COUNT_8_BIT;
  default:
    return VK_SAMPLE_COUNT_1_BIT;
  }
}

static VkCullModeFlags toVkCullMode(CullMode mode) {
  switch (mode) {
  case CullMode::None:
    return VK_CULL_MODE_NONE;
  case CullMode::Front:
    return VK_CULL_MODE_FRONT_BIT;
  case CullMode::Back:
    return VK_CULL_MODE_BACK_BIT;
  }

  return VK_CULL_MODE_BACK_BIT;
}

static VkBlendFactor toVkBlendFactor(BlendFactor factor) {
  switch (factor) {
  case BlendFactor::Zero:
    return VK_BLEND_FACTOR_ZERO;
  case BlendFactor::One:
    return VK_BLEND_FACTOR_ONE;
  case BlendFactor::SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case BlendFactor::OneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case BlendFactor::DstAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case BlendFactor::OneMinusDstAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case BlendFactor::SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case BlendFactor::OneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case BlendFactor::DstColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case BlendFactor::OneMinusDstColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case BlendFactor::Src1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case BlendFactor::OneMinusSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  }
  return VK_BLEND_FACTOR_ZERO;
}

static VkBlendOp toVkBlendOp(BlendOp op) {
  switch (op) {
  case BlendOp::Add:
    return VK_BLEND_OP_ADD;
  case BlendOp::Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case BlendOp::ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  case BlendOp::Min:
    return VK_BLEND_OP_MIN;
  case BlendOp::Max:
    return VK_BLEND_OP_MAX;
  }
  return VK_BLEND_OP_ADD;
}

VulkanDevice::~VulkanDevice() { shutdown(); }

bool VulkanDevice::init(const EngineConfig &config, IPlatform *platform) {
  platform_ = platform;
  config_ = config;

  if (volkInitialize() != VK_SUCCESS) {
    s_logger.critical("Failed to initialize volk");
    return false;
  }

  enableValidation_ = config.enableValidation && checkValidationSupport();

  std::vector<const char *> extensions =
      platform->getRequiredInstanceExtensions();

  if (enableValidation_) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "raiden",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "raiden engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_4,
  };

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
  };

  if (enableValidation_) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers_.size());
    createInfo.ppEnabledLayerNames = validationLayers_.data();

    debugCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
        .pUserData = nullptr,
    };

    createInfo.pNext = &debugCreateInfo;
  }

  if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
    s_logger.critical("Failed to create Vulkan instance");
    return false;
  }

  volkLoadInstanceOnly(instance_);

  if (enableValidation_) {
    if (createDebugUtilsMessengerEXT(instance_, &debugCreateInfo, nullptr,
                                     &debugMessenger_) != VK_SUCCESS) {
      s_logger.error("Failed to set up debug messenger");
    }
  }

  if (!platform->createVulkanSurface(instance_, &surface_)) {
    s_logger.critical("Failed to create Vulkan surface");
    return false;
  }

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
  if (deviceCount == 0) {
    s_logger.critical("No Vulkan-capable GPUs found");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (isDeviceSuitable(device, surface_)) {
      physicalDevice_ = device;
      break;
    }
  }

  if (physicalDevice_ == VK_NULL_HANDLE) {
    s_logger.critical("No suitable GPU found");
    return false;
  }

  auto indices = findQueueFamilies(physicalDevice_, surface_);

  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  float queuePriority = 1.0F;
  queueCreateInfos.reserve(uniqueQueueFamilies.size());

  for (uint32_t family : uniqueQueueFamilies) {
    queueCreateInfos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    });
  }

  VkPhysicalDeviceFeatures deviceFeatures{
      .dualSrcBlend = VK_TRUE,
      .samplerAnisotropy = VK_TRUE,
  };

  VkDeviceCreateInfo deviceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions_.size()),
      .ppEnabledExtensionNames = deviceExtensions_.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_) !=
      VK_SUCCESS) {
    s_logger.critical("Failed to create logical device");
    return false;
  }

  volkLoadDevice(device_);

  if (!allocator_.init(instance_, physicalDevice_, device_)) {
    s_logger.critical("Failed to create VMA allocator");
    return false;
  }

  graphicsQueueIndex_ = indices.graphicsFamily.value();
  presentQueueIndex_ = indices.presentFamily.value();
  vkGetDeviceQueue(device_, graphicsQueueIndex_, 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, presentQueueIndex_, 0, &presentQueue_);

  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(physicalDevice_, &props);
  timestampPeriod_ = props.limits.timestampPeriod;

  // resolve MSAA sample count
  sampleCount_ = toVkSampleCount(config_.antialiasing);
  
  if (sampleCount_ > VK_SAMPLE_COUNT_1_BIT) {
    VkSampleCountFlags colorCounts = props.limits.framebufferColorSampleCounts;
    VkSampleCountFlags depthCounts = props.limits.framebufferDepthSampleCounts;

    if (((colorCounts & sampleCount_) == 0U) || ((depthCounts & sampleCount_) == 0U)) {
      s_logger.warn("MSAAx{} not supported, falling back to no MSAA",
                    static_cast<int>(sampleCount_));
      sampleCount_ = VK_SAMPLE_COUNT_1_BIT;
      config_.antialiasing = Antialiasing::None;
    }
  }

  VkQueryPoolCreateInfo qpInfo{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = kMaxFrames * 2,
  };
  vkCreateQueryPool(device_, &qpInfo, nullptr, &queryPool_);

  int windowWidth = 0, windowHeight = 0;
  platform->getWindowSize(windowWidth, windowHeight);

  if (!swapchain_.init(
          physicalDevice_, device_, surface_, indices.graphicsFamily.value(),
          indices.presentFamily.value(), static_cast<uint32_t>(windowWidth),
          static_cast<uint32_t>(windowHeight), config.window.vsync)) {
    s_logger.critical("Failed to create swapchain");
    return false;
  }

  depthFormat_ = chooseDepthFormat();

  if (!renderPass_.init(device_, swapchain_.imageFormat(), depthFormat_,
                        sampleCount_)) {
    s_logger.critical("Failed to create render pass");
    return false;
  }

  if (!depthImage_.init(device_, allocator_.handle(), swapchain_.extent().width,
                        swapchain_.extent().height, depthFormat_,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                        VK_IMAGE_ASPECT_DEPTH_BIT, sampleCount_)) {
    s_logger.critical("Failed to create depth image");
    return false;
  }

  if (!createFramebuffers()) {
    s_logger.critical("Failed to create framebuffers");
    return false;
  }

  VkCommandPoolCreateInfo transferPoolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = graphicsQueueIndex_,
  };
  if (vkCreateCommandPool(device_, &transferPoolInfo, nullptr,
                          &transferPool_) != VK_SUCCESS) {
    s_logger.critical("Failed to create transfer command pool");
    return false;
  }

  // per-worker secondary command pools created lazily in drawFrame

  if (!descriptorPool_.init(device_, physicalDevice_, allocator_.handle(),
                            transferPool_, graphicsQueue_)) {
    s_logger.critical("Failed to create descriptor pool");
    return false;
  }

  for (auto &[uniformBuffer, uniformSet] : perFrame_) {
    uniformBuffer.init(allocator_.handle(), sizeof(FrameUniforms),
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    uniformBuffer.map();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_.handle(),
        .descriptorSetCount = 1,
        .pSetLayouts = descriptorPool_.uboSetLayoutAddr(),
    };
    vkAllocateDescriptorSets(device_, &allocInfo, &uniformSet);

    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer.buffer(),
        .offset = 0,
        .range = sizeof(FrameUniforms),
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = uniformSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &bufferInfo,
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }

  auto const imageCount = static_cast<uint32_t>(swapchain_.imageViews().size());

  if (!frameContext_.init(device_, graphicsQueueIndex_, imageCount)) {
    s_logger.critical("Failed to create frame context");
    return false;
  }

  frameSecondaries_.resize(imageCount);

  // initialise frame timing
  lastFrameTime_ = std::chrono::steady_clock::now();
  frameIndex_ = 0;
  totalTime_ = 0.0F;

  for (auto &b : timestampReady_) {
    b = false;
  }

  s_logger.info("Vulkan device initialized.");
  return true;
}

std::unique_ptr<IPipeline>
VulkanDevice::createPipeline(const PipelineDesc &desc) {
  // resolve shader path: "shaders/thing.slang" -> "thing.vert.spv"
  namespace fs = std::filesystem;
  fs::path slangPath(desc.shader.path);
  std::string stem = slangPath.stem().string();

  std::string shadersDir = SHADERS_DIR;
  std::string vertPath = shadersDir + "/" + stem + ".vert.spv";
  std::string fragPath = shadersDir + "/" + stem + ".frag.spv";

  VulkanShader vertShader;
  if (!vertShader.init(device_, ShaderStage::Vertex, vertPath)) {
    s_logger.error("Failed to load vertex shader for pipeline");
    return nullptr;
  }

  VulkanShader fragShader;
  if (!fragShader.init(device_, ShaderStage::Fragment, fragPath)) {
    s_logger.error("Failed to load fragment shader for pipeline");
    vertShader.shutdown();
    return nullptr;
  }

  // convert PipelineDesc vertex layout -> VulkanVertexInputDescription
  VertexInputDescription vertexDesc;
  VkVertexInputBindingDescription binding{
      .binding = 0,
      .stride = desc.vertexLayout.stride,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  vertexDesc.bindings.push_back(binding);

  for (const auto &attr : desc.vertexLayout.attributes) {
    VkVertexInputAttributeDescription vkAttr{
        .location = attr.location,
        .binding = 0,
        .format = toVkFormat(attr.format),
        .offset = attr.offset,
    };
    vertexDesc.attributes.push_back(vkAttr);
  }

  std::array<VkDescriptorSetLayout, 3> setLayouts{
      descriptorPool_.uboSetLayout(),     // set 0
      descriptorPool_.samplerSetLayout(), // set 1
      descriptorPool_.textureSetLayout(), // set 2
  };

  auto impl = std::make_unique<VulkanPipelineImpl>();
  VkCompareOp depthOp = (desc.depthCompareOp == CompareOp::LessOrEqual)
                            ? VK_COMPARE_OP_LESS_OR_EQUAL
                            : VK_COMPARE_OP_LESS;

  BlendConfig blendConfig{};
  if (desc.blendEnable) {
    blendConfig.blendEnable = true;
    blendConfig.srcColorBlendFactor = toVkBlendFactor(desc.blendSrcFactor);
    blendConfig.dstColorBlendFactor = toVkBlendFactor(desc.blendDstFactor);
    blendConfig.colorBlendOp = toVkBlendOp(desc.blendOp);
    blendConfig.srcAlphaBlendFactor = toVkBlendFactor(desc.blendSrcAlphaFactor);
    blendConfig.dstAlphaBlendFactor = toVkBlendFactor(desc.blendDstAlphaFactor);
    blendConfig.alphaBlendOp = toVkBlendOp(desc.blendAlphaOp);
  }

  if (!impl->init(device_, renderPass_.renderPass(), vertShader, fragShader,
                  vertexDesc, desc.depthTestEnable, desc.depthWriteEnable,
                  depthOp, toVkCullMode(desc.cullMode), sampleCount_,
                  setLayouts.data(), 3, blendConfig)) {
    s_logger.error("Failed to create pipeline");
    vertShader.shutdown();
    fragShader.shutdown();
    return nullptr;
  }

  vertShader.shutdown();
  fragShader.shutdown();

  s_logger.info("Pipeline created from PipelineDesc");
  return impl;
}

std::unique_ptr<ITexture> VulkanDevice::createTexture(const TextureDesc &desc) {
  auto impl = std::make_unique<VulkanTextureImpl>();
  if (!impl->init(device_, allocator_.handle(), graphicsQueue_, transferPool_,
                  desc)) {
    s_logger.error("Failed to create texture");
    return nullptr;
  }
  return impl;
}

std::unique_ptr<ISampler> VulkanDevice::createSampler(const SamplerDesc &desc) {
  auto sampler = std::make_unique<VulkanSampler>();
  if (!sampler->init(device_, desc)) {
    s_logger.error("Failed to create sampler");
    return nullptr;
  }
  return sampler;
}

void VulkanDevice::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(
        device_); // wait for in-flight work before destroying anything
  }

  frameContext_.shutdown();

  for (auto &frame : frameSecondaries_) {
    frame.cbs.clear();
  }

  for (auto *pool : workerPools_) {
    if (pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, pool, nullptr);
    }
  }
  workerPools_.clear();

  if (transferPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, transferPool_, nullptr);
    transferPool_ = VK_NULL_HANDLE;
  }

  descriptorPool_.shutdown();

  destroyFramebuffers();
  pipelineOwnership_.clear();

  for (auto &img : msaaColorImages_) {
    img.shutdown();
  }

  msaaColorImages_.clear();
  depthImage_.shutdown();

  for (auto &[uniformBuffer, uniformSet] : perFrame_) {
    uniformBuffer.shutdown();
  }

  if (queryPool_ != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device_, queryPool_, nullptr);
    queryPool_ = VK_NULL_HANDLE;
  }

  allocator_.shutdown();
  renderPass_.shutdown();

  swapchain_.shutdown();

  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }

  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  if (debugMessenger_ != VK_NULL_HANDLE) {
    destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    debugMessenger_ = VK_NULL_HANDLE;
  }

  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void * /*userData*/) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    s_logger.error("{}", data->pMessage);
  } else {
    s_logger.warn("{}", data->pMessage);
  }
  return VK_FALSE;
}

bool VulkanDevice::checkValidationSupport() {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : validationLayers_) {
    bool found = false;
    for (const auto &layerProps : availableLayers) {
      if (std::strcmp(layerName, layerProps.layerName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

VkResult VulkanDevice::createDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *messenger) {
  return vkCreateDebugUtilsMessengerEXT(instance, createInfo, allocator,
                                        messenger);
}

void VulkanDevice::destroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks *allocator) {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

  if (func != nullptr) {
    func(instance, messenger, allocator);
  }
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device,
                                                   VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = 0U;

    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport != 0U) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }
  }

  return indices;
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device,
                                    VkSurfaceKHR surface) {
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);

  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  auto indices = findQueueFamilies(device, surface);
  bool extensionsSupported = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    auto swapChainSupport = querySwapChainSupport(device, surface);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate &&
         (deviceFeatures.samplerAnisotropy != 0U);
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount = 0;

  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions_.begin(),
                                           deviceExtensions_.end());
  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

SwapChainSupport VulkanDevice::querySwapChainSupport(VkPhysicalDevice device,
                                                     VkSurfaceKHR surface) {
  SwapChainSupport support;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &support.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  if (formatCount > 0) {
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         support.formats.data());
  }

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);
  if (presentModeCount > 0) {
    support.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, support.presentModes.data());
  }

  return support;
}

bool VulkanDevice::drawFrame(const RenderCallback &callback) {
  uint32_t imageIndex = 0;
  if (!frameContext_.beginFrame(swapchain_, imageIndex)) {
    return recreateSwapchain();
  }

  VkCommandBuffer cmd = frameContext_.currentCommandBuffer();

  uint32_t idx = frameIndex_ % kMaxFrames;

  // read previous frame's GPU timestamps
  if (timestampReady_[idx]) {
    std::array<uint64_t, 2> ts;

    if (vkGetQueryPoolResults(device_, queryPool_, idx * 2, 2, sizeof(ts),
                              ts.data(), sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
      gpuTimeMs_ = static_cast<float>(ts[1] - ts[0]) * timestampPeriod_ * 1e-6F;
    }
  }

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
    s_logger.error("Failed to begin command buffer");
    return false;
  }

  vkCmdResetQueryPool(cmd, queryPool_, idx * 2, 2);
  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_,
                      idx * 2);

  std::array<VkClearValue, 2> clearValues{};

  clearValues[0].color.float32[0] = 0.05F;
  clearValues[0].color.float32[1] = 0.05F;
  clearValues[0].color.float32[2] = 0.2F;
  clearValues[0].color.float32[3] = 1.0F;
  clearValues[1].depthStencil.depth = 1.0F;

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass_.renderPass(),
      .framebuffer = framebuffers_[imageIndex],
      .renderArea = {{0, 0}, swapchain_.extent()},
      .clearValueCount = 2,
      .pClearValues = clearValues.data(),
  };

  VkViewport viewport{
      .x = 0.0F,
      .y = 0.0F,
      .width = static_cast<float>(swapchain_.extent().width),
      .height = static_cast<float>(swapchain_.extent().height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = swapchain_.extent(),
  };

  bool useMT = (jobSystem_ != nullptr) && (jobSystem_->workerCount() > 1) &&
               (lastDrawCalls_ >= kMinDrawCallsForMT);

  if (!useMT) {
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
  } else {
    vkCmdBeginRenderPass(cmd, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }

  // delta time
  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
  lastFrameTime_ = now;
  totalTime_ += dt;

  auto &perFrame = perFrame_[idx];

  float aspect = static_cast<float>(swapchain_.extent().width) /
                 static_cast<float>(swapchain_.extent().height);

  // default camera, overridden by Camera component if world is set
  FrameUniforms uniforms{};
  uniforms.model = glm::mat4(1.0F);
  uniforms.view = glm::lookAt(glm::vec3(0.0F, 0.0F, 3.0F), glm::vec3(0.0F),
                              glm::vec3(0.0F, 1.0F, 0.0F));
  uniforms.projection =
      glm::perspective(glm::radians(45.0F), aspect, 0.1F, 100.0F);
  uniforms.projection[1][1] *= -1.0F;
  // convert from OpenGL [-1,1] to Vulkan [0,1] depth range
  uniforms.projection[2][2] =
      (0.5F * uniforms.projection[2][2]) + (0.5F * uniforms.projection[2][3]);
  uniforms.projection[3][2] =
      (0.5F * uniforms.projection[3][2]) + (0.5F * uniforms.projection[3][3]);
  uniforms.extra = {totalTime_, dt,
                    static_cast<float>(swapchain_.extent().width),
                    static_cast<float>(swapchain_.extent().height)};

  // read camera from ECS world if available
  if (world_ != nullptr) {
    world_->view<Camera>().each([&](Entity, Camera &cam) {
      if (cam.active) {
        uniforms.view = cam.view;
        uniforms.projection = glm::perspective(glm::radians(cam.fov), aspect,
                                               cam.zNear, cam.zFar);
        uniforms.projection[1][1] *= -1.0F;

        uniforms.projection[2][2] =
            (0.5F * uniforms.projection[2][2]) + (0.5F * uniforms.projection[2][3]);

        uniforms.projection[3][2] =
            (0.5F * uniforms.projection[3][2]) + (0.5F * uniforms.projection[3][3]);
      }
    });
  }

  // extract camera position from the view matrix
  glm::mat4 invView = glm::inverse(uniforms.view);
  auto camPos = glm::vec3(invView[3]);

  uniforms.cameraPos = glm::vec4(camPos, 0.0F);
  uniforms.lightDir =
      glm::vec4(glm::normalize(glm::vec3(1.0F, 2.0F, 1.0F)), 2.0F);
  uniforms.lightColor = glm::vec4(1.0F, 1.0F, 1.0F, 0.0F);
  uniforms.ambientSky = glm::vec4(0.1F, 0.15F, 0.3F, 1.0F);
  uniforms.ambientGround = glm::vec4(0.02F, 0.01F, 0.01F, 0.0F);

  perFrame.uniformBuffer.upload(&uniforms, sizeof(uniforms));

  if (useMT) {
    uint32_t frameCtx = frameContext_.currentFrame();
    uint32_t numWorkers = jobSystem_->workerCount();

    // ensure per-worker pools exist
    if (workerPools_.size() < numWorkers) {
      workerPools_.resize(numWorkers, VK_NULL_HANDLE);
    }

    for (uint32_t i = 0; i < numWorkers; ++i) {
      if (workerPools_[i] == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphicsQueueIndex_,
        };

        vkCreateCommandPool(device_, &poolInfo, nullptr, &workerPools_[i]);
      }
    }

    auto &secondaries = frameSecondaries_[frameCtx];
    if (secondaries.cbs.size() < numWorkers) {
      secondaries.cbs.resize(numWorkers, VK_NULL_HANDLE);
    }

    // ensure/reset secondary CBs from per-worker pools
    for (uint32_t i = 0; i < numWorkers; ++i) {
      if (secondaries.cbs[i] == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = workerPools_[i],
            .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
            .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(device_, &allocInfo, &secondaries.cbs[i]);
      } else {
        vkResetCommandBuffer(secondaries.cbs[i], 0);
      }
    }

    // inheritance info for secondary CBs
    VkCommandBufferInheritanceInfo inheritance{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .renderPass = renderPass_.renderPass(),
        .subpass = 0,
        .framebuffer = framebuffers_[imageIndex],
    };

    std::atomic<uint32_t> readyCount{0};
    std::array<std::array<uint32_t, 2>, 32>
        workerStats{}; // [worker][0]=drawCalls, [1]=triangles
    uint32_t cappedWorkers = std::min(numWorkers, 32U);

    // phase 1: begin all secondary CBs on the main thread
    for (uint32_t i = 0; i < cappedWorkers; ++i) {
      VkCommandBuffer secCmd = secondaries.cbs[i];

      VkCommandBufferBeginInfo secBegin{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
          .pInheritanceInfo = &inheritance,
      };
      vkBeginCommandBuffer(secCmd, &secBegin);
    }

    // phase 2: submit all jobs (workers record into their own pool's CB)
    for (uint32_t i = 0; i < cappedWorkers; ++i) {
      VkCommandBuffer secCmd = secondaries.cbs[i];

      JobDesc desc;
      desc.task = [&callback, secCmd, &pool = descriptorPool_,
                   uboSet = perFrame.uniformSet, &readyCount,
                   &stats = workerStats[i], i, cappedWorkers, viewport,
                   scissor]() {
        VulkanCommandBuffer vkCmdBuf(secCmd, pool, uboSet);

        vkCmdBuf.setViewport(static_cast<int>(viewport.x),
                             static_cast<int>(viewport.y),
                             static_cast<int>(viewport.width),
                             static_cast<int>(viewport.height));

        vkCmdBuf.setScissor(scissor.offset.x, scissor.offset.y,
                            static_cast<int>(scissor.extent.width),
                            static_cast<int>(scissor.extent.height));

        callback(vkCmdBuf, i, cappedWorkers);
        vkEndCommandBuffer(secCmd);
        stats[0] = vkCmdBuf.drawCalls();
        stats[1] = vkCmdBuf.triangles();
        readyCount.fetch_add(1, std::memory_order_release);
      };
      jobSystem_->submit(std::move(desc));
    }

    // wait for all workers, assisting other jobs
    while (readyCount.load(std::memory_order_acquire) < cappedWorkers) {
      jobSystem_->assistOnce();
    }

    // execute secondaries from primary
    vkCmdExecuteCommands(cmd, cappedWorkers, secondaries.cbs.data());

    // aggregate draw statistics
    lastDrawCalls_ = 0;
    lastTriangles_ = 0;
    for (uint32_t i = 0; i < cappedWorkers; ++i) {
      lastDrawCalls_ += workerStats[i][0];
      lastTriangles_ += workerStats[i][1];
    }
  } else {
    VulkanCommandBuffer vkCmdBuf(cmd, descriptorPool_, perFrame.uniformSet);
    callback(vkCmdBuf, 0, 1);
    lastDrawCalls_ = vkCmdBuf.drawCalls();
    lastTriangles_ = vkCmdBuf.triangles();
  }

  vkCmdEndRenderPass(cmd);

  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_,
                      (idx * 2) + 1);

  timestampReady_[idx] = true;

  frameIndex_++;

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    s_logger.error("Failed to end command buffer");
    return false;
  }

  if (!frameContext_.endFrame(swapchain_, graphicsQueue_, presentQueue_,
                              imageIndex)) {
    return recreateSwapchain();
  }

  return true;
}

std::shared_ptr<IMaterial> VulkanDevice::createMaterial(
    const MaterialDesc &desc, std::shared_ptr<ITexture> albedo,
    std::shared_ptr<ITexture> normal,
    std::shared_ptr<ITexture> metallicRoughness,
    std::shared_ptr<ITexture> emissive, std::shared_ptr<ITexture> occlusion) {

  // resolve shader path
  std::string shaderPath = desc.shader == "builtin://pbr"
                               ? "shaders/pbr.slang"
                               : std::string(desc.shader);

  namespace fs = std::filesystem;
  fs::path slangPath(shaderPath);
  std::string stem = slangPath.stem().string();

  std::string shadersDir = SHADERS_DIR;
  std::string vertPath = shadersDir + "/" + stem + ".vert.spv";
  std::string fragPath = shadersDir + "/" + stem + ".frag.spv";

  VulkanShader vertShader;
  if (!vertShader.init(device_, ShaderStage::Vertex, vertPath)) {
    s_logger.error("createMaterial: failed to load vertex shader");
    return nullptr;
  }

  VulkanShader fragShader;
  if (!fragShader.init(device_, ShaderStage::Fragment, fragPath)) {
    s_logger.error("createMaterial: failed to load fragment shader");
    vertShader.shutdown();
    return nullptr;
  }

  VertexInputDescription vertexDesc;

  vertexDesc.bindings.push_back({
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  });

  vertexDesc.attributes = {
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, pos)},
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, normal)},
      {.location = 2,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, color)},
      {.location = 3,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = offsetof(Vertex, uv)},
  };

  std::array<VkDescriptorSetLayout, 4> setLayouts{
      descriptorPool_.uboSetLayout(),            // set 0
      descriptorPool_.samplerSetLayout(),        // set 1
      descriptorPool_.materialSetLayout(),       // set 2
      descriptorPool_.materialParamsSetLayout(), // set 3
  };

  auto pipeline = std::make_unique<VulkanPipelineImpl>();
  if (!pipeline->init(device_, renderPass_.renderPass(), vertShader, fragShader,
                      vertexDesc, desc.depthTest, true, VK_COMPARE_OP_LESS,
                      VK_CULL_MODE_BACK_BIT, sampleCount_, setLayouts.data(),
                      4)) {
    s_logger.error("createMaterial: failed to create pipeline");

    vertShader.shutdown();
    fragShader.shutdown();

    return nullptr;
  }

  vertShader.shutdown();
  fragShader.shutdown();

  auto mat = std::make_shared<VulkanMaterial>();
  if (!mat->init(device_, allocator_.handle(), descriptorPool_, pipeline.get(),
                 desc, std::move(albedo), std::move(normal),
                 std::move(metallicRoughness), std::move(emissive),
                 std::move(occlusion))) {
    s_logger.error("createMaterial: failed to initialize material");
    return nullptr;
  }

  // keep pipeline alive as long as the material is alive
  // store in a device-level cache keyed by the raw pointer
  pipelineOwnership_.push_back(std::move(pipeline));

  s_logger.info("Material created.");
  return mat;
}

bool VulkanDevice::createFramebuffers() {
  bool msaa = sampleCount_ != VK_SAMPLE_COUNT_1_BIT;

  // create MSAA color images
  if (msaa) {
    msaaColorImages_.resize(swapchain_.imageViews().size());

    for (auto &msaaColorImage : msaaColorImages_) {
      if (!msaaColorImage.init(
              device_, allocator_.handle(), swapchain_.extent().width,
              swapchain_.extent().height, swapchain_.imageFormat(),
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
              VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VK_IMAGE_ASPECT_COLOR_BIT,
              sampleCount_)) {
        s_logger.error("Failed to create MSAA color image");
        return false;
      }
    }
  }

  framebuffers_.resize(swapchain_.imageViews().size());

  for (size_t i = 0; i < framebuffers_.size(); ++i) {
    std::array<VkImageView, 3> attachments{};
    uint32_t attachmentCount = 1;

    if (msaa) {
      attachments[0] = msaaColorImages_[i].view(); // MSAA color
      attachments[1] = depthImage_.view();         // MSAA depth
      attachments[2] = swapchain_.imageViews()[i]; // resolve target
      attachmentCount = depthFormat_ != VK_FORMAT_UNDEFINED ? 3U : 2U;
    } else {
      attachments[0] = swapchain_.imageViews()[i]; // color
      attachments[1] = depthImage_.view();         // depth
      attachmentCount = depthFormat_ != VK_FORMAT_UNDEFINED ? 2U : 1U;
    }

    VkFramebufferCreateInfo fbInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass_.renderPass(),
        .attachmentCount = attachmentCount,
        .pAttachments = attachments.data(),
        .width = swapchain_.extent().width,
        .height = swapchain_.extent().height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &framebuffers_[i]) !=
        VK_SUCCESS) {
      s_logger.error("Failed to create framebuffer");
      return false;
    }
  }

  s_logger.info("Framebuffers created (MSAA: {}).", msaa ? "yes" : "no");
  return true;
}

void VulkanDevice::destroyFramebuffers() {
  for (auto *fb : framebuffers_) {
    vkDestroyFramebuffer(device_, fb, nullptr);
  }

  framebuffers_.clear();

  for (auto &img : msaaColorImages_) {
    img.shutdown();
  }

  msaaColorImages_.clear();
}

bool VulkanDevice::recreateSwapchain() {
  int width = 0, height = 0;
  platform_->getWindowSize(width, height);

  // window minimized, wait until it has real size again before rebuilding.
  while (width == 0 || height == 0) {
    platform_->getWindowSize(width, height);
    if (!platform_->pollEvents()) {
      return false; // user closed the window while minimized
    }
  }

  vkDeviceWaitIdle(device_);

  destroyFramebuffers();
  depthImage_.shutdown();

  auto indices = findQueueFamilies(physicalDevice_, surface_);

  if (!swapchain_.recreate(
          physicalDevice_, surface_, indices.graphicsFamily.value(),
          indices.presentFamily.value(), static_cast<uint32_t>(width),
          static_cast<uint32_t>(height), config_.window.vsync)) {
    s_logger.error("Failed to recreate swapchain");
    return false;
  }

  if (!depthImage_.init(device_, allocator_.handle(), swapchain_.extent().width,
                        swapchain_.extent().height, depthFormat_,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                        VK_IMAGE_ASPECT_DEPTH_BIT, sampleCount_)) {
    s_logger.error("Failed to recreate depth image");
    return false;
  }

  if (!createFramebuffers()) {
    s_logger.error("Failed to recreate framebuffers");
    return false;
  }

  s_logger.info("Swapchain recreated.");
  return true;
}

std::unique_ptr<IBuffer> VulkanDevice::createBuffer(const BufferDesc &desc) {
  auto buffer = std::make_unique<VulkanBufferImpl>();
  if (!buffer->init(allocator_.handle(), desc)) {
    s_logger.error("Failed to create buffer");
    return nullptr;
  }
  return buffer;
}

VkFormat VulkanDevice::chooseDepthFormat() {
  return findDepthFormat(physicalDevice_);
}

} // namespace Raiden::Renderer
