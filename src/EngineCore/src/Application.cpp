#include <RaidenEngineCore/Application.hpp>
#include <RaidenEngineCore/Assets/AssetManager.hpp>
#include <RaidenEngineCore/Audio/OpenALDevice.hpp>
#include <RaidenEngineCore/Engine/VulkanImGuiBackend.hpp>
#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/IVulkanRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDevice.hpp>

#include <functional>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::Application");

Application::Application(std::unique_ptr<IPlatform> platform,
                         std::unique_ptr<IRenderDevice> device,
                         std::unique_ptr<IVirtualFileSystem> vfs)
    : platform_(std::move(platform)), device_(std::move(device)),
      vfs_(std::move(vfs)) {
  lastFrameTime_ = std::chrono::steady_clock::now();
}

Application::~Application() { shutdown(); }

bool Application::init(const EngineConfig &config) {
  config_ = config;

  s_logger.info("Initializing application...");

  jobSystem_.init();

  if (!platform_->init(config_.window, config_.renderBackend)) {
    s_logger.error("Failed to initialize platform.");
    return false;
  }

  if (!device_->init(config_, platform_.get())) {
    s_logger.error("Failed to initialize render device.");
    return false;
  }

  device_->setJobSystem(jobSystem_);

  assetManager_ = std::make_unique<AssetManager>(*device_, *vfs_);
  assetManager_->setJobSystem(jobSystem_);

  audioDevice_ = std::make_unique<OpenALDevice>();
  auto *audioDev = static_cast<OpenALDevice *>(audioDevice_.get());
  audioDev->setJobSystem(jobSystem_);
  if (!audioDevice_->init(config_.audio, *vfs_)) {
    s_logger.warn("Failed to initialize audio device.");
    audioDevice_.reset();
  }

  // init ImGui overlay
  if (auto *vkDevice = dynamic_cast<IVulkanRenderDevice *>(device_.get())) {
    auto backend = std::make_unique<VulkanImGuiBackend>(
        *vkDevice, vkDevice->getRenderPass(),
        vkDevice->getSwapchainImageCount());
    overlay_ = std::make_unique<ImGuiOverlay>();
    overlay_->init(std::move(backend));
  }

  s_logger.info("Application initialized successfully.");
  running_ = true;
  return true;
}

bool Application::loadGamePlugin(std::string_view path) {
  if (!pluginLoader_.load(path)) {
    s_logger.error("Failed to load game plugin.");
    return false;
  }

  if (!pluginLoader_.plugin().init(*device_, *vfs_, *assetManager_,
                                    platform_.get(), audioDevice_.get())) {
    s_logger.error("Game plugin init failed.");
    pluginLoader_.unload();
    return false;
  }

  auto *vkDev = dynamic_cast<IVulkanRenderDevice *>(device_.get());
  if (vkDev) {
    if (auto *world = pluginLoader_.plugin().getWorld())
      vkDev->setWorld(world);
  }

  s_logger.info("Game plugin '{}' initialized.", pluginLoader_.plugin().name());
  return true;
}

void Application::shutdown() {
  if (!running_)
    return;
  running_ = false;

  s_logger.info("Shutting down application...");

  overlay_.reset();
  pluginLoader_.unload();
  assetManager_.reset();
  audioDevice_.reset();
  jobSystem_.shutdown();
  device_->shutdown();
  platform_->shutdown();
}

void Application::run() {
  s_logger.info("Entering main loop.");

  if (!pluginLoader_.isLoaded()) {
    s_logger.warn("No game plugin loaded, running with default scene.");
  }

  while (running_) {
    auto now = std::chrono::steady_clock::now();
    float deltaTime =
        std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    if (!platform_->pollEvents()) {
      break;
    }

    if (pluginLoader_.isLoaded()) {
      pluginLoader_.plugin().update(deltaTime, platform_->getInputState());
    }

    if (audioDevice_) {
      audioDevice_->processPendingLoads();
    }

    assetManager_->processLoadQueue();

    if (overlay_) {
      int w, h;
      platform_->getWindowSize(w, h);

      ProfilerFrameData profiler{};
      profiler.cpuFrameTimeMs = deltaTime * 1000.0f;

      if (auto *vkDev = dynamic_cast<IVulkanRenderDevice *>(device_.get())) {
        auto &d = dynamic_cast<const VulkanDevice &>(*vkDev);
        profiler.gpuFrameTimeMs = d.gpuTimeMs();
        profiler.drawCalls = d.lastDrawCalls();
        profiler.triangles = d.lastTriangles();
      }

      std::function<void()> pluginUI;
      if (pluginLoader_.isLoaded()) {
        pluginUI = [&]() { pluginLoader_.plugin().onDebugUI(); };
      }

      overlay_->newFrame(platform_->getInputState(), w, h, deltaTime, profiler,
                         pluginUI);
      overlay_->endFrame();
    }

    device_->drawFrame([&](ICommandBuffer &cmd, uint32_t wi, uint32_t n) {
      // worker 0 handles game rendering
      if (wi == 0 && pluginLoader_.isLoaded()) {
        pluginLoader_.plugin().render(cmd);
      }

      // worker n-1 handles overlay (if any workers remain)
      if (n > 1 && wi == n - 1 && overlay_) {
        overlay_->renderDrawData(cmd);
      }

      // single-threaded fallback: everything on worker 0
      if (n == 1 && wi == 0 && overlay_) {
        overlay_->renderDrawData(cmd);
      }
    });

    // end the ImGui frame even if drawFrame skipped the callback (e.g. resize)
    if (overlay_) {
      overlay_->endFrame();
    }

    platform_->endInputFrame();
  }

  s_logger.info("Exited main loop.");
}

} // namespace Raiden::Core
