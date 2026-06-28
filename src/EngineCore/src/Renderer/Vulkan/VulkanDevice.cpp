#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Platform/IPlatform.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDevice.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanBufferImpl.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::VulkanDevice");

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

  // proudly stolen
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
  float queuePriority = 1.0f;
  for (uint32_t family : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{
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

  if (enableValidation_) {
    deviceCreateInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers_.size());
    deviceCreateInfo.ppEnabledLayerNames = validationLayers_.data();
  }

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

  VkQueryPoolCreateInfo qpInfo{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = kMaxFrames * 2,
  };
  vkCreateQueryPool(device_, &qpInfo, nullptr, &queryPool_);

  int windowWidth = 0;
  int windowHeight = 0;
  platform->getWindowSize(windowWidth, windowHeight);

  if (!swapchain_.init(
          physicalDevice_, device_, surface_, indices.graphicsFamily.value(),
          indices.presentFamily.value(), static_cast<uint32_t>(windowWidth),
          static_cast<uint32_t>(windowHeight), config.window.vsync)) {
    s_logger.critical("Failed to create swapchain");
    return false;
  }

  depthFormat_ = chooseDepthFormat();

  if (!renderPass_.init(device_, swapchain_.imageFormat(), depthFormat_)) {
    s_logger.critical("Failed to create render pass");
    return false;
  }

  if (!depthImage_.init(device_, allocator_.handle(), swapchain_.extent().width,
                        swapchain_.extent().height, depthFormat_,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                        VK_IMAGE_ASPECT_DEPTH_BIT)) {
    s_logger.critical("Failed to create depth image");
    return false;
  }

  if (!createFramebuffers()) {
    s_logger.critical("Failed to create framebuffers");
    return false;
  }

  descriptorPool_.init(device_);

  // per-frame uniform buffers and descriptor sets
  for (auto &[uniformBuffer, uniformSet] : perFrame_) {
    uniformBuffer.init(
        allocator_.handle(), sizeof(FrameUniforms),
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

  std::string shadersDir = SHADERS_DIR;
  if (!vertexShader_.init(device_, ShaderStage::Vertex,
                          shadersDir + "/triangle.vert.spv")) {
    s_logger.critical("Failed to load vertex shader");
    return false;
  }
  if (!fragmentShader_.init(device_, ShaderStage::Fragment,
                            shadersDir + "/triangle.frag.spv")) {
    s_logger.critical("Failed to load fragment shader");
    return false;
  }

  VertexInputDescription vertexDesc;
  VkVertexInputBindingDescription binding{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  vertexDesc.bindings.push_back(binding);

  VkVertexInputAttributeDescription posAttr{
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(Vertex, pos),
  };
  vertexDesc.attributes.push_back(posAttr);

  VkVertexInputAttributeDescription colorAttr{
      .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(Vertex, color),
  };
  vertexDesc.attributes.push_back(colorAttr);

  if (!pipeline_.init(device_, renderPass_.renderPass(), swapchain_.extent(),
                      vertexShader_, fragmentShader_, vertexDesc)) {
    s_logger.critical("Failed to create graphics pipeline");
    return false;
  }

  std::array<Vertex, 3> vertices = {{
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
  }};

  if (!vertexBuffer_.init(
          allocator_.handle(), sizeof(vertices),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)) {
    s_logger.critical("Failed to create vertex buffer");
    return false;
  }

  if (!vertexBuffer_.map()) {
    s_logger.critical("Failed to map vertex buffer");
    return false;
  }

  vertexBuffer_.upload(vertices.data(), sizeof(vertices));
  vertexBuffer_.unmap();

  std::array<uint16_t, 3> indexData = {{0, 1, 2}};

  if (!indexBuffer_.init(
          allocator_.handle(), sizeof(indexData),
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)) {
    s_logger.critical("Failed to create index buffer");
    return false;
  }

  if (!indexBuffer_.map()) {
    s_logger.critical("Failed to map index buffer");
    return false;
  }
  indexBuffer_.upload(indexData.data(), sizeof(indexData));
  indexBuffer_.unmap();

  if (!frameContext_.init(device_, graphicsQueueIndex_)) {
    s_logger.critical("Failed to create frame context");
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

std::unique_ptr<IPipeline> VulkanDevice::createPipeline(
    const PipelineDesc &desc) {
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

  VkDescriptorSetLayout setLayouts[] = {
      descriptorPool_.uboSetLayout(),
      descriptorPool_.samplerSetLayout(),
  };

  auto impl = std::make_unique<VulkanPipelineImpl>();
  if (!impl->init(device_, renderPass_.renderPass(), vertShader, fragShader,
                  vertexDesc, desc.depthTestEnable, setLayouts, 2)) {
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

std::unique_ptr<ITexture> VulkanDevice::createTexture(
    const TextureDesc &desc) {
  auto impl = std::make_unique<VulkanTextureImpl>();
  if (!impl->init(device_, allocator_.handle(), graphicsQueue_, transferPool_,
                  desc)) {
    s_logger.error("Failed to create texture");
    return nullptr;
  }
  return impl;
}

void VulkanDevice::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(
        device_); // wait for in-flight work before destroying anything
  }

  destroyFramebuffers();
  pipeline_.shutdown();
  depthImage_.shutdown();
  indexBuffer_.shutdown();
  vertexBuffer_.shutdown();

  for (auto &[uniformBuffer, uniformSet] : perFrame_) {
    uniformBuffer.shutdown();
  }

  descriptorPool_.shutdown();

  if (queryPool_ != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device_, queryPool_, nullptr);
    queryPool_ = VK_NULL_HANDLE;
  }

  allocator_.shutdown();
  vertexShader_.shutdown();
  fragmentShader_.shutdown();
  renderPass_.shutdown();
  frameContext_.shutdown();

  if (transferPool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, transferPool_, nullptr);
    transferPool_ = VK_NULL_HANDLE;
  }

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
  uint32_t layerCount;
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
  if (func) {
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
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport) {
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
         deviceFeatures.samplerAnisotropy;
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
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

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  if (formatCount > 0) {
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         support.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);
  if (presentModeCount > 0) {
    support.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, support.presentModes.data());
  }

  return support;
}

bool VulkanDevice::drawFrame() {
  uint32_t imageIndex = 0;
  if (!frameContext_.beginFrame(swapchain_, imageIndex)) {
    return recreateSwapchain();
  }

  VkCommandBuffer cmd = frameContext_.currentCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
    s_logger.error("Failed to begin command buffer");
    return false;
  }

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color.float32[0] = 0.05f;
  clearValues[0].color.float32[1] = 0.05f;
  clearValues[0].color.float32[2] = 0.2f;
  clearValues[0].color.float32[3] = 1.0f;
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass_.renderPass(),
      .framebuffer = framebuffers_[imageIndex],
      .renderArea = {{0, 0}, swapchain_.extent()},
      .clearValueCount = 2,
      .pClearValues = clearValues.data(),
  };

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline());

  VkBuffer vertexBuffer = vertexBuffer_.buffer();
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
  vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer(), 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);

  vkCmdEndRenderPass(cmd);

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

