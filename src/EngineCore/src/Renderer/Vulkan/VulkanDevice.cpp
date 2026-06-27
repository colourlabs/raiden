#include <RaidenEngineCore/Platform/IPlatform.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDevice.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <cstring>
#include <set>

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
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "raiden";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "raiden engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  if (enableValidation_) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers_.size());
    createInfo.ppEnabledLayerNames = validationLayers_.data();

    debugCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr;

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
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = family;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
  deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions_.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions_.data();

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

  graphicsQueueIndex_ = indices.graphicsFamily.value();
  presentQueueIndex_ = indices.presentFamily.value();
  vkGetDeviceQueue(device_, graphicsQueueIndex_, 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, presentQueueIndex_, 0, &presentQueue_);

  int windowWidth = 0;
  int windowHeight = 0;
  platform->getWindowSize(windowWidth, windowHeight);

  if (!swapchain_.init(physicalDevice_, device_, surface_,
                        indices.graphicsFamily.value(),
                        indices.presentFamily.value(),
                        static_cast<uint32_t>(windowWidth),
                        static_cast<uint32_t>(windowHeight),
                        config.window.vsync)) {
    s_logger.critical("Failed to create swapchain");
    return false;
  }

  if (!renderPass_.init(device_, swapchain_.imageFormat())) {
    s_logger.critical("Failed to create render pass");
    return false;
  }

  if (!createFramebuffers()) {
    s_logger.critical("Failed to create framebuffers");
    return false;
  }

  if (!frameContext_.init(device_, graphicsQueueIndex_)) {
    s_logger.critical("Failed to create frame context");
    return false;
  }

  return true;
}

void VulkanDevice::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_); // wait for in-flight work before destroying anything
  }

  destroyFramebuffers();
  renderPass_.shutdown();
  frameContext_.shutdown();
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
  return vkCreateDebugUtilsMessengerEXT(instance, createInfo, allocator, messenger);
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

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
    s_logger.error("Failed to begin command buffer");
    return false;
  }

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_.renderPass();
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchain_.extent();

  VkClearValue clearColor{};
  clearColor.color.float32[0] = 0.05f;
  clearColor.color.float32[1] = 0.05f;
  clearColor.color.float32[2] = 0.2f;
  clearColor.color.float32[3] = 1.0f;
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // TODO: bind pipeline and draw here

  vkCmdEndRenderPass(cmd);

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    s_logger.error("Failed to end command buffer");
    return false;
  }

  if (!frameContext_.endFrame(swapchain_, graphicsQueue_, presentQueue_, imageIndex)) {
    return recreateSwapchain();
  }

  return true;
}

bool VulkanDevice::createFramebuffers() {
  framebuffers_.resize(swapchain_.imageViews().size());

  for (size_t i = 0; i < framebuffers_.size(); ++i) {
    VkImageView attachments[] = {swapchain_.imageViews()[i]};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderPass_.renderPass();
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = attachments;
    fbInfo.width = swapchain_.extent().width;
    fbInfo.height = swapchain_.extent().height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
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

  auto indices = findQueueFamilies(physicalDevice_, surface_);

  if (!swapchain_.recreate(physicalDevice_, surface_,
                           indices.graphicsFamily.value(),
                           indices.presentFamily.value(),
                           static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height),
                           config_.window.vsync)) {
    s_logger.error("Failed to recreate swapchain");
    return false;
  }

  if (!createFramebuffers()) {
    s_logger.error("Failed to recreate framebuffers");
    return false;
  }

  s_logger.info("Swapchain recreated.");
  return true;
}

} // namespace Raiden::Core
