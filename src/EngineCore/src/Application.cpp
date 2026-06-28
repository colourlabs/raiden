#include <RaidenEngineCore/Application.hpp>
#include <RaidenEngineCore/Logger.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::Application");

Application::Application(std::unique_ptr<IPlatform> platform,
                         std::unique_ptr<IRenderDevice> device,
                         std::unique_ptr<IVirtualFileSystem> vfs)
    : platform_(std::move(platform)),
      device_(std::move(device)),
      vfs_(std::move(vfs)) {
  lastFrameTime_ = std::chrono::steady_clock::now();
}

Application::~Application() { shutdown(); }

bool Application::init(const EngineConfig &config) {
  config_ = config;

  s_logger.info("Initializing application...");

  if (!platform_->init(config_.window, config_.renderBackend)) {
    s_logger.error("Failed to initialize platform.");
    return false;
  }

  if (!device_->init(config_, platform_.get())) {
    s_logger.error("Failed to initialize render device.");
    return false;
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

  if (!pluginLoader_.plugin().init(*device_, *vfs_)) {
    s_logger.error("Game plugin init failed.");
    pluginLoader_.unload();
    return false;
  }

  s_logger.info("Game plugin '{}' initialized.", pluginLoader_.plugin().name());
  return true;
}

void Application::shutdown() {
  if (running_) {
    s_logger.info("Shutting down application...");
    running_ = false;
    pluginLoader_.unload();
    device_->shutdown();
    platform_->shutdown();
  }
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
      running_ = false;
    }

    if (pluginLoader_.isLoaded()) {
      pluginLoader_.plugin().update(deltaTime, platform_->getInputState());
    }

    device_->drawFrame([&](ICommandBuffer &cmd) {
      if (pluginLoader_.isLoaded()) {
        pluginLoader_.plugin().render(cmd);
      }
    });
  }
  
  s_logger.info("Exited main loop.");
}

} // namespace Raiden::Core