bool VulkanDevice::drawFrame(const RenderCallback &callback) {
  uint32_t imageIndex = 0;
  if (!frameContext_.beginFrame(swapchain_, imageIndex)) {
    return recreateSwapchain();
  }

  VkCommandBuffer cmd = frameContext_.currentCommandBuffer();

  static uint32_t frameIndex = 0;
  uint32_t idx = frameIndex % kMaxFrames;

  // read previous frame's GPU timestamps
  uint64_t ts[2];
  if (vkGetQueryPoolResults(device_, queryPool_, idx * 2, 2, sizeof(ts), ts,
                            sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
    gpuTimeMs_ = static_cast<float>(ts[1] - ts[0]) * timestampPeriod_ * 1e-6f;
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
  clearValues[0].color.float32[0] = 0.05f;
  clearValues[0].color.float32[1] = 0.05f;
  clearValues[0].color.float32[2] = 0.2f;
  clearValues[0].color.float32[3] = 1.0f;
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass_.renderPass(),
      .framebuffer = framebuffers_[imageIndex],
      .renderArea = {{0, 0}, swapchain_.extent()},
      .clearValueCount = 2,
      .pClearValues = clearValues.data(),
  };

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // dynamic viewport + scissor (required for pipelines from createPipeline)
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchain_.extent().width);
  viewport.height = static_cast<float>(swapchain_.extent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = swapchain_.extent(),
  };
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // update frame uniforms
  static auto lastFrameTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - lastFrameTime).count();
  lastFrameTime = now;
  totalTime_ += dt;

  auto &perFrame = perFrame_[idx];

  float aspect = static_cast<float>(swapchain_.extent().width) /
                 static_cast<float>(swapchain_.extent().height);
  FrameUniforms uniforms{
      .projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f),
      .view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f),
                          glm::vec3(0.0f, 1.0f, 0.0f)),
      .extra = {totalTime_, dt,
                static_cast<float>(swapchain_.extent().width),
                static_cast<float>(swapchain_.extent().height)},
  };

  perFrame.uniformBuffer.upload(&uniforms, sizeof(uniforms));

  VulkanCommandBuffer vkCmdBuf(cmd, descriptorPool_, perFrame.uniformSet);
  callback(vkCmdBuf);

  vkCmdEndRenderPass(cmd);

  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_,
                      idx * 2 + 1);
  lastDrawCalls_ = vkCmdBuf.drawCalls();
  lastTriangles_ = vkCmdBuf.triangles();

  frameIndex++;

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

bool VulkanDevice::createFramebuffers() {
  framebuffers_.resize(swapchain_.imageViews().size());

  for (size_t i = 0; i < framebuffers_.size(); ++i) {
    std::array<VkImageView, 2> attachments = {swapchain_.imageViews()[i],
                                              depthImage_.view()};

    VkFramebufferCreateInfo fbInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass_.renderPass(),
        .attachmentCount = renderPass_.hasDepth() ? 2u : 1u,
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

  s_logger.info("Framebuffers created.");
  return true;
}

void VulkanDevice::destroyFramebuffers() {
  for (auto fb : framebuffers_) {
    vkDestroyFramebuffer(device_, fb, nullptr);
  }
  framebuffers_.clear();
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
                        VK_IMAGE_ASPECT_DEPTH_BIT)) {
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

VkFormat VulkanDevice::chooseDepthFormat() {
  return findDepthFormat(physicalDevice_);
}

} // namespace Raiden::Core
