#include <RaidenEngineCore/Application.hpp>
#include <RaidenEngineCore/Logger.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::Application");

Application::Application(std::unique_ptr<IPlatform> platform,
                         std::unique_ptr<IRenderDevice> device)
    : platform_(std::move(platform)), device_(std::move(device)) {}

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

void Application::shutdown() {
  if (running_) {
    s_logger.info("Shutting down application...");
    running_ = false;
    device_->shutdown();
    platform_->shutdown();
  }
}

void Application::run() {
  s_logger.info("Entering main loop.");
  while (running_) {
    if (!platform_->pollEvents()) {
      running_ = false;
    }
  }
  s_logger.info("Exited main loop.");
}

} // namespace Raiden::Core
